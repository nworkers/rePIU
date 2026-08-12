#ifndef REPIU_HLE_GLIDE_LFB_REGION_H_
#define REPIU_HLE_GLIDE_LFB_REGION_H_

#include <cstddef>
#include <cstdint>

#include "repiu/hle/glide_lfb.h"

namespace repiu::hle
{

// grLfbWriteRegion / grLfbReadRegion: rectangle transfers between guest memory
// and the frame buffer that need no lock. Everything here is platform neutral --
// the source format table, the stride rule, clipping, and the pixel conversion
// into the 565 staging surface that stands in for the linear frame buffer.
//
// Reference: 3Dfx Glide 2.4 Reference Manual, grLfbWriteRegion /
// grLfbReadRegion / GrLfbSrcFmt_t.
// https://www.bitsavers.org/components/3dfx/Glide_Reference_Manual_2.4_199707.pdf

// GrLfbSrcFmt_t. These are the manual's values: 565 is 0, not 1. The color
// formats below are implemented; the depth-carrying ones and the run-length
// encoding are listed so an unsupported request can be named rather than only
// counted.
constexpr std::uint32_t kGlideLfbSrcFmt565 = 0x00U;
constexpr std::uint32_t kGlideLfbSrcFmt555 = 0x01U;
constexpr std::uint32_t kGlideLfbSrcFmt1555 = 0x02U;
constexpr std::uint32_t kGlideLfbSrcFmt888 = 0x04U;
constexpr std::uint32_t kGlideLfbSrcFmt8888 = 0x05U;
constexpr std::uint32_t kGlideLfbSrcFmt565Depth = 0x0CU;
constexpr std::uint32_t kGlideLfbSrcFmt555Depth = 0x0DU;
constexpr std::uint32_t kGlideLfbSrcFmt1555Depth = 0x0EU;
constexpr std::uint32_t kGlideLfbSrcFmtZa16 = 0x0FU;
constexpr std::uint32_t kGlideLfbSrcFmtRle16 = 0x80U;

// Bytes one source pixel occupies, or 0 for a format this layer cannot size.
// 888 is four bytes because Glide keeps it 32-bit aligned as 0RGB.
std::uint32_t GlideLfbSrcFormatBytesPerPixel(std::uint32_t src_format);

// True for the color formats WriteGlideLfbRegion converts. Depth-carrying
// formats and RLE16 are sized or variable but not implemented, so they answer
// false and the caller reports an unsupported argument.
bool GlideLfbSrcFormatSupported(std::uint32_t src_format);

// Glide expects a real stride, but a single-row transfer never consults one and
// PIU passes 0 there. A zero stride therefore means the rows are packed:
// `width * bytes-per-pixel`. A negative stride is a bottom-up image and is
// returned unchanged.
std::int32_t ResolveGlideLfbRegionStride(std::uint32_t width,
                                         std::uint32_t bytes_per_pixel,
                                         std::int32_t stride);

// Region coordinates are NOT origin-relative. grLfbLock takes an explicit
// GrOriginLocation_t and grLfbWriteRegion / grLfbReadRegion take none, because
// they address the frame buffer in its native layout, where row 0 is the top.
// Confirmed on pumpit8, whose window is GR_ORIGIN_LOWER_LEFT: mirroring the row
// index the way a lock would renders its full-screen LFB pass upside down.
// Neither entry point here flips, and the staging surface stays top-down.

// Converts a `src_format` rectangle into the surface's 565 texels.
// `src_color_format` is the GrColorFormat_t the source words are packed in --
// grLfbWriteColorFormat's value, defaulting to grSstWinOpen's -- and
// `surface_color_format` is the frame buffer's. Both are needed because the two
// can differ; when they agree the pixels pass through untranslated. The
// rectangle is clipped to the surface.
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
                         GlideLfbSurface* surface);

// Copies a rectangle of the surface out in the frame buffer's native 565 form,
// which is what grLfbReadRegion is defined to return. `dst_stride_bytes`
// follows the same zero rule as the write path. `copied_width` and
// `copied_height` report the clipped rectangle so the caller can forward
// exactly the bytes that were filled and leave inter-row padding alone; both
// may be null.
bool ReadGlideLfbRegion(std::uint32_t src_x,
                        std::uint32_t src_y,
                        std::uint32_t src_width,
                        std::uint32_t src_height,
                        std::int32_t dst_stride_bytes,
                        const GlideLfbSurface& surface,
                        std::uint8_t* dst_data,
                        std::size_t dst_data_byte_count,
                        std::uint32_t* copied_width,
                        std::uint32_t* copied_height);

}  // namespace repiu::hle

#endif  // REPIU_HLE_GLIDE_LFB_REGION_H_
