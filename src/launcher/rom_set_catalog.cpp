#include "repiu/launcher/rom_set_catalog.h"

#include "repiu/assets/piu_chd_mount.h"
#include "repiu/target/target_profile.h"

#include <system_error>

namespace repiu::launcher
{
namespace
{

// Mirrors the mount's own scan: exactly one .chd directly under roms/<id>/.
struct ChdScan
{
    bool directory_present = false;
    std::uint32_t chd_count = 0;
    std::filesystem::path chd_path;
};

ChdScan ScanChdDirectory(const std::filesystem::path& directory)
{
    ChdScan scan;
    std::error_code directory_error;
    const std::filesystem::directory_iterator iterator(directory,
                                                       directory_error);
    if (directory_error)
    {
        return scan;
    }
    scan.directory_present = true;
    for (const auto& entry : iterator)
    {
        std::error_code entry_error;
        if (!entry.is_regular_file(entry_error) || entry_error)
        {
            continue;
        }
        if (entry.path().extension() != ".chd")
        {
            continue;
        }
        ++scan.chd_count;
        if (scan.chd_count == 1U)
        {
            scan.chd_path = entry.path();
        }
    }
    return scan;
}

}  // namespace

const char* RomSetAvailabilityName(const RomSetAvailability value)
{
    switch (value)
    {
        case RomSetAvailability::kRunnable:
            return "runnable";
        case RomSetAvailability::kMissingZip:
            return "missing-zip";
        case RomSetAvailability::kZipMissingEntries:
            return "zip-missing-entries";
        case RomSetAvailability::kMissingChdDirectory:
            return "missing-chd-directory";
        case RomSetAvailability::kMissingChd:
            return "missing-chd";
        case RomSetAvailability::kMultipleChd:
            return "multiple-chd";
    }
    return "unknown";
}

RomSetEntry ProbeRomSet(const std::string& rom_set_id,
                        const std::string& display_name,
                        const std::string& parent_id,
                        const std::filesystem::path& roms_root)
{
    RomSetEntry entry;
    entry.id = rom_set_id;
    entry.display_name = display_name;
    entry.parent_id = parent_id;
    entry.rom_zip_path = roms_root / (rom_set_id + ".zip");

    std::error_code zip_error;
    if (!std::filesystem::is_regular_file(entry.rom_zip_path, zip_error) ||
        zip_error)
    {
        entry.availability = RomSetAvailability::kMissingZip;
        entry.reason = "roms/" + rom_set_id + ".zip not found";
        return entry;
    }
    if (!assets::PiuRomZipHasRequiredEntries(entry.rom_zip_path))
    {
        entry.availability = RomSetAvailability::kZipMissingEntries;
        entry.reason =
            "roms/" + rom_set_id + ".zip is missing required PIU10 entries";
        return entry;
    }

    const ChdScan scan = ScanChdDirectory(roms_root / rom_set_id);
    if (!scan.directory_present)
    {
        entry.availability = RomSetAvailability::kMissingChdDirectory;
        entry.reason = "roms/" + rom_set_id + "/ not found";
        return entry;
    }
    if (scan.chd_count == 0U)
    {
        entry.availability = RomSetAvailability::kMissingChd;
        entry.reason = "no .chd under roms/" + rom_set_id + "/";
        return entry;
    }
    if (scan.chd_count > 1U)
    {
        entry.availability = RomSetAvailability::kMultipleChd;
        entry.reason = "more than one .chd under roms/" + rom_set_id + "/";
        return entry;
    }

    entry.availability = RomSetAvailability::kRunnable;
    entry.chd_path = scan.chd_path;
    return entry;
}

std::vector<RomSetEntry> BuildRomSetCatalog(
    const std::filesystem::path& roms_root)
{
    std::vector<RomSetEntry> catalog;
    const auto& profiles = target::GetBuiltInTargetProfiles();
    catalog.reserve(profiles.size());
    for (const target::TargetProfile& profile : profiles)
    {
        if (profile.rom_set_id.empty())
        {
            continue;
        }
        catalog.push_back(ProbeRomSet(std::string(profile.rom_set_id),
                                      std::string(profile.display_name),
                                      std::string(profile.parent_rom_set_id),
                                      roms_root));
    }
    return catalog;
}

}  // namespace repiu::launcher
