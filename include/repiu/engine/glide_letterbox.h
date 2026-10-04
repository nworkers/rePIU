#ifndef REPIU_ENGINE_GLIDE_LETTERBOX_H_
#define REPIU_ENGINE_GLIDE_LETTERBOX_H_

#include <cstdint>

namespace repiu::engine
{

// Task 769. Where the guest's picture goes inside the window's drawable: the
// largest rectangle of the logical screen's aspect ratio that fits, centred,
// in OpenGL's bottom-left pixel coordinates. The rest of the drawable is the
// black letterbox or pillarbox bars.
struct GlideLetterboxRect
{
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

// Scales by min(drawable / logical) on both axes, rounding to the nearest
// pixel, and splits the leftover evenly with the odd pixel on the right and
// top. A zero logical size fills the drawable; a zero drawable is all zero.
[[nodiscard]] GlideLetterboxRect ComputeGlideLetterboxRect(
    std::uint32_t logical_width, std::uint32_t logical_height,
    std::uint32_t drawable_width, std::uint32_t drawable_height);

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_GLIDE_LETTERBOX_H_
