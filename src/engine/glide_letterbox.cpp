#include "repiu/engine/glide_letterbox.h"

#include <algorithm>

namespace repiu::engine
{

GlideLetterboxRect ComputeGlideLetterboxRect(std::uint32_t logical_width,
                                             std::uint32_t logical_height,
                                             std::uint32_t drawable_width,
                                             std::uint32_t drawable_height)
{
    GlideLetterboxRect rect;
    if (drawable_width == 0U || drawable_height == 0U)
    {
        return rect;
    }
    rect.width = drawable_width;
    rect.height = drawable_height;
    if (logical_width == 0U || logical_height == 0U)
    {
        return rect;
    }
    // Integer cross-multiplication decides which axis binds, so an exact
    // ratio never loses a pixel to floating-point rounding.
    const std::uint64_t wide = static_cast<std::uint64_t>(drawable_width) *
        logical_height;
    const std::uint64_t tall = static_cast<std::uint64_t>(drawable_height) *
        logical_width;
    if (wide > tall)
    {
        // Pillarbox: height binds.
        rect.width = static_cast<std::uint32_t>(
            (tall + logical_height / 2U) / logical_height);
        rect.width = std::clamp<std::uint32_t>(rect.width, 1U, drawable_width);
    }
    else if (wide < tall)
    {
        // Letterbox: width binds.
        rect.height = static_cast<std::uint32_t>(
            (wide + logical_width / 2U) / logical_width);
        rect.height =
            std::clamp<std::uint32_t>(rect.height, 1U, drawable_height);
    }
    rect.x = (drawable_width - rect.width) / 2U;
    rect.y = (drawable_height - rect.height) / 2U;
    return rect;
}

}  // namespace repiu::engine
