#include "glide_lfb_region_probe.h"

#include "repiu/hle/glide_lfb_region.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace repiu::tools
{
namespace
{

using repiu::hle::GlideLfbSurface;

constexpr std::uint32_t kArgb = repiu::hle::kGlideColorFormatArgb;
constexpr std::uint32_t kAbgr = repiu::hle::kGlideColorFormatAbgr;

// Little-endian source pixels as GrLfbSrcFmt_t defines them.
std::vector<std::uint8_t> Pixel16(std::uint16_t value)
{
    return {static_cast<std::uint8_t>(value),
            static_cast<std::uint8_t>(value >> 8U)};
}

std::vector<std::uint8_t> Pixel32(std::uint8_t alpha, std::uint8_t red,
                                  std::uint8_t green, std::uint8_t blue)
{
    return {blue, green, red, alpha};
}

std::uint16_t SurfaceTexel(const GlideLfbSurface& surface, std::uint32_t x,
                           std::uint32_t y)
{
    const std::size_t offset =
        (static_cast<std::size_t>(y) * surface.width() + x) *
        repiu::hle::kGlideLfb565BytesPerTexel;
    const std::uint8_t* pixels = surface.pixels();
    return static_cast<std::uint16_t>(
        pixels[offset] |
        (static_cast<std::uint16_t>(pixels[offset + 1U]) << 8U));
}

void SetSurfaceTexel(GlideLfbSurface* surface, std::uint32_t x, std::uint32_t y,
                     std::uint16_t value)
{
    const std::size_t offset =
        (static_cast<std::size_t>(y) * surface->width() + x) *
        repiu::hle::kGlideLfb565BytesPerTexel;
    std::uint8_t* pixels = surface->pixels();
    pixels[offset] = static_cast<std::uint8_t>(value);
    pixels[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
}

// Writes one pixel through the region path and reports the texel it produced.
bool ConvertOnePixel(std::uint32_t src_format,
                     const std::vector<std::uint8_t>& pixel,
                     std::uint32_t src_color_format,
                     std::uint32_t surface_color_format,
                     std::uint16_t* texel)
{
    GlideLfbSurface surface;
    if (!surface.Resize(2U, 2U))
    {
        return false;
    }
    if (!repiu::hle::WriteGlideLfbRegion(0U, 0U, 1U, 1U, src_format, 0,
                                         pixel.data(), pixel.size(),
                                         src_color_format,
                                         surface_color_format, &surface))
    {
        return false;
    }
    *texel = SurfaceTexel(surface, 0U, 0U);
    return true;
}

}  // namespace

bool RunGlideLfbRegionProbe()
{
    using repiu::hle::GlideLfbSrcFormatBytesPerPixel;
    using repiu::hle::GlideLfbSrcFormatSupported;
    using repiu::hle::ReadGlideLfbRegion;
    using repiu::hle::ResolveGlideLfbRegionStride;
    using repiu::hle::WriteGlideLfbRegion;

    // The Glide 2.4 table: 565 is 0, not 1, and 888 keeps one 32-bit word per
    // pixel. Getting this wrong is what made every observed write be declined.
    const bool format_table =
        repiu::hle::kGlideLfbSrcFmt565 == 0x00U &&
        repiu::hle::kGlideLfbSrcFmt555 == 0x01U &&
        repiu::hle::kGlideLfbSrcFmt1555 == 0x02U &&
        repiu::hle::kGlideLfbSrcFmt888 == 0x04U &&
        repiu::hle::kGlideLfbSrcFmt8888 == 0x05U &&
        GlideLfbSrcFormatBytesPerPixel(repiu::hle::kGlideLfbSrcFmt565) == 2U &&
        GlideLfbSrcFormatBytesPerPixel(repiu::hle::kGlideLfbSrcFmt555) == 2U &&
        GlideLfbSrcFormatBytesPerPixel(repiu::hle::kGlideLfbSrcFmt1555) == 2U &&
        GlideLfbSrcFormatBytesPerPixel(repiu::hle::kGlideLfbSrcFmt888) == 4U &&
        GlideLfbSrcFormatBytesPerPixel(repiu::hle::kGlideLfbSrcFmt8888) == 4U &&
        GlideLfbSrcFormatBytesPerPixel(0x81U) == 0U &&
        GlideLfbSrcFormatSupported(repiu::hle::kGlideLfbSrcFmt565) &&
        GlideLfbSrcFormatSupported(repiu::hle::kGlideLfbSrcFmt8888) &&
        !GlideLfbSrcFormatSupported(repiu::hle::kGlideLfbSrcFmt565Depth) &&
        !GlideLfbSrcFormatSupported(repiu::hle::kGlideLfbSrcFmtZa16) &&
        !GlideLfbSrcFormatSupported(repiu::hle::kGlideLfbSrcFmtRle16);

    // PIU passes stride 0 on single-row transfers, where the stride is never
    // consulted. Packed rows are the only reading that transfers anything.
    const bool stride_rule =
        ResolveGlideLfbRegionStride(640U, 4U, 0) == 2560 &&
        ResolveGlideLfbRegionStride(640U, 2U, 0) == 1280 &&
        ResolveGlideLfbRegionStride(640U, 4U, 4096) == 4096 &&
        ResolveGlideLfbRegionStride(640U, 4U, -2560) == -2560;

    // One saturated channel per format, checked against the 565 packing.
    std::uint16_t texel_565 = 0;
    std::uint16_t texel_555 = 0;
    std::uint16_t texel_1555 = 0;
    std::uint16_t texel_888 = 0;
    std::uint16_t texel_8888 = 0;
    std::uint16_t texel_green = 0;
    const bool conversion =
        ConvertOnePixel(repiu::hle::kGlideLfbSrcFmt565, Pixel16(0xF800U), kArgb,
                        kArgb, &texel_565) &&
        texel_565 == 0xF800U &&
        ConvertOnePixel(repiu::hle::kGlideLfbSrcFmt555, Pixel16(0x7C00U), kArgb,
                        kArgb, &texel_555) &&
        texel_555 == 0xF800U &&
        ConvertOnePixel(repiu::hle::kGlideLfbSrcFmt1555, Pixel16(0xFC00U),
                        kArgb, kArgb, &texel_1555) &&
        texel_1555 == 0xF800U &&
        ConvertOnePixel(repiu::hle::kGlideLfbSrcFmt888,
                        Pixel32(0x00U, 0xFFU, 0x00U, 0x00U), kArgb, kArgb,
                        &texel_888) &&
        texel_888 == 0xF800U &&
        ConvertOnePixel(repiu::hle::kGlideLfbSrcFmt8888,
                        Pixel32(0xFFU, 0xFFU, 0x00U, 0x00U), kArgb, kArgb,
                        &texel_8888) &&
        texel_8888 == 0xF800U &&
        ConvertOnePixel(repiu::hle::kGlideLfbSrcFmt8888,
                        Pixel32(0xFFU, 0x00U, 0xFFU, 0x00U), kArgb, kArgb,
                        &texel_green) &&
        texel_green == 0x07E0U;

    // The source word follows grLfbWriteColorFormat, not a fixed RGB order.
    // PIU is ABGR on both sides, where the identity below is what keeps red and
    // blue from swapping; the cross cases prove the two formats are independent.
    std::uint16_t texel_abgr_pair = 0;
    std::uint16_t texel_abgr_source = 0;
    std::uint16_t texel_abgr_surface = 0;
    std::uint16_t texel_abgr_565 = 0;
    const std::vector<std::uint8_t> abgr_red = {0xFFU, 0x00U, 0x00U, 0xFFU};
    const bool color_order =
        // ABGR source into an ABGR frame buffer: bytes pass through, and the
        // red byte lands in the low five bits the way an ABGR buffer stores it.
        ConvertOnePixel(repiu::hle::kGlideLfbSrcFmt8888, abgr_red, kAbgr, kAbgr,
                        &texel_abgr_pair) &&
        texel_abgr_pair == 0x001FU &&
        // Same bytes read as ARGB name that byte blue, so it packs high.
        ConvertOnePixel(repiu::hle::kGlideLfbSrcFmt8888, abgr_red, kArgb, kAbgr,
                        &texel_abgr_source) &&
        texel_abgr_source == 0xF800U &&
        // An RGB-ordered source into an ABGR buffer must be translated.
        ConvertOnePixel(repiu::hle::kGlideLfbSrcFmt8888,
                        Pixel32(0xFFU, 0xFFU, 0x00U, 0x00U), kArgb, kAbgr,
                        &texel_abgr_surface) &&
        texel_abgr_surface == 0x001FU &&
        // 16-bit sources follow the same rule: an ABGR 565 word is BGR565.
        ConvertOnePixel(repiu::hle::kGlideLfbSrcFmt565, Pixel16(0xF800U), kAbgr,
                        kAbgr, &texel_abgr_565) &&
        texel_abgr_565 == 0xF800U;

    // A rectangle running off the edge is clipped, and nothing outside it moves.
    GlideLfbSurface clipped;
    clipped.Resize(4U, 4U);
    std::memset(clipped.pixels(), 0, clipped.byte_count());
    std::vector<std::uint8_t> block(4U * 4U * 4U, 0U);
    for (std::size_t index = 0; index < 16U; ++index)
    {
        block[index * 4U + 2U] = 0xFFU;  // red
    }
    const bool clipping =
        WriteGlideLfbRegion(2U, 2U, 4U, 4U, repiu::hle::kGlideLfbSrcFmt8888, 0,
                            block.data(), block.size(), kArgb, kArgb,
                            &clipped) &&
        SurfaceTexel(clipped, 2U, 2U) == 0xF800U &&
        SurfaceTexel(clipped, 3U, 3U) == 0xF800U &&
        SurfaceTexel(clipped, 1U, 2U) == 0x0000U &&
        SurfaceTexel(clipped, 2U, 1U) == 0x0000U &&
        // Fully outside is refused rather than clamped into the surface.
        !WriteGlideLfbRegion(4U, 0U, 1U, 1U, repiu::hle::kGlideLfbSrcFmt8888, 0,
                             block.data(), block.size(), kArgb, kArgb,
                             &clipped) &&
        // A short source buffer must not be read past.
        !WriteGlideLfbRegion(0U, 0U, 2U, 2U, repiu::hle::kGlideLfbSrcFmt8888, 0,
                             block.data(), 8U, kArgb, kArgb, &clipped) &&
        // Bottom-up sources are declined instead of read backwards.
        !WriteGlideLfbRegion(0U, 0U, 2U, 2U, repiu::hle::kGlideLfbSrcFmt8888,
                             -16, block.data(), block.size(), kArgb, kArgb,
                             &clipped);

    // Write then read the same rectangle: the 565 texels come back unchanged
    // and a wider destination stride leaves its padding alone.
    GlideLfbSurface round;
    round.Resize(8U, 4U);
    std::memset(round.pixels(), 0, round.byte_count());
    std::vector<std::uint8_t> source;
    for (std::uint32_t index = 0; index < 4U; ++index)
    {
        const std::vector<std::uint8_t> pixel = Pixel32(
            0xFFU, static_cast<std::uint8_t>(index * 0x40U), 0x00U, 0x00U);
        source.insert(source.end(), pixel.begin(), pixel.end());
    }
    constexpr std::size_t kDestinationStride = 8U;
    std::vector<std::uint8_t> destination(kDestinationStride * 2U, 0xAAU);
    std::uint32_t copied_width = 0;
    std::uint32_t copied_height = 0;
    const bool round_trip =
        WriteGlideLfbRegion(1U, 1U, 2U, 2U, repiu::hle::kGlideLfbSrcFmt8888, 0,
                            source.data(), source.size(), kArgb, kArgb,
                            &round) &&
        ReadGlideLfbRegion(1U, 1U, 2U, 2U,
                           static_cast<std::int32_t>(kDestinationStride), round,
                           destination.data(), destination.size(),
                           &copied_width, &copied_height) &&
        copied_width == 2U && copied_height == 2U &&
        destination[0] == static_cast<std::uint8_t>(SurfaceTexel(round, 1U, 1U)) &&
        destination[1] ==
            static_cast<std::uint8_t>(SurfaceTexel(round, 1U, 1U) >> 8U) &&
        destination[kDestinationStride] ==
            static_cast<std::uint8_t>(SurfaceTexel(round, 1U, 2U)) &&
        // Bytes 4..7 of each row are padding the transfer must not touch.
        destination[4] == 0xAAU && destination[5] == 0xAAU &&
        destination[6] == 0xAAU && destination[7] == 0xAAU &&
        destination[kDestinationStride + 4U] == 0xAAU;

    // Region row indices are native frame buffer rows, not origin-relative
    // ones: `y` addresses the surface directly at both ends. Mirroring them the
    // way a lock's origin would is what rendered pumpit8's full-screen LFB pass
    // upside down under its GR_ORIGIN_LOWER_LEFT window.
    GlideLfbSurface rows;
    rows.Resize(2U, 4U);
    std::memset(rows.pixels(), 0, rows.byte_count());
    SetSurfaceTexel(&rows, 0U, 0U, 0x1234U);
    SetSurfaceTexel(&rows, 1U, 0U, 0x5678U);
    std::vector<std::uint8_t> row_destination(4U, 0U);
    const std::vector<std::uint8_t> row_source =
        Pixel32(0xFFU, 0xFFU, 0x00U, 0x00U);
    const bool row_mapping =
        ReadGlideLfbRegion(0U, 0U, 2U, 1U, 0, rows, row_destination.data(),
                           row_destination.size(), nullptr, nullptr) &&
        row_destination[0] == 0x34U && row_destination[1] == 0x12U &&
        row_destination[2] == 0x78U && row_destination[3] == 0x56U &&
        // Row 3 is the last surface row, not the first.
        ReadGlideLfbRegion(0U, 3U, 1U, 1U, 0, rows, row_destination.data(),
                           row_destination.size(), nullptr, nullptr) &&
        row_destination[0] == 0x00U && row_destination[1] == 0x00U &&
        WriteGlideLfbRegion(0U, 3U, 1U, 1U, repiu::hle::kGlideLfbSrcFmt8888, 0,
                            row_source.data(), row_source.size(), kArgb, kArgb,
                            &rows) &&
        SurfaceTexel(rows, 0U, 3U) == 0xF800U &&
        SurfaceTexel(rows, 0U, 0U) == 0x1234U;

    const bool all = format_table && stride_rule && conversion &&
        color_order && clipping && round_trip && row_mapping;

    std::cout << "glide_lfb_region_format_table="
              << (format_table ? "true" : "false")
              << "\nglide_lfb_region_stride_rule="
              << (stride_rule ? "true" : "false")
              << "\nglide_lfb_region_conversion="
              << (conversion ? "true" : "false")
              << "\nglide_lfb_region_color_order="
              << (color_order ? "true" : "false")
              << "\nglide_lfb_region_clipping="
              << (clipping ? "true" : "false")
              << "\nglide_lfb_region_round_trip="
              << (round_trip ? "true" : "false")
              << "\nglide_lfb_region_row_mapping="
              << (row_mapping ? "true" : "false")
              << "\nglide_lfb_region_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
