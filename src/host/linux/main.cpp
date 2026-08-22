// Task 502. The Linux launcher entry point.
//
// This is Stage 2 of the port: the launcher runs, lists the ROM sets it finds,
// and stores the operator's settings. There is no execution engine on Linux
// yet, so a selection is reported rather than started -- saying so plainly is
// better than spawning something that cannot exist and letting the failure
// surface as a missing file.
//
// The Windows entry point keeps its own loop and process spawning, because both
// are tied to Win32 specifics: the child that runs a game, and the address-space
// ordering that forced it into a separate process. What the two share is the
// launcher itself, which is platform-neutral and used here directly.

#include "repiu/launcher/launcher_settings.h"
#include "repiu/launcher/launcher_ui.h"
#include "repiu/launcher/rom_set_catalog.h"

#include <chrono>
#include <filesystem>
#include <iostream>

int main()
{
    const std::filesystem::path config_directory("cfg");
    const std::filesystem::path roms_root("roms");

    repiu::launcher::LauncherSettingsLoad stored =
        repiu::launcher::LoadLauncherSettings(config_directory);
    for (const std::string& warning : stored.warnings)
    {
        std::cerr << "warning: " << warning << "\n";
    }

    const std::vector<repiu::launcher::RomSetEntry> catalog =
        repiu::launcher::BuildRomSetCatalog(roms_root);
    std::size_t runnable = 0;
    for (const repiu::launcher::RomSetEntry& entry : catalog)
    {
        if (repiu::launcher::IsRomSetRunnable(entry))
        {
            ++runnable;
        }
    }
    std::cout << "rom sets: " << catalog.size() << " listed, " << runnable
              << " runnable\n";

    // Bring-up instrumentation: how long the window actually lived. A launcher
    // that closes instantly and one that never appears look identical from
    // outside the process.
    const auto ui_start = std::chrono::steady_clock::now();
    const repiu::launcher::LauncherUiResult chosen =
        repiu::launcher::RunLauncherUi(catalog, stored.settings);
    std::cout << "launcher ui ran for "
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - ui_start)
                     .count()
              << " s\n";
    if (chosen.unavailable)
    {
        std::cerr << "launcher unavailable: " << chosen.message << "\n";
        return 1;
    }
    if (chosen.settings_changed &&
        !repiu::launcher::SaveLauncherSettings(config_directory,
                                               chosen.settings))
    {
        std::cerr << "warning: failed to write "
                  << repiu::launcher::LauncherSettingsPath(config_directory)
                         .string()
                  << "\n";
    }
    if (!chosen.launch)
    {
        std::cout << "launcher closed without starting a ROM set\n";
        return 0;
    }

    // Stage 3 replaces this with the ported execution engine.
    std::cout << "selected rom set: " << chosen.rom_set_id << "\n"
              << "the execution engine is not available on Linux yet, so "
                 "nothing was started\n";
    return 0;
}
