#include "launcher_probe.h"

#include "repiu/launcher/launcher_settings.h"
#include "repiu/launcher/rom_set_catalog.h"
#include "repiu/target/target_profile.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace repiu::tools
{
namespace
{

using repiu::launcher::ApplyLauncherSettings;
using repiu::launcher::BuildLauncherChildCommandLine;
using repiu::launcher::BuildRomSetCatalog;
using repiu::launcher::LauncherSettings;
using repiu::launcher::LauncherSettingsPath;
using repiu::launcher::LoadLauncherSettings;
using repiu::launcher::ProbeRomSet;
using repiu::launcher::ResolveLauncherEnvironmentOverrides;
using repiu::launcher::RomSetAvailability;
using repiu::launcher::RomSetEntry;
using repiu::launcher::SaveLauncherSettings;
using repiu::launcher::kLauncherSwapIntervalVariable;
using repiu::launcher::kLauncherYmzVolumeVariable;

std::filesystem::path MakeScratchDirectory(const std::string& name)
{
    std::error_code error;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path(error) /
        ("repiu_launcher_probe_" + name);
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    return root;
}

void WriteFile(const std::filesystem::path& path, const std::string& text)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

// PiuRomZipHasRequiredEntries scans the file for the four PIU10 entry names,
// so a file carrying those names stands in for a real archive.
void WriteRomZip(const std::filesystem::path& path, const bool complete)
{
    std::string text = "PK\x03\x04 fake archive ";
    text += "mk3_1.0_bios.u22 mk3_1.1_bios.u22 piu10.u8 ";
    if (complete)
    {
        text += "piu10.u9 ";
    }
    WriteFile(path, text);
}

bool ProbeCatalogStates()
{
    const std::filesystem::path root = MakeScratchDirectory("catalog");
    const std::filesystem::path roms = root / "roms";
    std::error_code error;
    std::filesystem::create_directories(roms, error);

    auto probe = [&roms](const std::string& id) {
        return ProbeRomSet(id, id + " display", "parent", roms);
    };

    const RomSetEntry missing_zip = probe("nozip");

    WriteRomZip(roms / "badzip.zip", false);
    const RomSetEntry bad_zip = probe("badzip");

    WriteRomZip(roms / "nodir.zip", true);
    const RomSetEntry missing_directory = probe("nodir");

    WriteRomZip(roms / "nochd.zip", true);
    std::filesystem::create_directories(roms / "nochd", error);
    WriteFile(roms / "nochd" / "readme.txt", "not a chd");
    const RomSetEntry missing_chd = probe("nochd");

    WriteRomZip(roms / "twochd.zip", true);
    WriteFile(roms / "twochd" / "a.chd", "chd a");
    WriteFile(roms / "twochd" / "b.chd", "chd b");
    const RomSetEntry multiple_chd = probe("twochd");

    WriteRomZip(roms / "good.zip", true);
    WriteFile(roms / "good" / "disc.chd", "chd");
    const RomSetEntry runnable = probe("good");

    const bool ok =
        missing_zip.availability == RomSetAvailability::kMissingZip &&
        !missing_zip.reason.empty() &&
        bad_zip.availability == RomSetAvailability::kZipMissingEntries &&
        missing_directory.availability ==
            RomSetAvailability::kMissingChdDirectory &&
        missing_chd.availability == RomSetAvailability::kMissingChd &&
        multiple_chd.availability == RomSetAvailability::kMultipleChd &&
        runnable.availability == RomSetAvailability::kRunnable &&
        runnable.reason.empty() &&
        runnable.chd_path == roms / "good" / "disc.chd" &&
        runnable.display_name == "good display" &&
        runnable.parent_id == "parent" &&
        // Only a runnable entry resolves a CHD path.
        multiple_chd.chd_path.empty();

    std::filesystem::remove_all(root, error);
    return ok;
}

bool ProbeCatalogCoverage()
{
    const std::filesystem::path root = MakeScratchDirectory("coverage");
    const std::vector<RomSetEntry> catalog = BuildRomSetCatalog(root / "roms");

    std::size_t expected = 0;
    for (const target::TargetProfile& profile :
         target::GetBuiltInTargetProfiles())
    {
        if (!profile.rom_set_id.empty())
        {
            ++expected;
        }
    }
    const bool sized = catalog.size() == expected && expected >= 22U;
    // An empty roms root makes every entry unavailable rather than absent, so
    // the launcher can explain each one instead of silently hiding it.
    const bool all_missing =
        std::all_of(catalog.begin(), catalog.end(), [](const RomSetEntry& e) {
            return e.availability == RomSetAvailability::kMissingZip &&
                !e.display_name.empty();
        });
    const bool has_known = std::any_of(
        catalog.begin(), catalog.end(),
        [](const RomSetEntry& e) { return e.id == "pumpit1"; });
    // The direct-executable profile carries no ROM set and must be skipped.
    const bool skips_non_rom_set = std::none_of(
        catalog.begin(), catalog.end(),
        [](const RomSetEntry& e) { return e.id == "dos4gw_hello"; });

    std::error_code error;
    std::filesystem::remove_all(root, error);
    return sized && all_missing && has_known && skips_non_rom_set;
}

bool ProbeSettingsRoundTrip()
{
    const std::filesystem::path root = MakeScratchDirectory("settings");
    const std::filesystem::path config = root / "cfg";

    // A missing file is not an error and stores nothing.
    const auto absent = LoadLauncherSettings(config);
    const bool absent_ok = !absent.file_present &&
        !absent.settings.has_swap_interval &&
        !absent.settings.has_ymz_volume &&
        absent.settings.last_rom_set.empty() && absent.warnings.empty();

    LauncherSettings settings;
    settings.has_swap_interval = true;
    settings.swap_interval = 1;
    settings.has_ymz_volume = true;
    settings.ymz_volume = 0.5F;
    settings.last_rom_set = "pumpit8";
    const bool saved = SaveLauncherSettings(config, settings);
    const auto reloaded = LoadLauncherSettings(config);
    const bool round_trip = saved && reloaded.file_present &&
        reloaded.warnings.empty() && reloaded.settings.has_swap_interval &&
        reloaded.settings.swap_interval == 1 &&
        reloaded.settings.has_ymz_volume &&
        reloaded.settings.ymz_volume > 0.49F &&
        reloaded.settings.ymz_volume < 0.51F &&
        reloaded.settings.last_rom_set == "pumpit8";

    // Absent keys stay absent rather than defaulting, so an unwritten option
    // never publishes anything.
    LauncherSettings empty;
    empty.last_rom_set = "pumpit1";
    const bool saved_empty = SaveLauncherSettings(config, empty);
    const auto reloaded_empty = LoadLauncherSettings(config);
    const bool sparse_ok = saved_empty &&
        !reloaded_empty.settings.has_swap_interval &&
        !reloaded_empty.settings.has_ymz_volume &&
        reloaded_empty.settings.last_rom_set == "pumpit1";

    // A malformed value warns and is ignored; the rest of the file still
    // applies, matching the config-file rule the project already follows.
    WriteFile(LauncherSettingsPath(config),
              "[Video]\nswap_interval = later\n"
              "[Audio]\nymz_volume = loud\n"
              "[Launcher]\nlast_rom_set = pumpit3\n");
    const auto malformed = LoadLauncherSettings(config);
    const bool malformed_ok = malformed.file_present &&
        malformed.warnings.size() == 2U &&
        !malformed.settings.has_swap_interval &&
        !malformed.settings.has_ymz_volume &&
        malformed.settings.last_rom_set == "pumpit3";

    std::error_code error;
    std::filesystem::remove_all(root, error);
    return absent_ok && round_trip && sparse_ok && malformed_ok;
}

bool ProbeEnvironmentPrecedence()
{
    LauncherSettings settings;
    settings.has_swap_interval = true;
    settings.swap_interval = 1;
    settings.has_ymz_volume = true;
    settings.ymz_volume = 2.0F;

    std::vector<std::pair<std::string, std::string>> published;
    const auto publish = [&published](const char* name,
                                      const std::string& value) {
        published.emplace_back(name, value);
    };

    // Nothing in the environment: both values are published under the names the
    // existing consumers read.
    published.clear();
    const auto free_overrides =
        ResolveLauncherEnvironmentOverrides(nullptr, nullptr);
    const auto applied_free =
        ApplyLauncherSettings(settings, free_overrides, publish);
    const bool free_ok = applied_free.swap_interval_published &&
        applied_free.ymz_volume_published && published.size() == 2U &&
        published[0].first == kLauncherSwapIntervalVariable &&
        published[0].second == "1" &&
        published[1].first == kLauncherYmzVolumeVariable;

    // An environment variable already set wins, even when empty.
    published.clear();
    const auto held = ResolveLauncherEnvironmentOverrides("0", "");
    const auto applied_held = ApplyLauncherSettings(settings, held, publish);
    const bool held_ok = !applied_held.swap_interval_published &&
        !applied_held.ymz_volume_published && published.empty();

    // A partially set environment publishes only the other option.
    published.clear();
    const auto mixed = ResolveLauncherEnvironmentOverrides(nullptr, "1.0");
    const auto applied_mixed = ApplyLauncherSettings(settings, mixed, publish);
    const bool mixed_ok = applied_mixed.swap_interval_published &&
        !applied_mixed.ymz_volume_published && published.size() == 1U &&
        published[0].first == kLauncherSwapIntervalVariable;

    // Unstored options publish nothing at all.
    published.clear();
    const auto applied_absent =
        ApplyLauncherSettings(LauncherSettings{}, free_overrides, publish);
    const bool absent_ok = !applied_absent.swap_interval_published &&
        !applied_absent.ymz_volume_published && published.empty();

    return free_ok && held_ok && mixed_ok && absent_ok;
}

bool ProbeChildCommandLine()
{
    // The executable path is always quoted, so a space in it stays one
    // argument. This is the failure that never shows up in a developer tree
    // and always shows up in an installed one.
    const bool spaced =
        BuildLauncherChildCommandLine("C:\\Program Files\\rePIU\\repiu.exe",
                                      "pumpit8") ==
        "\"C:\\Program Files\\rePIU\\repiu.exe\" pumpit8";
    const bool plain =
        BuildLauncherChildCommandLine("repiu.exe", "pumpit1") ==
        "\"repiu.exe\" pumpit1";
    // An empty ROM set leaves no trailing separator, so the child sees no
    // argument at all rather than an empty one, which would send it straight
    // back into the launcher.
    const bool empty_rom_set =
        BuildLauncherChildCommandLine("repiu.exe", "") ==
        "\"repiu.exe\"";
    return spaced && plain && empty_rom_set;
}

}  // namespace

bool RunLauncherProbe()
{
    const bool states_ok = ProbeCatalogStates();
    const bool coverage_ok = ProbeCatalogCoverage();
    const bool settings_ok = ProbeSettingsRoundTrip();
    const bool precedence_ok = ProbeEnvironmentPrecedence();
    const bool command_line_ok = ProbeChildCommandLine();
    const bool all = states_ok && coverage_ok && settings_ok &&
        precedence_ok && command_line_ok;
    std::cout << "launcher_catalog_states=" << (states_ok ? "true" : "false")
              << "\nlauncher_catalog_coverage="
              << (coverage_ok ? "true" : "false")
              << "\nlauncher_settings_round_trip="
              << (settings_ok ? "true" : "false")
              << "\nlauncher_environment_precedence="
              << (precedence_ok ? "true" : "false")
              << "\nlauncher_child_command_line="
              << (command_line_ok ? "true" : "false")
              << "\nlauncher_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
