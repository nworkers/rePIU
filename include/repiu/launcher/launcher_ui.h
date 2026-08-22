#ifndef REPIU_LAUNCHER_LAUNCHER_UI_H_
#define REPIU_LAUNCHER_LAUNCHER_UI_H_

#include "repiu/launcher/launcher_settings.h"
#include "repiu/launcher/rom_set_catalog.h"

#include <string>
#include <vector>

namespace repiu::launcher
{

struct LauncherUiResult
{
    // False when the window was closed or the run was cancelled, in which case
    // the caller exits without starting a guest.
    bool launch = false;
    std::string rom_set_id;
    LauncherSettings settings;
    // True when the settings differ from what was loaded, so the caller writes
    // them back only when the operator actually changed something.
    bool settings_changed = false;
    // Set when the window or GL context could not be created; the caller falls
    // back to its previous no-argument behavior rather than exiting.
    bool unavailable = false;
    std::string message;
};

// Opens the launcher window, runs until the operator starts a ROM set or
// closes it, and tears the window down before returning so the guest can create
// its own.
LauncherUiResult RunLauncherUi(const std::vector<RomSetEntry>& catalog,
                               const LauncherSettings& initial_settings);

}  // namespace repiu::launcher

#endif  // REPIU_LAUNCHER_LAUNCHER_UI_H_
