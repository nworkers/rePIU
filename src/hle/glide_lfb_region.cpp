#include "repiu/hle/glide_lfb_region.h"

#include <cstring>

namespace repiu::hle
{
namespace
{

// One clipped rectangle plus the row pitches both sides use. Computing it once
// keeps the read and the write path from disagreeing about what "clipped" means.
struct RegionPlan
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::size_t surface_row_pitch = 0;
    std::size_t external_row_pitch = 0;
    bool valid = false;
};

RegionPlan PlanRegion(std::uint32_t x,
                      std::uint32_t y,
                      std::uint32_t width,
                      std::uint32_t height,
                      std::uint32_t bytes_per_pixel,
                      std::int32_t external_stride_bytes,
                      const GlideLfbSurface& surface)
{
    RegionPlan plan;
    const std::uint32_t surface_width = surface.width();
    const std::uint32_t surface_height = surface.height();
    if (surface_width == 0U || surface_height == 0U || width == 0U ||
        height == 0U || bytes_per_pixel == 0U)
    {
        return plan;
    }
    // A negative stride is a bottom-up image whose rows precede `data`, which
    // would move the readable span outside the buffer the caller validated and
    // copied. Nothing observed uses one, so decline instead of guessing.
    if (external_stride_bytes < 0)
    {
        return plan;
    }
    if (x >= surface_width || y >= surface_height)
    {
        return plan;
    }
    plan.width = (x + width > surface_width) ? (surface_width - x) : width;
    plan.height = (y + height > surface_height) ? (surface_height - y) : height;
    plan.surface_row_pitch =
        static_cast<std::size_t>(surface_width) * kGlideLfb565BytesPerTexel;
    plan.external_row_pitch = static_cast<std::size_t>(
        ResolveGlideLfbRegionStride(width, bytes_per_pixel,
                                    external_stride_bytes));
    plan.valid = plan.width != 0U && plan.height != 0U &&
        plan.external_row_pitch != 0U;
    return plan;
}

// Byte offsets of red, green, and blue inside a little-endian 32-bit source
// word. GrColorFormat_t names the word from the most significant byte down, so
// ARGB stores blue first in memory and ABGR stores red first. 888 uses the same
// layout with the alpha slot as padding.
struct ChannelOffsets
{
    std::size_t red;
    std::size_t green;
    std::size_t blue;
};

ChannelOffsets ResolveChannelOffsets(std::uint32_t color_format)
{
    switch (color_format)
    {
        case kGlideColorFormatAbgr:
            return {0U, 1U, 2U};
        case kGlideColorFormatRgba:
            return {3U, 2U, 1U};
        case kGlideColorFormatBgra:
            return {1U, 2U, 3U};
        case kGlideColorFormatArgb:
        default:
            return {2U, 1U, 0U};
    }
}

// GrLfbSrcFmt_t pixels are little-endian packed words whose channel order
// follows `src_color_format`, exactly as GrColor_t does elsewhere in Glide.
// Alpha and the padding byte of 888 carry nothing a 565 destination can hold,
// so both are dropped.
std::uint16_t ConvertSourcePixelTo565(const std::uint8_t* pixel,
                                      std::uint32_t src_format,
                                      std::uint32_t src_color_format,
                                      std::uint32_t surface_color_format)
{
    std::uint32_t red = 0;
    std::uint32_t green = 0;
    std::uint32_t blue = 0;
    const bool source_is_bgr = GlideColorFormatUsesBgrOrder(src_color_format);
    switch (src_format)
    {
        case kGlideLfbSrcFmt565:
        {
            const std::uint32_t texel = static_cast<std::uint32_t>(pixel[0]) |
                (static_cast<std::uint32_t>(pixel[1]) << 8U);
            const std::uint8_t high = ExpandGlideChannel5(texel >> 11U);
            const std::uint8_t low = ExpandGlideChannel5(texel);
            red = source_is_bgr ? low : high;
            green = ExpandGlideChannel6(texel >> 5U);
            blue = source_is_bgr ? high : low;
            break;
        }
        case kGlideLfbSrcFmt555:
        case kGlideLfbSrcFmt1555:
        {
            const std::uint32_t texel = static_cast<std::uint32_t>(pixel[0]) |
                (static_cast<std::uint32_t>(pixel[1]) << 8U);
            const std::uint8_t high = ExpandGlideChannel5(texel >> 10U);
            const std::uint8_t low = ExpandGlideChannel5(texel);
            red = source_is_bgr ? low : high;
            green = ExpandGlideChannel5(texel >> 5U);
            blue = source_is_bgr ? high : low;
            break;
        }
        case kGlideLfbSrcFmt888:
        case kGlideLfbSrcFmt8888:
        default:
        {
            const ChannelOffsets offsets =
                ResolveChannelOffsets(src_color_format);
            red = pixel[offsets.red];
            green = pixel[offsets.green];
            blue = pixel[offsets.blue];
            break;
        }
    }
    return PackGlideTexel565(red, green, blue, surface_color_format);
}

}  // namespace

std::uint32_t GlideLfbSrcFormatBytesPerPixel(std::uint32_t src_format)
{
    switch (src_format)
    {
        case kGlideLfbSrcFmt565:
        case kGlideLfbSrcFmt555:
        case kGlideLfbSrcFmt1555:
        case kGlideLfbSrcFmtZa16:
            return 2U;
        // Glide keeps 888 at one 32-bit word per pixel (0RGB).
        case kGlideLfbSrcFmt888:
        case kGlideLfbSrcFmt8888:
        case kGlideLfbSrcFmt565Depth:
        case kGlideLfbSrcFmt555Depth:
        case kGlideLfbSrcFmt1555Depth:
            return 4U;
        default:
            return 0U;
    }
}

