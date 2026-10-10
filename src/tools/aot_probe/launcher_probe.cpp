#include "launcher_probe.h"

#include "repiu/launcher/command_line_options.h"
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
using repiu::launcher::kLauncherPostShaderVariable;
using repiu::launcher::kLauncherTextureFullPrecisionVariable;
using repiu::launcher::kLauncherFullscreenVariable;
using repiu::launcher::kLauncherKeepAspectVariable;

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
    settings.has_post_shader = true;
    settings.post_shader = "my_crt.glsl";
    settings.has_texture_full_precision = true;
    settings.texture_full_precision = false;
    settings.has_fullscreen = true;
    settings.fullscreen = true;
    settings.has_keep_aspect = true;
    settings.keep_aspect = false;
    const bool saved = SaveLauncherSettings(config, settings);
    const auto reloaded = LoadLauncherSettings(config);
    const bool round_trip = saved && reloaded.file_present &&
        reloaded.warnings.empty() && reloaded.settings.has_swap_interval &&
        reloaded.settings.swap_interval == 1 &&
        reloaded.settings.has_ymz_volume &&
        reloaded.settings.ymz_volume > 0.49F &&
        reloaded.settings.ymz_volume < 0.51F &&
        reloaded.settings.last_rom_set == "pumpit8" &&
        reloaded.settings.has_post_shader &&
        reloaded.settings.post_shader == "my_crt.glsl" &&
        reloaded.settings.has_texture_full_precision &&
        !reloaded.settings.texture_full_precision &&
        reloaded.settings.has_fullscreen && reloaded.settings.fullscreen &&
        reloaded.settings.has_keep_aspect &&
        !reloaded.settings.keep_aspect;

    // Absent keys stay absent rather than defaulting, so an unwritten option
    // never publishes anything.
    LauncherSettings empty;
    empty.last_rom_set = "pumpit1";
    const bool saved_empty = SaveLauncherSettings(config, empty);
    const auto reloaded_empty = LoadLauncherSettings(config);
    const bool sparse_ok = saved_empty &&
        !reloaded_empty.settings.has_swap_interval &&
        !reloaded_empty.settings.has_ymz_volume &&
        !reloaded_empty.settings.has_post_shader &&
        !reloaded_empty.settings.has_texture_full_precision &&
        !reloaded_empty.settings.has_fullscreen &&
        !reloaded_empty.settings.has_keep_aspect &&
        reloaded_empty.settings.last_rom_set == "pumpit1";

    // A malformed value warns and is ignored; the rest of the file still
    // applies, matching the config-file rule the project already follows.
    WriteFile(LauncherSettingsPath(config),
              "[Video]\nswap_interval = later\n"
              "texture_full_precision = maybe\n"
              "fullscreen = yes\nkeep_aspect = 2\n"
              "[Audio]\nymz_volume = loud\n"
              "[Launcher]\nlast_rom_set = pumpit3\n");
    const auto malformed = LoadLauncherSettings(config);
    const bool malformed_ok = malformed.file_present &&
        malformed.warnings.size() == 5U &&
        !malformed.settings.has_swap_interval &&
        !malformed.settings.has_texture_full_precision &&
        !malformed.settings.has_fullscreen &&
        !malformed.settings.has_keep_aspect &&
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
    settings.has_post_shader = true;
    settings.post_shader = "crt";
    settings.has_texture_full_precision = true;
    settings.texture_full_precision = false;

    std::vector<std::pair<std::string, std::string>> published;
    const auto publish = [&published](const char* name,
                                      const std::string& value) {
        published.emplace_back(name, value);
    };

    // Nothing in the environment: both values are published under the names the
    // existing consumers read.
    published.clear();
    const auto free_overrides =
        ResolveLauncherEnvironmentOverrides(nullptr, nullptr, nullptr);
    const auto applied_free =
        ApplyLauncherSettings(settings, free_overrides, publish);
    const bool free_ok = applied_free.swap_interval_published &&
        applied_free.ymz_volume_published &&
        applied_free.post_shader_published &&
        applied_free.texture_full_precision_published &&
        published.size() == 4U &&
        published[0].first == kLauncherSwapIntervalVariable &&
        published[0].second == "1" &&
        published[1].first == kLauncherYmzVolumeVariable &&
        published[2].first == kLauncherPostShaderVariable &&
        published[2].second == "crt" &&
        published[3].first ==
            std::string(kLauncherTextureFullPrecisionVariable) &&
        published[3].second == "0";

    // An environment variable already set wins, even when empty.
    published.clear();
    const auto held = ResolveLauncherEnvironmentOverrides("0", "", "", "1");
    const auto applied_held = ApplyLauncherSettings(settings, held, publish);
    const bool held_ok = !applied_held.swap_interval_published &&
        !applied_held.ymz_volume_published &&
        !applied_held.post_shader_published &&
        !applied_held.texture_full_precision_published && published.empty();

    // A partially set environment publishes only the other option.
    published.clear();
    const auto mixed = ResolveLauncherEnvironmentOverrides(nullptr, "1.0", nullptr);
    const auto applied_mixed = ApplyLauncherSettings(settings, mixed, publish);
    const bool mixed_ok = applied_mixed.swap_interval_published &&
        !applied_mixed.ymz_volume_published &&
        applied_mixed.post_shader_published && published.size() == 3U &&
        published[0].first == kLauncherSwapIntervalVariable &&
        published[1].first == kLauncherPostShaderVariable;

    // Unstored options publish nothing at all.
    published.clear();
    const auto applied_absent =
        ApplyLauncherSettings(LauncherSettings{}, free_overrides, publish);
    const bool absent_ok = !applied_absent.swap_interval_published &&
        !applied_absent.ymz_volume_published && published.empty();

    // Issue #45: the display options publish under their own names, and a
    // caller-set variable for either one holds only that one back.
    LauncherSettings display;
    display.has_fullscreen = true;
    display.fullscreen = true;
    display.has_keep_aspect = true;
    display.keep_aspect = false;
    published.clear();
    const auto applied_display =
        ApplyLauncherSettings(display, free_overrides, publish);
    const bool display_ok = applied_display.fullscreen_published &&
        applied_display.keep_aspect_published && published.size() == 2U &&
        published[0].first == std::string(kLauncherFullscreenVariable) &&
        published[0].second == "1" &&
        published[1].first == std::string(kLauncherKeepAspectVariable) &&
        published[1].second == "0";
    published.clear();
    const auto display_held = ResolveLauncherEnvironmentOverrides(
        nullptr, nullptr, nullptr, nullptr, "0", nullptr);
    const auto applied_display_held =
        ApplyLauncherSettings(display, display_held, publish);
    const bool display_held_ok = !applied_display_held.fullscreen_published &&
        applied_display_held.keep_aspect_published &&
        published.size() == 1U &&
        published[0].first == std::string(kLauncherKeepAspectVariable);

    return free_ok && held_ok && mixed_ok && absent_ok && display_ok &&
        display_held_ok;
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

