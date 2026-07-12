#include "repiu/assets/pumpit1_mount.h"

#include <libchdr/chd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace repiu::assets
{
namespace
{

constexpr std::uint32_t kIsoSectorBytes = 2048;

std::uint32_t ReadLe32(const std::uint8_t* bytes)
{
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

bool ZipContainsRequiredPumpIt1Entries(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return false;
    }
    const std::vector<std::string> required = {
        "mk3_1.0_bios.u22", "mk3_1.1_bios.u22", "piu10.u8", "piu10.u9"};
    std::vector<bool> found(required.size(), false);
    std::array<char, 4096> buffer = {};
    std::string overlap;
    while (stream)
    {
        stream.read(buffer.data(), buffer.size());
        const std::streamsize count = stream.gcount();
        if (count <= 0)
        {
            break;
        }
        std::string chunk = overlap;
        chunk.append(buffer.data(), static_cast<std::size_t>(count));
        for (std::size_t index = 0; index < required.size(); ++index)
        {
            found[index] = found[index] ||
                           chunk.find(required[index]) != std::string::npos;
        }
        overlap = chunk.substr(chunk.size() > 64 ? chunk.size() - 64 : 0);
    }
    return std::all_of(found.begin(), found.end(), [](bool value) {
        return value;
    });
}

class ChdCdReader
{
public:
    ~ChdCdReader()
    {
        if (file_ != nullptr)
        {
            chd_close(file_);
        }
    }

    bool Open(const std::filesystem::path& path, std::string* message)
    {
        const std::string narrow = path.string();
        const chd_error error = chd_open(
            narrow.c_str(), CHD_OPEN_READ, nullptr, &file_);
        if (error != CHDERR_NONE)
        {
            if (message != nullptr)
            {
                *message = std::string("chd_open failed: ") +
                           chd_error_string(error);
            }
            return false;
        }
        header_ = chd_get_header(file_);
        if (header_ == nullptr || header_->version != 5 ||
            header_->unitbytes < 2352 || header_->hunkbytes == 0 ||
            header_->hunkbytes % header_->unitbytes != 0)
        {
            if (message != nullptr)
            {
                *message = "CHD is not a supported v5 raw CD image";
            }
            return false;
        }
        hunk_.resize(header_->hunkbytes);
        frames_per_hunk_ = header_->hunkbytes / header_->unitbytes;
        return true;
    }

    bool ReadIsoSector(std::uint32_t lba,
                       std::array<std::uint8_t, kIsoSectorBytes>* sector)
    {
        if (header_ == nullptr || sector == nullptr)
        {
            return false;
        }
        const std::uint32_t hunk_index = lba / frames_per_hunk_;
        if (cached_hunk_ != hunk_index)
        {
            if (chd_read(file_, hunk_index, hunk_.data()) != CHDERR_NONE)
            {
                return false;
            }
            cached_hunk_ = hunk_index;
        }
        const std::uint32_t frame_index = lba % frames_per_hunk_;
        const std::uint8_t* frame = hunk_.data() +
            static_cast<std::size_t>(frame_index) * header_->unitbytes;
        const std::uint32_t user_offset = frame[15] == 2 ? 24U : 16U;
        if (user_offset + sector->size() > header_->unitbytes)
        {
            return false;
        }
        std::memcpy(sector->data(), frame + user_offset, sector->size());
        return true;
    }

    std::string Identity() const
    {
        if (header_ == nullptr)
        {
            return {};
        }
        std::ostringstream stream;
        stream << header_->logicalbytes << ':' << header_->hunkbytes << ':';
        for (std::uint8_t byte : header_->sha1)
        {
            stream << std::hex << std::setw(2) << std::setfill('0')
                   << static_cast<unsigned>(byte);
        }
        return stream.str();
    }

private:
    chd_file* file_ = nullptr;
    const chd_header* header_ = nullptr;
    std::vector<std::uint8_t> hunk_;
    std::uint32_t frames_per_hunk_ = 0;
    std::uint32_t cached_hunk_ = UINT32_MAX;
};

struct IsoRecord
{
    std::uint32_t extent = 0;
    std::uint32_t bytes = 0;
    bool directory = false;
    std::string name;
};

std::string NormalizeIsoName(const std::uint8_t* bytes, std::size_t length)
{
    std::string name(reinterpret_cast<const char*>(bytes), length);
    const std::size_t version = name.find(';');
    if (version != std::string::npos)
    {
        name.resize(version);
    }
    while (!name.empty() && name.back() == '.')
    {
        name.pop_back();
    }
    for (char& value : name)
    {
        if (value == '/' || value == '\\' || value == ':')
        {
            value = '_';
        }
    }
    return name;
}

bool ReadExtent(ChdCdReader* reader,
                std::uint32_t extent,
                std::uint32_t byte_count,
                std::vector<std::uint8_t>* bytes)
{
    if (reader == nullptr || bytes == nullptr)
    {
        return false;
    }
    bytes->assign(byte_count, 0);
    std::array<std::uint8_t, kIsoSectorBytes> sector = {};
    for (std::uint32_t offset = 0; offset < byte_count;
         offset += kIsoSectorBytes)
    {
        if (!reader->ReadIsoSector(extent + offset / kIsoSectorBytes,
                                   &sector))
        {
            return false;
        }
        const std::uint32_t copy = std::min<std::uint32_t>(
            kIsoSectorBytes, byte_count - offset);
        std::memcpy(bytes->data() + offset, sector.data(), copy);
    }
    return true;
}

bool ParseDirectory(ChdCdReader* reader,
                    std::uint32_t extent,
                    std::uint32_t byte_count,
                    std::vector<IsoRecord>* records)
{
    std::vector<std::uint8_t> bytes;
    if (records == nullptr || !ReadExtent(reader, extent, byte_count, &bytes))
    {
        return false;
    }
    for (std::size_t offset = 0; offset < bytes.size();)
    {
        const std::uint8_t length = bytes[offset];
        if (length == 0)
        {
            offset = ((offset / kIsoSectorBytes) + 1U) * kIsoSectorBytes;
            continue;
        }
        if (offset + length > bytes.size() || length < 34)
        {
            return false;
        }
        const std::uint8_t name_length = bytes[offset + 32];
        if (33U + name_length > length)
        {
            return false;
        }
        const std::uint8_t* name = bytes.data() + offset + 33;
        if (!(name_length == 1 && (name[0] == 0 || name[0] == 1)))
        {
            records->push_back(IsoRecord{
                ReadLe32(bytes.data() + offset + 2),
                ReadLe32(bytes.data() + offset + 10),
                (bytes[offset + 25] & 0x02U) != 0,
                NormalizeIsoName(name, name_length),
            });
        }
        offset += length;
    }
    return true;
}

bool ExtractTree(ChdCdReader* reader,
                 const IsoRecord& directory,
                 const std::filesystem::path& output,
                 PumpIt1MountResult* result,
                 std::uint32_t depth)
{
    if (depth > 32 || !std::filesystem::create_directories(output) &&
                          !std::filesystem::is_directory(output))
    {
        return false;
    }
    std::vector<IsoRecord> records;
    if (!ParseDirectory(reader, directory.extent, directory.bytes, &records))
    {
        return false;
    }
    for (const IsoRecord& record : records)
    {
        if (record.name.empty() || record.name == "." || record.name == "..")
        {
            continue;
        }
        const std::filesystem::path target = output / record.name;
        if (record.directory)
        {
            if (!ExtractTree(reader, record, target, result, depth + 1U))
            {
                return false;
            }
            continue;
        }
        std::vector<std::uint8_t> bytes;
        if (!ReadExtent(reader, record.extent, record.bytes, &bytes))
        {
            return false;
        }
        std::ofstream stream(target, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            return false;
        }
        stream.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
        if (!stream)
        {
            return false;
        }
        ++result->extracted_file_count;
        result->extracted_byte_count += bytes.size();
    }
    return true;
}

}  // namespace

bool PreparePumpIt1Mount(const std::filesystem::path& roms_root,
                         const std::filesystem::path& cache_root,
                         PumpIt1MountResult* result)
{
    if (result == nullptr)
    {
        return false;
    }
    *result = PumpIt1MountResult{};
    result->rom_zip_path = roms_root / "pumpit1.zip";
    const std::filesystem::path chd_directory = roms_root / "pumpit1";
    if (!ZipContainsRequiredPumpIt1Entries(result->rom_zip_path))
    {
        result->message = "pumpit1.zip is missing required MAME ROM entries";
        return true;
    }
    std::error_code directory_error;
    const std::filesystem::directory_iterator directory(
        chd_directory, directory_error);
    if (directory_error)
    {
        result->message = "pumpit1 CHD directory not found";
        return true;
    }
    for (const auto& entry : directory)
    {
        if (entry.is_regular_file() && entry.path().extension() == ".chd")
        {
            if (!result->chd_path.empty())
            {
                result->message = "multiple pumpit1 CHD files found";
                return true;
            }
            result->chd_path = entry.path();
        }
    }
    if (result->chd_path.empty())
    {
        result->message = "pumpit1 CHD file not found";
        return true;
    }

    ChdCdReader reader;
    if (!reader.Open(result->chd_path, &result->message))
    {
        return true;
    }
    result->mount_root = cache_root / "pumpit1";
    result->executable_path = result->mount_root / "PIU" / "PIU.EXE";
    const std::filesystem::path marker = result->mount_root / ".chd-identity";
    const std::string identity = reader.Identity();
    std::ifstream marker_input(marker);
    std::string cached_identity;
    std::getline(marker_input, cached_identity);
    if (cached_identity == identity &&
        std::filesystem::is_regular_file(result->executable_path))
    {
        result->valid = true;
        result->mounted = true;
        result->cache_reused = true;
        result->message = "pumpit1 CHD mount cache reused";
        return true;
    }

    std::error_code remove_error;
    std::filesystem::remove_all(result->mount_root, remove_error);
    std::filesystem::create_directories(result->mount_root);
    std::array<std::uint8_t, kIsoSectorBytes> pvd = {};
    if (!reader.ReadIsoSector(16, &pvd) || pvd[0] != 1 ||
        std::memcmp(pvd.data() + 1, "CD001", 5) != 0)
    {
        result->message = "CHD does not contain an ISO9660 primary volume";
        return true;
    }
    const std::uint8_t* root = pvd.data() + 156;
    if (root[0] < 34)
    {
        result->message = "ISO9660 root directory record is invalid";
        return true;
    }
    const IsoRecord root_record{
        ReadLe32(root + 2), ReadLe32(root + 10), true, {}};
    if (!ExtractTree(&reader, root_record, result->mount_root, result, 0))
    {
        result->message = "failed to materialize ISO9660 mount view";
        return true;
    }
    std::ofstream marker_output(marker, std::ios::trunc);
    marker_output << identity << '\n';
    if (!marker_output ||
        !std::filesystem::is_regular_file(result->executable_path))
    {
        result->message = "mounted ISO9660 does not contain PIU/PIU.EXE";
        return true;
    }
    result->valid = true;
    result->mounted = true;
    result->message = "pumpit1 CHD mounted as ISO9660 read-only cache";
    return true;
}

}  // namespace repiu::assets
