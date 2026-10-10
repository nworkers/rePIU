#include "repiu/update/release_archive.h"

#include <miniz.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <system_error>

namespace repiu::update
{
namespace
{

struct ArchiveEntry
{
    std::string name;  // As stored, '/' or '\' separated.
    std::vector<std::uint8_t> data;
    bool executable = false;
};

bool EndsWith(std::string_view text, std::string_view suffix)
{
    return text.size() >= suffix.size() &&
        text.substr(text.size() - suffix.size()) == suffix;
}

// Splits an entry name into safe components, or fails. Empty components and
// `.` are dropped; anything that could leave the staging folder is refused.
bool SafeComponents(std::string_view name, std::vector<std::string>* parts)
{
    parts->clear();
    if (name.empty() || name.front() == '/' || name.front() == '\\' ||
        (name.size() >= 2U && name[1] == ':'))
    {
        return false;
    }
    std::string current;
    for (std::size_t index = 0; index <= name.size(); ++index)
    {
        const char character = index < name.size() ? name[index] : '/';
        if (character == '/' || character == '\\')
        {
            if (current == "..")
            {
                return false;
            }
            if (!current.empty() && current != ".")
            {
                parts->push_back(current);
            }
            current.clear();
            continue;
        }
        if (static_cast<unsigned char>(character) < 0x20U)
        {
            return false;
        }
        current.push_back(character);
    }
    return true;
}

std::uint64_t ParseOctal(const char* field, std::size_t size, bool* ok)
{
    std::uint64_t value = 0;
    std::size_t index = 0;
    while (index < size && (field[index] == ' ' || field[index] == '\0'))
    {
        ++index;
    }
    bool any = false;
    for (; index < size; ++index)
    {
        const char character = field[index];
        if (character == ' ' || character == '\0')
        {
            break;
        }
        if (character < '0' || character > '7')
        {
            *ok = false;
            return 0;
        }
        value = value * 8U + static_cast<std::uint64_t>(character - '0');
        any = true;
    }
    *ok = any;
    return value;
}

std::string FieldString(const char* field, std::size_t size)
{
    return std::string(field, strnlen(field, size));
}

// The `path` record of a pax extended header, if it has one.
std::string PaxPath(const std::vector<std::uint8_t>& data)
{
    std::size_t position = 0;
    while (position < data.size())
    {
        std::size_t space = position;
        while (space < data.size() && data[space] != ' ')
        {
            ++space;
        }
        std::size_t length = 0;
        for (std::size_t index = position; index < space; ++index)
        {
            if (data[index] < '0' || data[index] > '9')
            {
                return std::string();
            }
            length = length * 10U + static_cast<std::size_t>(data[index] - '0');
        }
        if (space + 2U > position + length)
        {
            break;
        }
        if (length == 0U || position + length > data.size())
        {
            break;
        }
        const std::string record(
            reinterpret_cast<const char*>(data.data()) + space + 1,
            position + length - space - 2);  // Drop the trailing newline.
        if (record.rfind("path=", 0) == 0)
        {
            return record.substr(5);
        }
        position += length;
    }
    return std::string();
}

bool InflateGzip(const std::vector<std::uint8_t>& archive,
                 std::vector<std::uint8_t>* out, std::string* error)
{
    // RFC 1952: ID1 ID2 CM FLG MTIME(4) XFL OS, then the optional fields FLG
    // names, the raw deflate stream, and CRC32 and ISIZE.
    if (archive.size() < 18U || archive[0] != 0x1FU || archive[1] != 0x8BU ||
        archive[2] != 8U)
    {
        *error = "not a gzip stream";
        return false;
    }
    const std::uint8_t flags = archive[3];
    std::size_t position = 10;
    const auto skip_zero_terminated = [&]() {
        while (position < archive.size() && archive[position] != 0U)
        {
            ++position;
        }
        ++position;
    };
    if ((flags & 0x04U) != 0U)
    {
        if (position + 2U > archive.size())
        {
            *error = "truncated gzip header";
            return false;
        }
        position += 2U + (static_cast<std::size_t>(archive[position]) |
                          (static_cast<std::size_t>(archive[position + 1U]) << 8));
    }
    if ((flags & 0x08U) != 0U)
    {
        skip_zero_terminated();
    }
    if ((flags & 0x10U) != 0U)
    {
        skip_zero_terminated();
    }
    if ((flags & 0x02U) != 0U)
    {
        position += 2U;
    }
    if (position + 8U > archive.size())
    {
        *error = "truncated gzip stream";
        return false;
    }
    const std::size_t deflate_size = archive.size() - position - 8U;
    std::size_t inflated_size = 0;
    void* inflated = tinfl_decompress_mem_to_heap(
        archive.data() + position, deflate_size, &inflated_size, 0);
    if (inflated == nullptr)
    {
        *error = "gzip data does not inflate";
        return false;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(inflated);
    out->assign(bytes, bytes + inflated_size);
    mz_free(inflated);
    const std::uint8_t* trailer = archive.data() + archive.size() - 8U;
    const std::uint32_t expected_crc = static_cast<std::uint32_t>(trailer[0]) |
        (static_cast<std::uint32_t>(trailer[1]) << 8) |
        (static_cast<std::uint32_t>(trailer[2]) << 16) |
        (static_cast<std::uint32_t>(trailer[3]) << 24);
    const std::uint32_t expected_size = static_cast<std::uint32_t>(trailer[4]) |
        (static_cast<std::uint32_t>(trailer[5]) << 8) |
        (static_cast<std::uint32_t>(trailer[6]) << 16) |
        (static_cast<std::uint32_t>(trailer[7]) << 24);
    const auto crc = static_cast<std::uint32_t>(
        mz_crc32(MZ_CRC32_INIT, out->data(), out->size()));
    if (crc != expected_crc ||
        static_cast<std::uint32_t>(out->size()) != expected_size)
    {
        *error = "gzip checksum mismatch";
        return false;
    }
    return true;
}

bool ReadTar(const std::vector<std::uint8_t>& tar,
             std::vector<ArchiveEntry>* entries, std::string* error)
{
    std::size_t position = 0;
    std::string pending_name;
    std::uint64_t total = 0;
    while (position + 512U <= tar.size())
    {
        const char* header = reinterpret_cast<const char*>(tar.data() + position);
        if (std::all_of(header, header + 512, [](char c) { return c == 0; }))
        {
            return true;  // The end-of-archive block.
        }
        bool size_ok = false;
        const std::uint64_t size = ParseOctal(header + 124, 12, &size_ok);
        bool mode_ok = false;
        const std::uint64_t mode = ParseOctal(header + 100, 8, &mode_ok);
        if (!size_ok || size > kMaxReleaseArchiveBytes)
        {
            *error = "bad tar entry size";
            return false;
        }
        const char type = header[156];
        const std::size_t data_start = position + 512U;
        const std::size_t padded = static_cast<std::size_t>((size + 511U) / 512U * 512U);
        if (data_start + size > tar.size())
        {
            *error = "truncated tar entry";
            return false;
        }
        const std::vector<std::uint8_t> data(
            tar.begin() + static_cast<std::ptrdiff_t>(data_start),
            tar.begin() + static_cast<std::ptrdiff_t>(data_start + size));
        position = data_start + padded;

        if (type == 'L')
        {
            pending_name.assign(reinterpret_cast<const char*>(data.data()),
                                strnlen(reinterpret_cast<const char*>(data.data()),
                                        data.size()));
            continue;
        }
        if (type == 'x')
        {
            pending_name = PaxPath(data);
            continue;
        }
        if (type == 'g')
        {
            continue;
        }
        std::string name = pending_name;
        pending_name.clear();
        if (name.empty())
        {
            name = FieldString(header, 100);
            if (std::memcmp(header + 257, "ustar", 5) == 0)
            {
                const std::string prefix = FieldString(header + 345, 155);
                if (!prefix.empty())
                {
                    name = prefix + "/" + name;
                }
            }
        }
        if (type == '5')
        {
            continue;  // Directories are made as files need them.
        }
        if (type != '0' && type != '\0' && type != '7')
        {
            *error = "unsupported tar entry type for " + name;
            return false;
        }
        total += size;
        if (total > kMaxReleaseArchiveBytes)
        {
            *error = "archive content too large";
            return false;
        }
        ArchiveEntry entry;
        entry.name = name;
        entry.data = data;
        entry.executable = mode_ok && (mode & 0111U) != 0U;
        entries->push_back(std::move(entry));
    }
    *error = "tar archive has no end block";
    return false;
}

bool ReadZip(const std::vector<std::uint8_t>& archive,
             std::vector<ArchiveEntry>* entries, std::string* error)
{
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, archive.data(), archive.size(), 0))
    {
        *error = "not a zip archive";
        return false;
    }
    bool ok = true;
    std::uint64_t total = 0;
    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint index = 0; index < count && ok; ++index)
    {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip, index, &stat))
        {
            *error = "unreadable zip entry";
            ok = false;
            break;
        }
        if (stat.m_is_directory)
        {
            continue;
        }
        if (stat.m_is_encrypted || !stat.m_is_supported)
        {
            *error = std::string("unsupported zip entry ") + stat.m_filename;
            ok = false;
            break;
        }
        total += stat.m_uncomp_size;
        if (total > kMaxReleaseArchiveBytes)
        {
            *error = "archive content too large";
            ok = false;
            break;
        }
        std::size_t size = 0;
        void* data = mz_zip_reader_extract_to_heap(&zip, index, &size, 0);
        if (data == nullptr)
        {
            *error = std::string("zip entry does not inflate: ") + stat.m_filename;
            ok = false;
            break;
        }
        ArchiveEntry entry;
        entry.name = stat.m_filename;
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        entry.data.assign(bytes, bytes + size);
        mz_free(data);
        entries->push_back(std::move(entry));
    }
    mz_zip_reader_end(&zip);
    return ok;
}

}  // namespace

