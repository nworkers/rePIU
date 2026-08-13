#ifndef REPIU_HLE_GLIDE_TEXTURE_DECODE_H_
#define REPIU_HLE_GLIDE_TEXTURE_DECODE_H_

#include <cstdint>
#include <vector>

namespace repiu::hle
{

// Dimensions of a Glide texture LOD derived from the largeLod and aspectRatio
// fields. This project's PIU build uses the same convention as
// CalculateGlideTextureMemoryRequired: the LOD value is the base-2 log of the
// larger dimension scale, and aspectRatio biases width vs height around square
// (aspect 3). width_log2 = lod + aspect - 3, height_log2 = lod - aspect + 3.
struct GlideTextureDimensions
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

// Converts the standard 256-entry Glide palette from 0xAARRGGBB words to
// RGBA8. The high byte is ignored by Glide and output alpha is always opaque.
bool DecodeGlidePaletteToRgba8(const std::uint32_t* source,
                               std::size_t entry_count,
                               std::uint8_t* rgba8_out,
                               std::size_t output_size);

bool CalculateGlideTextureDimensions(std::uint32_t large_lod,
                                     std::uint32_t aspect_ratio,
                                     GlideTextureDimensions* dimensions);

// Decode a Glide texture image at the given format into tightly packed RGBA8
// (width*height*4 bytes). Returns false for an unsupported format or a source
// range too small for width*height texels. `source_size` bounds the readable
// source bytes. Palette is optional (256 * RGBA8) for P_8 / AP_88 formats.
bool DecodeGlideTextureToRgba8(std::uint32_t format,
                               std::uint32_t width,
                               std::uint32_t height,
                               const std::uint8_t* source,
                               std::size_t source_size,
                               const std::uint8_t* palette_rgba8,
                               std::vector<std::uint8_t>* rgba8_out);

// Returns true if the Glide texture format is supported and decodable by our backend.
bool IsGlideTextureFormatAcceptable(std::uint32_t format);

bool IsGlidePalettizedTextureFormat(std::uint32_t format);

}  // namespace repiu::hle

#endif  // REPIU_HLE_GLIDE_TEXTURE_DECODE_H_
