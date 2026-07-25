#ifndef REPIU_ASSETS_ROM_ZIP_ARCHIVE_H_
#define REPIU_ASSETS_ROM_ZIP_ARCHIVE_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace repiu::assets
{

// One extracted MAME ROM-set member. `crc32` is the value recorded in the ZIP
// central directory, which for a MAME set is also the canonical ROM checksum, so
// a caller can compare it against the known-good value from the driver.
struct RomZipEntry
{
    bool valid = false;
    std::string name;
    std::uint32_t crc32 = 0;
    std::vector<std::uint8_t> data;
    std::string message;
};

// Extracts a single entry from a ROM ZIP into memory and verifies the extracted
// bytes against the CRC32 stored in the archive directory. ROM sets are small
// enough (a few MiB) that streaming is unnecessary.
//
// Returns a result whose `valid` is false on any failure; `message` always
// describes the outcome. The caller decides whether a failure is fatal.
RomZipEntry ExtractRomZipEntry(const std::filesystem::path& zip_path,
                               const std::string& entry_name);

}  // namespace repiu::assets

#endif  // REPIU_ASSETS_ROM_ZIP_ARCHIVE_H_
