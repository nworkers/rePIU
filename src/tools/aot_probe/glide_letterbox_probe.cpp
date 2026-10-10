#include "glide_letterbox_probe.h"

#include "repiu/engine/glide_letterbox.h"

#include <iostream>

namespace repiu::tools
{
namespace
{

bool Is(const engine::GlideLetterboxRect& rect, std::uint32_t x,
        std::uint32_t y, std::uint32_t width, std::uint32_t height)
{
    return rect.x == x && rect.y == y && rect.width == width &&
        rect.height == height;
}

}  // namespace

bool RunGlideLetterboxProbe()
{
    using engine::ComputeGlideLetterboxRect;

    // The guest's own ratio fills the drawable at any scale.
    const bool same_ratio =
        Is(ComputeGlideLetterboxRect(640U, 480U, 1280U, 960U), 0U, 0U, 1280U,
           960U) &&
        Is(ComputeGlideLetterboxRect(640U, 480U, 640U, 480U), 0U, 0U, 640U,
           480U);

    // A 16:9 fullscreen pillarboxes a 4:3 picture: 1440x1080 at x=240.
    const bool pillarbox =
        Is(ComputeGlideLetterboxRect(640U, 480U, 1920U, 1080U), 240U, 0U,
           1440U, 1080U) &&
        Is(ComputeGlideLetterboxRect(640U, 480U, 2560U, 1440U), 320U, 0U,
           1920U, 1440U);

    // A tall window letterboxes it.
    const bool letterbox =
        Is(ComputeGlideLetterboxRect(640U, 480U, 1280U, 1600U), 0U, 320U,
           1280U, 960U);

    // Odd leftovers: 1001 - 1000 leaves one column, which goes to the right;
    // 641x481 rounds 640.75 to 641 and leaves nothing.
    const bool odd =
        Is(ComputeGlideLetterboxRect(640U, 480U, 1001U, 750U), 0U, 0U, 1000U,
           750U) &&
        Is(ComputeGlideLetterboxRect(4U, 3U, 7U, 3U), 1U, 0U, 4U, 3U) &&
        Is(ComputeGlideLetterboxRect(640U, 480U, 641U, 481U), 0U, 0U, 641U,
           481U);

    // Degenerate sizes: no drawable is all zero; no logical size fills.
    const bool degenerate =
        Is(ComputeGlideLetterboxRect(640U, 480U, 0U, 480U), 0U, 0U, 0U, 0U) &&
        Is(ComputeGlideLetterboxRect(0U, 0U, 800U, 600U), 0U, 0U, 800U,
           600U) &&
        Is(ComputeGlideLetterboxRect(640U, 480U, 1U, 1000U), 0U, 499U, 1U,
           1U);

    // Issue #45: with keep_aspect off the picture fills any drawable; on, it
    // is the letterbox rectangle above.
    using engine::ComputeGlidePictureRect;
    const bool stretch =
        Is(ComputeGlidePictureRect(640U, 480U, 1920U, 1080U, false), 0U, 0U,
           1920U, 1080U) &&
        Is(ComputeGlidePictureRect(640U, 480U, 1280U, 1600U, false), 0U, 0U,
           1280U, 1600U) &&
        Is(ComputeGlidePictureRect(640U, 480U, 0U, 480U, false), 0U, 0U, 0U,
           0U) &&
        Is(ComputeGlidePictureRect(640U, 480U, 1920U, 1080U, true), 240U, 0U,
           1440U, 1080U);

    const bool all = same_ratio && pillarbox && letterbox && odd &&
        degenerate && stretch;
    std::cout << "glide_letterbox_same_ratio=" << (same_ratio ? "true" : "false")
              << "\nglide_letterbox_pillarbox=" << (pillarbox ? "true" : "false")
              << "\nglide_letterbox_letterbox=" << (letterbox ? "true" : "false")
              << "\nglide_letterbox_odd=" << (odd ? "true" : "false")
              << "\nglide_letterbox_degenerate="
              << (degenerate ? "true" : "false")
              << "\nglide_letterbox_stretch=" << (stretch ? "true" : "false")
              << "\nglide_letterbox_all=" << (all ? "true" : "false")
              << std::endl;
    return all;
}

}  // namespace repiu::tools