std::optional<ReleaseArchiveFormat> ReleaseArchiveFormatForName(
    std::string_view name)
{
    if (EndsWith(name, ".zip"))
    {
        return ReleaseArchiveFormat::kZip;
    }
    if (EndsWith(name, ".tar.gz"))
    {
        return ReleaseArchiveFormat::kTarGz;
    }
    return std::nullopt;
}

bool ExtractReleaseArchive(const std::vector<std::uint8_t>& archive,
                           ReleaseArchiveFormat format,
                           const std::filesystem::path& staging,
                           std::vector<std::filesystem::path>* files,
                           std::string* error)
{
    files->clear();
    std::vector<ArchiveEntry> entries;
    if (format == ReleaseArchiveFormat::kTarGz)
    {
        std::vector<std::uint8_t> tar;
        if (!InflateGzip(archive, &tar, error) || !ReadTar(tar, &entries, error))
        {
            return false;
        }
    }
    else if (!ReadZip(archive, &entries, error))
    {
        return false;
    }
    if (entries.empty())
    {
        *error = "archive has no files";
        return false;
    }

    // Every name made safe first, so a bad entry fails before anything is
    // written.
    std::vector<std::vector<std::string>> components(entries.size());
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        if (!SafeComponents(entries[index].name, &components[index]) ||
            components[index].empty())
        {
            *error = "unsafe path in archive: " + entries[index].name;
            return false;
        }
    }
    // One shared top folder, with every file below it, is stripped.
    bool strip = true;
    for (const auto& parts : components)
    {
        if (parts.size() < 2U || parts.front() != components.front().front())
        {
            strip = false;
            break;
        }
    }

    std::error_code fs_error;
    if (std::filesystem::exists(staging, fs_error))
    {
        *error = "staging folder already exists: " + staging.string();
        return false;
    }
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        std::filesystem::path relative;
        for (std::size_t part = strip ? 1U : 0U;
             part < components[index].size(); ++part)
        {
            // UTF-8 names, read as such on every host (u8path is deprecated).
            const std::string& component = components[index][part];
            relative /= std::filesystem::path(std::u8string(
                reinterpret_cast<const char8_t*>(component.data()),
                component.size()));
        }
        const std::filesystem::path target = staging / relative;
        std::filesystem::create_directories(target.parent_path(), fs_error);
        std::ofstream stream(target, std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<const char*>(entries[index].data.data()),
                     static_cast<std::streamsize>(entries[index].data.size()));
        stream.close();
        if (!stream)
        {
            *error = "cannot write " + target.string();
            return false;
        }
        if (entries[index].executable)
        {
            std::filesystem::permissions(
                target,
                std::filesystem::perms::owner_exec |
                    std::filesystem::perms::group_exec |
                    std::filesystem::perms::others_exec,
                std::filesystem::perm_options::add, fs_error);
        }
        files->push_back(relative);
    }
    return true;
}

}  // namespace repiu::update
