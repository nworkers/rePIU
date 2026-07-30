#ifndef REPIU_HLE_GLIDE_VERTEX_H_
#define REPIU_HLE_GLIDE_VERTEX_H_

#include <cstddef>
#include <cstdint>

namespace repiu::hle
{

constexpr std::size_t kGlideProducerVertexDwordCount = 15U;
constexpr std::size_t kGlideProducerVertexByteCount =
    kGlideProducerVertexDwordCount * sizeof(std::uint32_t);

struct GlideDrawVertex
{
    float x = 0.0F;
    float y = 0.0F;
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;
    float s = 0.0F;
    float t = 0.0F;
    float fog_oow = 1.0F;
    float texture_oow = 1.0F;
};

bool DecodeGlideProducerVertex(const std::uint32_t* producer_dwords,
                               std::size_t dword_count,
                               GlideDrawVertex* vertex);

}  // namespace repiu::hle

#endif  // REPIU_HLE_GLIDE_VERTEX_H_
