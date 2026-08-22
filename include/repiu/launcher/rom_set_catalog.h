#ifndef REPIU_LAUNCHER_ROM_SET_CATALOG_H_
#define REPIU_LAUNCHER_ROM_SET_CATALOG_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace repiu::launcher
{

// Why a ROM set can or cannot be started, using the same rules
// PreparePiuChdMount applies -- without extracting anything, because the
// launcher lists every catalog entry on startup and a full integrity pass over
// twenty-two discs would take far longer than a person will wait. The real
// judgement still happens in the mount at launch; this only explains, up front,
// what is obviously missing.
enum class RomSetAvailability : std::uint8_t
{
    kRunnable = 0,
    kMissingZip,
    kZipMissingEntries,
    kMissingChdDirectory,
    kMissingChd,
    kMultipleChd,
};

struct RomSetEntry
{
    std::string id;
    std::string display_name;
    std::string parent_id;
    RomSetAvailability availability = RomSetAvailability::kMissingZip;
    // One line naming what is wrong, empty when runnable.
    std::string reason;
    std::filesystem::path rom_zip_path;
    std::filesystem::path chd_path;
};

// Every built-in target profile that names a ROM set, in catalog order, each
// probed against `roms_root`. Profiles without a ROM set id (the direct
// executable path) are skipped.
[[nodiscard]] std::vector<RomSetEntry> BuildRomSetCatalog(
    const std::filesystem::path& roms_root);

// Probes one ROM set id. Exposed separately so the launcher can refresh a
// single row without rescanning the whole catalog.
[[nodiscard]] RomSetEntry ProbeRomSet(const std::string& rom_set_id,
                                      const std::string& display_name,
                                      const std::string& parent_id,
                                      const std::filesystem::path& roms_root);

[[nodiscard]] const char* RomSetAvailabilityName(RomSetAvailability value);

[[nodiscard]] inline bool IsRomSetRunnable(const RomSetEntry& entry)
{
    return entry.availability == RomSetAvailability::kRunnable;
}

}  // namespace repiu::launcher

#endif  // REPIU_LAUNCHER_ROM_SET_CATALOG_H_
