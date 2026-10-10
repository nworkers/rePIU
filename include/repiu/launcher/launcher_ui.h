#ifndef REPIU_LAUNCHER_LAUNCHER_UI_H_
#define REPIU_LAUNCHER_LAUNCHER_UI_H_

#include "repiu/launcher/launcher_settings.h"
#include "repiu/launcher/rom_set_catalog.h"

#include <string>
#include <vector>

namespace repiu::update
{
class LauncherUpdater;
}  // namespace repiu::update

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
    // Issue #48: the operator chose to update and the new release is staged;
    // the caller installs it and restarts.
    bool install_update = false;
    // Issue #52: the window closed because LT+RT+L3+R3 was held for a second.
    bool closed_by_exit_chord = false;
};

// Opens the launcher window, runs until the operator starts a ROM set or
// closes it, and tears the window down before returning so the guest can create
// its own. Issue #48: with an updater, a newer release is announced at the
// top, and the window closes with `install_update` once one is staged.
LauncherUiResult RunLauncherUi(const std::vector<RomSetEntry>& catalog,
                               const LauncherSettings& initial_settings,
                               update::LauncherUpdater* updater = nullptr);

}  // namespace repiu::launcher

#endif  // REPIU_LAUNCHER_LAUNCHER_UI_H_
