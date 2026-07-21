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

// 5- and 6-bit channels are expanded by replicating the high bits into the low
// ones so that the maximum encoded value maps exactly to 255.
std::uint8_t Expand5(std::uint32_t value)
{
    const std::uint32_t five = value & 0x1FU;
    return static_cast<std::uint8_t>((five << 3U) | (five >> 2U));
}

std::uint8_t Expand6(std::uint32_t value)
{
    const std::uint32_t six = value & 0x3FU;
    return static_cast<std::uint8_t>((six << 2U) | (six >> 4U));
}

}  // namespace

bool GlideLfbSurface::Resize(std::uint32_t width, std::uint32_t height)
{
    if (width == 0U || height == 0U)
    {
        return false;
    }
    if (width == width_ && height == height_ && !pixels_.empty())
    {
        return true;
    }
    const std::size_t required = static_cast<std::size_t>(width) * height *
        kGlideLfb565BytesPerTexel;
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
    if (locked_ || pixels_.empty())
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
        (*rgba8)[index * 4U + 0U] = Expand5(texel >> 11U);
        (*rgba8)[index * 4U + 1U] = Expand6(texel >> 5U);
        (*rgba8)[index * 4U + 2U] = Expand5(texel);
        (*rgba8)[index * 4U + 3U] = 255U;
    }
    return true;
}

bool EncodeRgba8ToGlideLfb565(const std::uint8_t* rgba8,
                              std::size_t rgba8_byte_count,
                              std::uint32_t width,
                              std::uint32_t height,
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
        const std::uint32_t texel = ((red >> 3U) << 11U) |
            ((green >> 2U) << 5U) | (blue >> 3U);
        destination[index * 2U] = static_cast<std::uint8_t>(texel);
        destination[index * 2U + 1U] =
            static_cast<std::uint8_t>(texel >> 8U);
    }
    return true;
}

}  // namespace repiu::hle
