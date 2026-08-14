#include "repiu/assets/rom_zip_archive.h"

#include <miniz.h>

#include <fstream>
#include <iomanip>
#include <sstream>

namespace repiu::assets
{
namespace
{

// MAME ROM sets are torrentzipped, so every member is deflate-compressed and the
// archive is read-only. miniz ships with libchdr; the ZIP reader APIs are enabled
// for this build through the MINIZ_ARCHIVE_APIS option in the top-level
// CMakeLists rather than by vendoring a second copy of miniz.
bool ReadWholeFile(const std::filesystem::path& path,
                   std::vector<std::uint8_t>* out,
                   std::string* message)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
    {
        *message = "cannot open ROM archive: " + path.string();
        return false;
    }
    const std::streamoff size = stream.tellg();
    if (size <= 0)
    {
        *message = "ROM archive is empty: " + path.string();
        return false;
    }
    stream.seekg(0, std::ios::beg);
    out->resize(static_cast<std::size_t>(size));
    if (!stream.read(reinterpret_cast<char*>(out->data()),
                     static_cast<std::streamsize>(size)))
    {
        *message = "cannot read ROM archive: " + path.string();
        return false;
    }
    return true;
}

std::string Hex32(std::uint32_t value)
{
    std::ostringstream stream;
    stream << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
    return stream.str();
}

}  // namespace

RomZipEntry ExtractRomZipEntry(const std::filesystem::path& zip_path,
                               const std::string& entry_name)
{
    RomZipEntry result;
    result.name = entry_name;

    std::vector<std::uint8_t> archive;
    if (!ReadWholeFile(zip_path, &archive, &result.message))
    {
        return result;
    }

    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);
    if (!mz_zip_reader_init_mem(&zip, archive.data(), archive.size(), 0))
    {
        result.message = "not a readable ZIP archive: " + zip_path.string();
        return result;
    }

    const int index = mz_zip_reader_locate_file(
        &zip, entry_name.c_str(), nullptr, 0);
    if (index < 0)
    {
        mz_zip_reader_end(&zip);
        result.missing = true;
        result.message =
            "ROM archive has no entry '" + entry_name + "': " + zip_path.string();
        return result;
    }

    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&zip, static_cast<mz_uint>(index), &stat))
    {
        mz_zip_reader_end(&zip);
        result.message = "cannot stat ROM entry '" + entry_name + "'";
        return result;
    }

    result.data.resize(static_cast<std::size_t>(stat.m_uncomp_size));
    if (!result.data.empty() &&
        !mz_zip_reader_extract_to_mem(&zip, static_cast<mz_uint>(index),
                                      result.data.data(), result.data.size(), 0))
    {
        mz_zip_reader_end(&zip);
        result.message = "cannot extract ROM entry '" + entry_name + "'";
        return result;
    }
    mz_zip_reader_end(&zip);

    result.crc32 = static_cast<std::uint32_t>(mz_crc32(
        MZ_CRC32_INIT, result.data.data(), result.data.size()));
    if (result.crc32 != static_cast<std::uint32_t>(stat.m_crc32))
    {
        std::ostringstream stream;
        stream << "ROM entry '" << entry_name << "' failed CRC32 check: computed "
               << Hex32(result.crc32) << " but archive records "
               << Hex32(static_cast<std::uint32_t>(stat.m_crc32));
        result.message = stream.str();
        result.data.clear();
        return result;
    }

    std::ostringstream stream;
    stream << "extracted '" << entry_name << "' (" << result.data.size()
           << " bytes, crc32 " << Hex32(result.crc32) << ")";
    result.message = stream.str();
    result.valid = true;
    return result;
}

RomZipEntry ExtractRomZipEntryWithParentFallback(
    const std::filesystem::path& zip_path,
    const std::string& entry_name,
    std::string_view parent_rom_set_id,
    const std::string& parent_entry_name)
{
    RomZipEntry result = ExtractRomZipEntry(zip_path, entry_name);
    if (result.valid || !result.missing || parent_rom_set_id.empty() ||
        parent_entry_name.empty())
    {
        return result;
    }

    result = ExtractRomZipEntry(zip_path, parent_entry_name);
    if (result.valid)
    {
        result.message = "parent fallback in current archive: " + result.message;
        return result;
    }
    if (!result.missing)
    {
        return result;
    }

    const std::filesystem::path parent_zip_path =
        zip_path.parent_path() /
        (std::string(parent_rom_set_id) + zip_path.extension().string());
    result = ExtractRomZipEntry(parent_zip_path, parent_entry_name);
    if (result.valid)
    {
        result.message = "parent archive fallback: " + result.message;
    }
    return result;
}

}  // namespace repiu::assets