// Task 771. The ROM set must stay the first positional argument whichever side
// of it the option sits on, and a malformed option must stop the run rather
// than become a ROM set name.
bool ProbeCommandLineOptions()
{
    using repiu::launcher::ParseCommandLineOptions;
    using Arguments = std::vector<std::string>;

    const auto after = ParseCommandLineOptions(
        Arguments{"pumpit8", "--post-shader", "crt"});
    const bool after_ok = after.error.empty() && after.has_post_shader &&
        after.post_shader == "crt" &&
        after.positional == Arguments{"pumpit8"};

    const auto before = ParseCommandLineOptions(
        Arguments{"--post-shader=scanline", "pumpitea"});
    const bool before_ok = before.error.empty() && before.has_post_shader &&
        before.post_shader == "scanline" &&
        before.positional == Arguments{"pumpitea"};

    const auto repeated = ParseCommandLineOptions(Arguments{
        "--post-shader", "crt", "pumpit1", "--post-shader=my_crt.glsl"});
    const bool repeated_ok = repeated.error.empty() &&
        repeated.post_shader == "my_crt.glsl" &&
        repeated.positional == Arguments{"pumpit1"};

    const auto none = ParseCommandLineOptions(Arguments{"pumpit1"});
    const bool none_ok = none.error.empty() && !none.has_post_shader &&
        none.positional == Arguments{"pumpit1"};

    const auto only_option =
        ParseCommandLineOptions(Arguments{"--post-shader", "crt"});
    const bool only_option_ok = only_option.error.empty() &&
        only_option.has_post_shader && only_option.positional.empty();

    const auto missing =
        ParseCommandLineOptions(Arguments{"pumpit1", "--post-shader"});
    const auto empty =
        ParseCommandLineOptions(Arguments{"--post-shader=", "pumpit1"});
    const bool errors_ok = !missing.error.empty() && !empty.error.empty();

    // Unknown arguments keep their order, and `--` hands the rest over as is.
    const auto passthrough = ParseCommandLineOptions(Arguments{
        "C:\\games\\PIU.EXE", "--other", "--", "--post-shader", "crt"});
    const bool passthrough_ok = passthrough.error.empty() &&
        !passthrough.has_post_shader &&
        passthrough.positional ==
            Arguments{"C:\\games\\PIU.EXE", "--other", "--post-shader", "crt"};

    return after_ok && before_ok && repeated_ok && none_ok &&
        only_option_ok && errors_ok && passthrough_ok;
}

}  // namespace

bool RunLauncherProbe()
{
    const bool states_ok = ProbeCatalogStates();
    const bool coverage_ok = ProbeCatalogCoverage();
    const bool settings_ok = ProbeSettingsRoundTrip();
    const bool precedence_ok = ProbeEnvironmentPrecedence();
    const bool command_line_ok = ProbeChildCommandLine();
    const bool options_ok = ProbeCommandLineOptions();
    const bool all = states_ok && coverage_ok && settings_ok &&
        precedence_ok && command_line_ok && options_ok;
    std::cout << "launcher_catalog_states=" << (states_ok ? "true" : "false")
              << "\nlauncher_catalog_coverage="
              << (coverage_ok ? "true" : "false")
              << "\nlauncher_settings_round_trip="
              << (settings_ok ? "true" : "false")
              << "\nlauncher_environment_precedence="
              << (precedence_ok ? "true" : "false")
              << "\nlauncher_child_command_line="
              << (command_line_ok ? "true" : "false")
              << "\nlauncher_command_line_options="
              << (options_ok ? "true" : "false")
              << "\nlauncher_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
