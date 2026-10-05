#include "imgui_ui_scale_probe.h"

#include "repiu/engine/imgui_ui_scale.h"
#include "repiu/engine/session_identity.h"

#include <cmath>
#include <iostream>

namespace repiu::tools
{
namespace
{

bool Near(float actual, float expected)
{
    return std::fabs(actual - expected) < 1.0e-4F;
}

}  // namespace

// #15. The scale the OSD and the launcher draw at, for the window heights
// they actually meet: the game's 1x, 2x and 4K fullscreen, and the launcher
// at its opening size, doubled, and squeezed below it. Also the target
// profile line's fallback before the loader has written one.
bool RunImGuiUiScaleProbe()
{
    using engine::kLauncherUiScale;
    using engine::kOsdUiScale;
    using engine::UiScaleForHeight;

    const bool osd_ok = Near(UiScaleForHeight(480.0F, kOsdUiScale), 0.75F) &&
        Near(UiScaleForHeight(960.0F, kOsdUiScale), 1.5F) &&
        Near(UiScaleForHeight(2160.0F, kOsdUiScale), 3.375F) &&
        Near(UiScaleForHeight(240.0F, kOsdUiScale), 0.75F) &&
        Near(UiScaleForHeight(0.0F, kOsdUiScale), 0.75F) &&
        Near(UiScaleForHeight(-5.0F, kOsdUiScale), 0.75F) &&
        Near(UiScaleForHeight(NAN, kOsdUiScale), 0.75F);
    const bool launcher_ok =
        Near(UiScaleForHeight(640.0F, kLauncherUiScale), 1.0F) &&
        Near(UiScaleForHeight(1280.0F, kLauncherUiScale), 2.0F) &&
        Near(UiScaleForHeight(320.0F, kLauncherUiScale), 1.0F);

    const bool before = engine::SessionTargetProfile() == "unknown";
    engine::SetSessionTargetProfile("pumpit1");
    const bool written = engine::SessionTargetProfile() == "pumpit1";
    engine::SetSessionTargetProfile("");
    const bool cleared = engine::SessionTargetProfile() == "unknown";
    const bool profile_ok = before && written && cleared;

    const bool all = osd_ok && launcher_ok && profile_ok;
    std::cout << "imgui_ui_scale_osd=" << (osd_ok ? "true" : "false")
              << "\nimgui_ui_scale_launcher="
              << (launcher_ok ? "true" : "false")
              << "\nimgui_ui_scale_target_profile="
              << (profile_ok ? "true" : "false")
              << "\nimgui_ui_scale_all=" << (all ? "true" : "false") << '\n';
    return all;
}

}  // namespace repiu::tools
