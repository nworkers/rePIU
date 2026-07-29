#ifndef REPIU_HLE_GLIDE_LFB_H_
#define REPIU_HLE_GLIDE_LFB_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace repiu::hle
{

// Glide 2.x linear frame buffer (LFB) support.
//
// grLfbLock hands the guest a real pointer it writes pixels into with native
// instructions; grLfbUnlock is where those pixels become a presentable image.
// Everything here is platform-neutral: the staging surface, the GrLfbInfo_t
// wire format, and the pixel conversions. The OpenGL upload lives in the Win32
// backend and the ABI decoding lives in the gate boundary.
//
// Reference: 3Dfx Glide 2.4 Reference Manual, grLfbLock / grLfbUnlock /
// GrLfbInfo_t / GrLfbWriteMode_t.
// https://www.bitsavers.org/components/3dfx/Glide_Reference_Manual_2.4_199707.pdf

// GrLock_t
constexpr std::uint32_t kGlideLfbReadOnly = 0U;
constexpr std::uint32_t kGlideLfbWriteOnly = 1U;

// GrBuffer_t
constexpr std::uint32_t kGlideBufferFrontBuffer = 0U;
constexpr std::uint32_t kGlideBufferBackBuffer = 1U;

// GrLfbWriteMode_t: only 565 is implemented; the observed PIU lock uses it.
constexpr std::uint32_t kGlideLfbWriteMode565 = 0U;

// GrOriginLocation_t
constexpr std::uint32_t kGlideOriginUpperLeft = 0U;
constexpr std::uint32_t kGlideOriginLowerLeft = 1U;

// GrLfbSrcFmt_t
constexpr std::uint32_t kGlideLfbSrcFmt565 = 1U;

// GrColorFormat_t. For 565 LFB locks, ARGB/RGBA select RGB565 while
// ABGR/BGRA select BGR565 (Glide 2.4 Programming Guide, Table 11.2).
constexpr std::uint32_t kGlideColorFormatArgb = 0U;
constexpr std::uint32_t kGlideColorFormatAbgr = 1U;
constexpr std::uint32_t kGlideColorFormatRgba = 2U;
constexpr std::uint32_t kGlideColorFormatBgra = 3U;

// GrLfbInfo_t is five 32-bit fields: size, lfbPtr, strideInBytes, writeMode,
// origin. The caller fills `size` with sizeof(GrLfbInfo_t) before the call.
constexpr std::size_t kGlideLfbInfoByteCount = 20U;
constexpr std::uint32_t kGlideLfbInfoExpectedSize = 20U;

// Bytes per texel in GR_LFBWRITEMODE_565.
constexpr std::uint32_t kGlideLfb565BytesPerTexel = 2U;

// A staging surface standing in for the Voodoo linear frame buffer. The guest
// writes 565 texels straight into `pixels()`; the backend consumes the RGBA8
// conversion at unlock time.
class GlideLfbSurface
{
public:
    // Size the surface for a width x height 565 image. Safe to call repeatedly;
    // it only reallocates when the dimensions actually change.
    bool Resize(std::uint32_t width, std::uint32_t height);

    // Mark the surface locked. `type` is GrLock_t. Returns false when a lock is
    // already outstanding, which Glide treats as a caller error.
    bool BeginLock(std::uint32_t type, std::uint32_t buffer,
                   std::uint32_t write_mode, std::uint32_t origin);
    void EndLock();

    std::uint8_t* pixels() { return pixels_.data(); }
    const std::uint8_t* pixels() const { return pixels_.data(); }
    std::size_t byte_count() const { return pixels_.size(); }

    std::uint32_t width() const { return width_; }
    std::uint32_t height() const { return height_; }
    std::uint32_t stride_in_bytes() const
    {
        return width_ * kGlideLfb565BytesPerTexel;
    }

    bool locked() const { return locked_; }
    std::uint32_t lock_type() const { return lock_type_; }
    std::uint32_t lock_buffer() const { return lock_buffer_; }
    std::uint32_t lock_write_mode() const { return lock_write_mode_; }
    std::uint32_t lock_origin() const { return lock_origin_; }

private:
    std::vector<std::uint8_t> pixels_;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    bool locked_ = false;
    std::uint32_t lock_type_ = 0;
    std::uint32_t lock_buffer_ = 0;
    std::uint32_t lock_write_mode_ = 0;
    std::uint32_t lock_origin_ = 0;
};

// Serialize the five GrLfbInfo_t fields the guest reads back after a lock.
// `image` receives kGlideLfbInfoByteCount little-endian bytes. `size` is echoed
// so the guest sees the value it supplied.
bool BuildGlideLfbInfoImage(std::uint32_t size,
                            std::uint32_t lfb_pointer,
                            std::uint32_t stride_in_bytes,
                            std::uint32_t write_mode,
                            std::uint32_t origin,
                            std::uint8_t* image,
                            std::size_t image_byte_count);

// GR_LFBWRITEMODE_565 -> RGBA8. `color_format` controls whether the outer
// five-bit channels are RGB or BGR. Alpha is opaque: 565 carries no alpha.
bool DecodeGlideLfb565ToRgba8(const std::uint8_t* source,
                              std::size_t source_byte_count,
                              std::uint32_t width,
                              std::uint32_t height,
                              std::uint32_t color_format,
                              std::vector<std::uint8_t>* rgba8);

// RGBA8 -> GR_LFBWRITEMODE_565, used to seed a lock from the framebuffer.
bool EncodeRgba8ToGlideLfb565(const std::uint8_t* rgba8,
                              std::size_t rgba8_byte_count,
                              std::uint32_t width,
                              std::uint32_t height,
                              std::uint32_t color_format,
                              std::uint8_t* destination,
                              std::size_t destination_byte_count);

// Writes a rectangular region of 565 pixels into the staging surface.
// The surface must have been resized to the screen dimensions beforehand.
bool WriteRegionToGlideLfb565(std::uint32_t dst_x,
                              std::uint32_t dst_y,
                              std::uint32_t src_width,
                              std::uint32_t src_height,
                              std::int32_t src_stride_bytes,
                              const std::uint8_t* src_data,
                              std::size_t src_data_byte_count,
                              GlideLfbSurface* surface);

}  // namespace repiu::hle

#endif  // REPIU_HLE_GLIDE_LFB_H_
