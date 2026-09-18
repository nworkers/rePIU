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

// GrColorFormat_t. For 565 LFB locks, ARGB/RGBA select RGB565 while
// ABGR/BGRA select BGR565 (Glide 2.4 Programming Guide, Table 11.2).
// GrLfbSrcFmt_t, the separate source-image format of the region transfers,
// lives in glide_lfb_region.h.
constexpr std::uint32_t kGlideColorFormatArgb = 0U;
constexpr std::uint32_t kGlideColorFormatAbgr = 1U;
constexpr std::uint32_t kGlideColorFormatRgba = 2U;
constexpr std::uint32_t kGlideColorFormatBgra = 3U;

// True when the color format puts blue in the high five bits of a 565 texel.
inline bool GlideColorFormatUsesBgrOrder(std::uint32_t color_format)
{
    return color_format == kGlideColorFormatAbgr ||
        color_format == kGlideColorFormatBgra;
}

// 5- and 6-bit channels are expanded by replicating the high bits into the low
// ones so that the maximum encoded value maps exactly to 255.
inline std::uint8_t ExpandGlideChannel5(std::uint32_t value)
{
    const std::uint32_t five = value & 0x1FU;
    return static_cast<std::uint8_t>((five << 3U) | (five >> 2U));
}

inline std::uint8_t ExpandGlideChannel6(std::uint32_t value)
{
    const std::uint32_t six = value & 0x3FU;
    return static_cast<std::uint8_t>((six << 2U) | (six >> 4U));
}

// Packs 8-bit channels into one 565 texel in `color_format` order.
inline std::uint16_t PackGlideTexel565(std::uint32_t red,
                                       std::uint32_t green,
                                       std::uint32_t blue,
                                       std::uint32_t color_format)
{
    const bool bgr = GlideColorFormatUsesBgrOrder(color_format);
    const std::uint32_t high = bgr ? blue : red;
    const std::uint32_t low = bgr ? red : blue;
    return static_cast<std::uint16_t>(((high >> 3U) << 11U) |
                                      ((green >> 2U) << 5U) | (low >> 3U));
}

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
    //
    // With external storage installed nothing is allocated: the size is checked
    // against that storage and refused when it does not fit.
    bool Resize(std::uint32_t width, std::uint32_t height);

    // Task 708. Use storage the caller owns instead of this object's own
    // buffer.
    //
    // grLfbLock hands `pixels()` to the guest as a 32-bit `GrLfbInfo_t::lfbPtr`
    // and the guest writes through it. On a 64-bit host the heap buffer this
    // class allocates for itself sits above 4 GiB, so that pointer arrives
    // truncated and the first write faults. The caller that knows how to place
    // memory a 32-bit field can name installs it here; this layer names no
    // operating system and does no placement of its own.
    //
    // The storage must outlive the surface, and installing it discards whatever
    // the surface held. Returns false for a null base or an empty range.
    bool UseExternalStorage(std::uint8_t* base, std::size_t byte_count);

    // Whether external storage is installed, which is what separates "the guest
    // may be given this address" from "this is an ordinary heap buffer".
    bool uses_external_storage() const { return external_base_ != nullptr; }

    // Mark the surface locked. `type` is GrLock_t. Returns false when a lock is
    // already outstanding, which Glide treats as a caller error.
    bool BeginLock(std::uint32_t type, std::uint32_t buffer,
                   std::uint32_t write_mode, std::uint32_t origin);
    void EndLock();

    std::uint8_t* pixels()
    {
        return external_base_ != nullptr ? external_base_ : pixels_.data();
    }
    const std::uint8_t* pixels() const
    {
        return external_base_ != nullptr ? external_base_ : pixels_.data();
    }
    // The bytes of the current image, not the capacity behind it. External
    // storage may be larger -- a host reserves in pages, and Windows in 64 KiB
    // granules -- and every caller of this uses it as the length of the image
    // to convert or copy, which is what it meant before external storage
    // existed.
    std::size_t byte_count() const
    {
        return external_base_ != nullptr
            ? static_cast<std::size_t>(width_) * height_ *
                  kGlideLfb565BytesPerTexel
            : pixels_.size();
    }

    // The capacity installed behind the image, which is what a Resize is
    // measured against.
    std::size_t external_byte_count() const { return external_byte_count_; }

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
    // Not owned. Null means this object's own buffer above is the storage.
    std::uint8_t* external_base_ = nullptr;
    std::size_t external_byte_count_ = 0;
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

}  // namespace repiu::hle

#endif  // REPIU_HLE_GLIDE_LFB_H_
