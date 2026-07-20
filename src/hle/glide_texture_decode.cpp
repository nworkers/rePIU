#include "repiu/hle/glide_texture_decode.h"

#include <algorithm>

namespace repiu::hle
{
namespace
{

constexpr std::int32_t kAspectSquare = 3;

// Expand an n-bit channel value to 8 bits by bit replication.
std::uint8_t Expand(std::uint32_t value, std::uint32_t bits)
{
    if (bits == 0U)
    {
        return 0U;
    }
    const std::uint32_t max_in = (1U << bits) - 1U;
    return static_cast<std::uint8_t>((value * 255U + max_in / 2U) / max_in);
}

}  // namespace

bool CalculateGlideTextureDimensions(std::uint32_t large_lod,
                                     std::uint32_t aspect_ratio,
                                     GlideTextureDimensions* dimensions)
{
    constexpr std::uint32_t kMaxLod = 8U;
    constexpr std::uint32_t kMaxAspect = 6U;
    if (dimensions == nullptr || large_lod > kMaxLod ||
        aspect_ratio > kMaxAspect)
    {
        return false;
    }
    const std::int32_t width_log2 = static_cast<std::int32_t>(large_lod) +
        static_cast<std::int32_t>(aspect_ratio) - kAspectSquare;
    const std::int32_t height_log2 = static_cast<std::int32_t>(large_lod) -
        static_cast<std::int32_t>(aspect_ratio) + kAspectSquare;
    dimensions->width = 1U << std::max(width_log2, 0);
    dimensions->height = 1U << std::max(height_log2, 0);
    return true;
}

bool DecodeGlideTextureToRgba8(std::uint32_t format,
                               std::uint32_t width,
                               std::uint32_t height,
                               const std::uint8_t* source,
                               std::size_t source_size,
                               const std::uint8_t* palette_rgba8,
                               std::vector<std::uint8_t>* rgba8_out)
{
    if (source == nullptr || rgba8_out == nullptr || width == 0U ||
        height == 0U)
    {
        return false;
    }
    const std::size_t texel_count =
        static_cast<std::size_t>(width) * height;

    // Glide GrTextureFormat_t: formats 0-7 are 8-bit, 8-15 are 16-bit.
    const bool sixteen_bit = format >= 8U;
    const std::size_t bytes_per_texel = sixteen_bit ? 2U : 1U;
    if (source_size < texel_count * bytes_per_texel)
    {
        return false;
    }

    rgba8_out->assign(texel_count * 4U, 0U);
    std::uint8_t* out = rgba8_out->data();

    for (std::size_t i = 0; i < texel_count; ++i)
    {
        std::uint8_t r = 0;
        std::uint8_t g = 0;
        std::uint8_t b = 0;
        std::uint8_t a = 255U;
        if (sixteen_bit)
        {
            const std::uint32_t texel =
                static_cast<std::uint32_t>(source[i * 2U]) |
                (static_cast<std::uint32_t>(source[i * 2U + 1U]) << 8U);
            switch (format)
            {
                case 10U:  // RGB_565
                    r = Expand((texel >> 11U) & 0x1FU, 5U);
                    g = Expand((texel >> 5U) & 0x3FU, 6U);
                    b = Expand(texel & 0x1FU, 5U);
                    a = 255U;
                    break;
                case 11U:  // ARGB_1555
                    a = ((texel >> 15U) & 0x1U) != 0U ? 255U : 0U;
                    r = Expand((texel >> 10U) & 0x1FU, 5U);
                    g = Expand((texel >> 5U) & 0x1FU, 5U);
                    b = Expand(texel & 0x1FU, 5U);
                    break;
                case 12U:  // ARGB_4444
                    a = Expand((texel >> 12U) & 0xFU, 4U);
                    r = Expand((texel >> 8U) & 0xFU, 4U);
                    g = Expand((texel >> 4U) & 0xFU, 4U);
                    b = Expand(texel & 0xFU, 4U);
                    break;
                case 13U:  // ALPHA_INTENSITY_88
                {
                    const std::uint8_t intensity =
                        static_cast<std::uint8_t>(texel & 0xFFU);
                    a = static_cast<std::uint8_t>((texel >> 8U) & 0xFFU);
                    r = g = b = intensity;
                    break;
                }
                case 8U:  // ARGB_8332
                    a = static_cast<std::uint8_t>((texel >> 8U) & 0xFFU);
                    r = Expand((texel >> 5U) & 0x7U, 3U);
                    g = Expand((texel >> 2U) & 0x7U, 3U);
                    b = Expand(texel & 0x3U, 2U);
                    break;
                case 14U:  // AP_88 (alpha + palette index)
                {
                    a = static_cast<std::uint8_t>((texel >> 8U) & 0xFFU);
                    const std::uint32_t index = texel & 0xFFU;
                    if (palette_rgba8 != nullptr)
                    {
                        r = palette_rgba8[index * 4U];
                        g = palette_rgba8[index * 4U + 1U];
                        b = palette_rgba8[index * 4U + 2U];
                    }
                    break;
                }
                default:
                    return false;
            }
        }
        else
        {
            const std::uint8_t texel = source[i];
            switch (format)
            {
                case 0U:  // RGB_332
                    r = Expand((texel >> 5U) & 0x7U, 3U);
                    g = Expand((texel >> 2U) & 0x7U, 3U);
                    b = Expand(texel & 0x3U, 2U);
                    a = 255U;
                    break;
                case 2U:  // ALPHA_8
                    a = texel;
                    r = g = b = texel;
                    break;
                case 3U:  // INTENSITY_8
                    r = g = b = texel;
                    a = 255U;
                    break;
                case 4U:  // ALPHA_INTENSITY_44
                {
                    const std::uint8_t intensity = Expand(texel & 0xFU, 4U);
                    a = Expand((texel >> 4U) & 0xFU, 4U);
                    r = g = b = intensity;
                    break;
                }
                case 5U:  // P_8 (palette index)
                    if (palette_rgba8 != nullptr)
                    {
                        r = palette_rgba8[texel * 4U];
                        g = palette_rgba8[texel * 4U + 1U];
                        b = palette_rgba8[texel * 4U + 2U];
                        a = palette_rgba8[texel * 4U + 3U];
                    }
                    else
                    {
                        r = g = b = texel;
                    }
                    break;
                default:
                    return false;
            }
        }
        out[i * 4U] = r;
        out[i * 4U + 1U] = g;
        out[i * 4U + 2U] = b;
        out[i * 4U + 3U] = a;
    }
    return true;
}

bool IsGlideTextureFormatAcceptable(std::uint32_t format)
{
    switch (format)
    {
        case 0U:  // RGB_332
        case 2U:  // ALPHA_8
        case 3U:  // INTENSITY_8
        case 4U:  // ALPHA_INTENSITY_44
        case 5U:  // P_8
        case 8U:  // ARGB_8332
        case 10U: // RGB_565
        case 11U: // ARGB_1555
        case 12U: // ARGB_4444
        case 13U: // ALPHA_INTENSITY_88
        case 14U: // AP_88
            return true;
        default:
            return false;
    }
}

}  // namespace repiu::hle
