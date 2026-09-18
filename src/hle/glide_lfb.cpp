#include "repiu/hle/glide_lfb.h"

#include <cstring>

namespace repiu::hle
{
namespace
{

void WriteLittleEndianUInt32(std::uint8_t* destination, std::uint32_t value)
{
    destination[0] = static_cast<std::uint8_t>(value);
    destination[1] = static_cast<std::uint8_t>(value >> 8U);
    destination[2] = static_cast<std::uint8_t>(value >> 16U);
    destination[3] = static_cast<std::uint8_t>(value >> 24U);
}

}  // namespace

bool GlideLfbSurface::UseExternalStorage(std::uint8_t* const base,
                                         const std::size_t byte_count)
{
    if (base == nullptr || byte_count == 0U)
    {
        return false;
    }
    external_base_ = base;
    external_byte_count_ = byte_count;
    // The heap buffer is given back rather than left shadowing the external
    // storage: keeping it would hold a second copy of a 600 KB surface that
    // nothing can reach any more.
    pixels_.clear();
    pixels_.shrink_to_fit();
    // The dimensions are dropped so the next Resize does its work rather than
    // matching against sizes that described the buffer just released.
    width_ = 0;
    height_ = 0;
    locked_ = false;
    return true;
}

bool GlideLfbSurface::Resize(std::uint32_t width, std::uint32_t height)
{
    if (width == 0U || height == 0U)
    {
        return false;
    }
    const std::size_t required = static_cast<std::size_t>(width) * height *
        kGlideLfb565BytesPerTexel;
    if (external_base_ != nullptr)
    {
        // Nothing is allocated here. The caller placed this storage where a
        // 32-bit field can name it, and growing it would mean moving it, which
        // would invalidate an address the guest may already hold.
        if (required > external_byte_count_)
        {
            return false;
        }
        if (width == width_ && height == height_)
        {
            return true;
        }
        std::memset(external_base_, 0, required);
        width_ = width;
        height_ = height;
        return true;
    }
    if (width == width_ && height == height_ && !pixels_.empty())
    {
        return true;
    }
    pixels_.assign(required, 0U);
    width_ = width;
    height_ = height;
    return true;
}

bool GlideLfbSurface::BeginLock(std::uint32_t type,
                                std::uint32_t buffer,
                                std::uint32_t write_mode,
                                std::uint32_t origin)
{
    if (locked_ || byte_count() == 0U)
    {
        return false;
    }
    locked_ = true;
    lock_type_ = type;
    lock_buffer_ = buffer;
    lock_write_mode_ = write_mode;
    lock_origin_ = origin;
    return true;
}

void GlideLfbSurface::EndLock()
{
    locked_ = false;
}

bool BuildGlideLfbInfoImage(std::uint32_t size,
                            std::uint32_t lfb_pointer,
                            std::uint32_t stride_in_bytes,
                            std::uint32_t write_mode,
                            std::uint32_t origin,
                            std::uint8_t* image,
                            std::size_t image_byte_count)
{
    if (image == nullptr || image_byte_count < kGlideLfbInfoByteCount)
    {
        return false;
    }
    WriteLittleEndianUInt32(image + 0U, size);
    WriteLittleEndianUInt32(image + 4U, lfb_pointer);
    WriteLittleEndianUInt32(image + 8U, stride_in_bytes);
    WriteLittleEndianUInt32(image + 12U, write_mode);
    WriteLittleEndianUInt32(image + 16U, origin);
    return true;
}

bool DecodeGlideLfb565ToRgba8(const std::uint8_t* source,
                              std::size_t source_byte_count,
                              std::uint32_t width,
                              std::uint32_t height,
                              std::uint32_t color_format,
                              std::vector<std::uint8_t>* rgba8)
{
    if (source == nullptr || rgba8 == nullptr || width == 0U || height == 0U)
    {
        return false;
    }
    const std::size_t texel_count =
        static_cast<std::size_t>(width) * height;
    if (source_byte_count < texel_count * kGlideLfb565BytesPerTexel)
    {
        return false;
    }
    rgba8->assign(texel_count * 4U, 0U);
    for (std::size_t index = 0; index < texel_count; ++index)
    {
        const std::uint32_t texel =
            static_cast<std::uint32_t>(source[index * 2U]) |
            (static_cast<std::uint32_t>(source[index * 2U + 1U]) << 8U);
        const std::uint8_t high = ExpandGlideChannel5(texel >> 11U);
        const std::uint8_t low = ExpandGlideChannel5(texel);
        const bool bgr = GlideColorFormatUsesBgrOrder(color_format);
        (*rgba8)[index * 4U + 0U] = bgr ? low : high;
        (*rgba8)[index * 4U + 1U] = ExpandGlideChannel6(texel >> 5U);
        (*rgba8)[index * 4U + 2U] = bgr ? high : low;
        (*rgba8)[index * 4U + 3U] = 255U;
    }
    return true;
}

bool EncodeRgba8ToGlideLfb565(const std::uint8_t* rgba8,
                              std::size_t rgba8_byte_count,
                              std::uint32_t width,
                              std::uint32_t height,
                              std::uint32_t color_format,
                              std::uint8_t* destination,
                              std::size_t destination_byte_count)
{
    if (rgba8 == nullptr || destination == nullptr || width == 0U ||
        height == 0U)
    {
        return false;
    }
    const std::size_t texel_count =
        static_cast<std::size_t>(width) * height;
    if (rgba8_byte_count < texel_count * 4U ||
        destination_byte_count < texel_count * kGlideLfb565BytesPerTexel)
    {
        return false;
    }
    for (std::size_t index = 0; index < texel_count; ++index)
    {
        const std::uint32_t red = rgba8[index * 4U + 0U];
        const std::uint32_t green = rgba8[index * 4U + 1U];
        const std::uint32_t blue = rgba8[index * 4U + 2U];
        const std::uint32_t texel =
            PackGlideTexel565(red, green, blue, color_format);
        destination[index * 2U] = static_cast<std::uint8_t>(texel);
        destination[index * 2U + 1U] =
            static_cast<std::uint8_t>(texel >> 8U);
    }
    return true;
}

}  // namespace repiu::hle
