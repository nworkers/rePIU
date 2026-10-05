#ifndef REPIU_ENGINE_IMGUI_UI_SCALE_H_
#define REPIU_ENGINE_IMGUI_UI_SCALE_H_

struct ImGuiStyle;

namespace repiu::engine
{

// #15. How big the Dear ImGui layers draw: the in-game OSD and the launcher.
// Text and spacing follow the window height, so enlarging the window -- a 2x
// game window, fullscreen on a 4K display, a maximised launcher -- enlarges
// them with the picture instead of leaving them at ImGui's fixed 13 px.
// See docs/design/20261006-i015-osd-re2dj-layout.md.
struct UiScaleRule
{
    float reference_height = 480.0F;
    float factor = 1.0F;
    float minimum = 1.0F;
};

// re2DJ's OSD rule: the game's logical screen is 480 lines, and the text stays
// a little under one font pixel per logical pixel so the overlay keeps clear
// of the picture. 1.5 in a 2x window, 3.375 in 4K fullscreen.
inline constexpr UiScaleRule kOsdUiScale{480.0F, 0.75F, 0.75F};
// The launcher opens 640 high, which is the size it was laid out at.
inline constexpr UiScaleRule kLauncherUiScale{640.0F, 1.0F, 1.0F};

// max(minimum, height / reference_height * factor). A non-positive height (a
// minimised window) gives the minimum.
[[nodiscard]] float UiScaleForHeight(float height, const UiScaleRule& rule);

// Replaces the current ImGui style with `base` scaled by `scale`: every size
// through ScaleAllSizes, and the text through FontScaleMain. Starting from the
// base each time keeps rounding from piling up when the scale changes back and
// forth. Call with the ImGui context current, before NewFrame.
void ApplyImGuiUiScale(const ImGuiStyle& base, float scale);

// Adds the font both layers draw with: ProggyForever, the scalable cut of
// ImGui's ProggyClean, which is what re2DJ's OSD shows at its usual sizes.
// Chosen outright rather than through AddFontDefault's size heuristic, so a
// small window does not fall back to the bitmap face. Call once, right after
// creating the ImGui context.
void AddImGuiUiFont();

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_IMGUI_UI_SCALE_H_
