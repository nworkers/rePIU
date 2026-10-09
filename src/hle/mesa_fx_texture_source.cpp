#include "repiu/hle/mesa_fx_texture_source.h"

namespace repiu::hle
{
namespace
{

constexpr std::uint32_t kGlideFormatRgb565 = 10U;
constexpr std::uint32_t kGlideFormatArgb4444 = 12U;

// Offsets of the shared Mesa build; see the header.
constexpr std::uint32_t kSavedCallerEbpOffset = 0x34U;
constexpr std::uint32_t kTextureObjectDriverData = 0x484U;
constexpr std::uint32_t kTextureObjectImages = 0x50U;
constexpr std::uint32_t kTextureInfoMipmapLevels = 0x14U;
constexpr std::uint32_t kTextureInfoMipmapStride = 0x14U;
constexpr std::uint32_t kMipmapLevelData = 0x8U;
constexpr std::uint32_t kImageWidth = 0xCU;
constexpr std::uint32_t kImageHeight = 0x10U;
constexpr std::uint32_t kImageData = 0x34U;
// Mesa 3.x's MAX_TEXTURE_LEVELS, and Glide's largest edge.
constexpr std::uint32_t kMaxLevels = 12U;
constexpr std::uint32_t kMaxEdge = 256U;

bool ReadU32(const MesaFxGuestReader& read, std::uint32_t address,
             std::uint32_t* value)
{
    return read(address, value, sizeof(*value));
}

std::uint16_t Pack4444(const std::uint8_t* rgba)
{
    return static_cast<std::uint16_t>(
        ((rgba[3] & 0xF0U) << 8) | ((rgba[0] & 0xF0U) << 4) |
        (rgba[1] & 0xF0U) | ((rgba[2] & 0xF0U) >> 4));
}

std::uint16_t Pack565(const std::uint8_t* rgba)
{
    return static_cast<std::uint16_t>(((rgba[0] & 0xF8U) << 8) |
                                      ((rgba[1] & 0xFCU) << 3) |
                                      (rgba[2] >> 3));
}

}  // namespace

const char* MesaFxSourceOutcomeName(MesaFxSourceOutcome outcome)
{
    switch (outcome)
    {
        case MesaFxSourceOutcome::kUsed:
            return "used";
        case MesaFxSourceOutcome::kNotApplicable:
            return "not-applicable";
        case MesaFxSourceOutcome::kUnreadable:
            return "unreadable";
        case MesaFxSourceOutcome::kLinkMismatch:
            return "link-mismatch";
        case MesaFxSourceOutcome::kDataMismatch:
            return "data-mismatch";
        case MesaFxSourceOutcome::kVerifyMismatch:
            return "verify-mismatch";
        case MesaFxSourceOutcome::kCount:
            break;
    }
    return "unknown";
}

MesaFxSourceOutcome BuildMesaFxFullPrecisionTexture(
    const MesaFxGuestReader& read, const MesaFxGateState& gate,
    std::uint32_t glide_format, std::uint32_t width, std::uint32_t height,
    const std::uint8_t* glide_data, std::size_t glide_data_size,
    std::vector<std::uint8_t>* rgba8)
{
    if (rgba8 == nullptr)
    {
        return MesaFxSourceOutcome::kNotApplicable;
    }
    rgba8->clear();
    const bool argb4444 = glide_format == kGlideFormatArgb4444;
    if (!read || glide_data == nullptr ||
        (!argb4444 && glide_format != kGlideFormatRgb565) || width == 0U ||
        height == 0U || width > kMaxEdge || height > kMaxEdge ||
        glide_data_size < static_cast<std::size_t>(width) * height * 2U ||
        gate.ebp >= kMaxLevels)
    {
        return MesaFxSourceOutcome::kNotApplicable;
    }

    // The texture object, from the caller's saved EBP, must own this info.
    std::uint32_t texture_object = 0;
    std::uint32_t driver_data = 0;
    if (!ReadU32(read, gate.esp + kSavedCallerEbpOffset, &texture_object) ||
        !ReadU32(read, texture_object + kTextureObjectDriverData,
                 &driver_data))
    {
        return MesaFxSourceOutcome::kUnreadable;
    }
    if (driver_data != gate.esi)
    {
        return MesaFxSourceOutcome::kLinkMismatch;
    }

    // And this level's converted data must be the buffer being uploaded.
    std::uint32_t level_data = 0;
    if (!ReadU32(read,
                 gate.esi + kTextureInfoMipmapLevels +
                     gate.ebp * kTextureInfoMipmapStride + kMipmapLevelData,
                 &level_data))
    {
        return MesaFxSourceOutcome::kUnreadable;
    }
    if (level_data != gate.data_address)
    {
        return MesaFxSourceOutcome::kDataMismatch;
    }

    std::uint32_t image = 0;
    std::uint32_t source_width = 0;
    std::uint32_t source_height = 0;
    std::uint32_t source_data = 0;
    if (!ReadU32(read, texture_object + kTextureObjectImages + gate.ebp * 4U,
                 &image) ||
        !ReadU32(read, image + kImageWidth, &source_width) ||
        !ReadU32(read, image + kImageHeight, &source_height) ||
        !ReadU32(read, image + kImageData, &source_data))
    {
        return MesaFxSourceOutcome::kUnreadable;
    }
    // Mesa only ever enlarges by whole factors to reach the Glide size.
    if (source_width == 0U || source_height == 0U || source_width > width ||
        source_height > height || width % source_width != 0U ||
        height % source_height != 0U)
    {
        return MesaFxSourceOutcome::kNotApplicable;
    }

    const std::uint32_t source_texel_bytes = argb4444 ? 4U : 3U;
    std::vector<std::uint8_t> source(static_cast<std::size_t>(source_width) *
                                     source_height * source_texel_bytes);
    if (!read(source_data, source.data(), source.size()))
    {
        return MesaFxSourceOutcome::kUnreadable;
    }

    std::vector<std::uint8_t> built(static_cast<std::size_t>(width) * height *
                                    4U);
    for (std::uint32_t y = 0; y < height; ++y)
    {
        const std::uint32_t source_y = y * source_height / height;
        for (std::uint32_t x = 0; x < width; ++x)
        {
            const std::uint32_t source_x = x * source_width / width;
            const std::uint8_t* texel =
                source.data() +
                (static_cast<std::size_t>(source_y) * source_width + source_x) *
                    source_texel_bytes;
            std::uint8_t* out =
                built.data() + (static_cast<std::size_t>(y) * width + x) * 4U;
            out[0] = texel[0];
            out[1] = texel[1];
            out[2] = texel[2];
            out[3] = argb4444 ? texel[3] : 0xFFU;
            const std::uint16_t packed = argb4444 ? Pack4444(out) : Pack565(out);
            const std::size_t index =
                (static_cast<std::size_t>(y) * width + x) * 2U;
            const std::uint16_t game =
                static_cast<std::uint16_t>(glide_data[index] |
                                           (glide_data[index + 1U] << 8));
            if (packed != game)
            {
                return MesaFxSourceOutcome::kVerifyMismatch;
            }
        }
    }
    *rgba8 = std::move(built);
    return MesaFxSourceOutcome::kUsed;
}

}  // namespace repiu::hle