bool GlideLfbSrcFormatSupported(std::uint32_t src_format)
{
    switch (src_format)
    {
        case kGlideLfbSrcFmt565:
        case kGlideLfbSrcFmt555:
        case kGlideLfbSrcFmt1555:
        case kGlideLfbSrcFmt888:
        case kGlideLfbSrcFmt8888:
            return true;
        default:
            return false;
    }
}

std::int32_t ResolveGlideLfbRegionStride(std::uint32_t width,
                                         std::uint32_t bytes_per_pixel,
                                         std::int32_t stride)
{
    if (stride != 0)
    {
        return stride;
    }
    return static_cast<std::int32_t>(width * bytes_per_pixel);
}

bool WriteGlideLfbRegion(std::uint32_t dst_x,
                         std::uint32_t dst_y,
                         std::uint32_t src_width,
                         std::uint32_t src_height,
                         std::uint32_t src_format,
                         std::int32_t src_stride_bytes,
                         const std::uint8_t* src_data,
                         std::size_t src_data_byte_count,
                         std::uint32_t src_color_format,
                         std::uint32_t surface_color_format,
                         GlideLfbSurface* surface)
{
    if (src_data == nullptr || surface == nullptr ||
        !GlideLfbSrcFormatSupported(src_format))
    {
        return false;
    }
    const std::uint32_t bytes_per_pixel =
        GlideLfbSrcFormatBytesPerPixel(src_format);
    const RegionPlan plan =
        PlanRegion(dst_x, dst_y, src_width, src_height, bytes_per_pixel,
                   src_stride_bytes, *surface);
    if (!plan.valid)
    {
        return false;
    }
    // The source must cover every pixel actually copied. Checking the clipped
    // rectangle rather than the requested one keeps a buffer sized to what the
    // hardware would transfer from being refused.
    const std::size_t required =
        (static_cast<std::size_t>(plan.height) - 1U) * plan.external_row_pitch +
        static_cast<std::size_t>(plan.width) * bytes_per_pixel;
    if (src_data_byte_count < required)
    {
        return false;
    }
    std::uint8_t* destination = surface->pixels();
    for (std::uint32_t row = 0; row < plan.height; ++row)
    {
        const std::uint8_t* source =
            src_data + static_cast<std::size_t>(row) * plan.external_row_pitch;
        std::uint8_t* target = destination +
            static_cast<std::size_t>(dst_y + row) * plan.surface_row_pitch +
            static_cast<std::size_t>(dst_x) * kGlideLfb565BytesPerTexel;
        for (std::uint32_t column = 0; column < plan.width; ++column)
        {
            const std::uint16_t texel = ConvertSourcePixelTo565(
                source + static_cast<std::size_t>(column) * bytes_per_pixel,
                src_format, src_color_format, surface_color_format);
            target[column * kGlideLfb565BytesPerTexel] =
                static_cast<std::uint8_t>(texel);
            target[column * kGlideLfb565BytesPerTexel + 1U] =
                static_cast<std::uint8_t>(texel >> 8U);
        }
    }
    return true;
}

bool ReadGlideLfbRegion(std::uint32_t src_x,
                        std::uint32_t src_y,
                        std::uint32_t src_width,
                        std::uint32_t src_height,
                        std::int32_t dst_stride_bytes,
                        const GlideLfbSurface& surface,
                        std::uint8_t* dst_data,
                        std::size_t dst_data_byte_count,
                        std::uint32_t* copied_width,
                        std::uint32_t* copied_height)
{
    if (copied_width != nullptr)
    {
        *copied_width = 0U;
    }
    if (copied_height != nullptr)
    {
        *copied_height = 0U;
    }
    if (dst_data == nullptr)
    {
        return false;
    }
    const RegionPlan plan =
        PlanRegion(src_x, src_y, src_width, src_height,
                   kGlideLfb565BytesPerTexel, dst_stride_bytes, surface);
    if (!plan.valid)
    {
        return false;
    }
    const std::size_t required =
        (static_cast<std::size_t>(plan.height) - 1U) * plan.external_row_pitch +
        static_cast<std::size_t>(plan.width) * kGlideLfb565BytesPerTexel;
    if (dst_data_byte_count < required)
    {
        return false;
    }
    const std::uint8_t* source_pixels = surface.pixels();
    for (std::uint32_t row = 0; row < plan.height; ++row)
    {
        const std::uint8_t* source = source_pixels +
            static_cast<std::size_t>(src_y + row) * plan.surface_row_pitch +
            static_cast<std::size_t>(src_x) * kGlideLfb565BytesPerTexel;
        std::uint8_t* target =
            dst_data + static_cast<std::size_t>(row) * plan.external_row_pitch;
        std::memcpy(target, source,
                    static_cast<std::size_t>(plan.width) *
                        kGlideLfb565BytesPerTexel);
    }
    if (copied_width != nullptr)
    {
        *copied_width = plan.width;
    }
    if (copied_height != nullptr)
    {
        *copied_height = plan.height;
    }
    return true;
}

}  // namespace repiu::hle
