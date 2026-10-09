#include "repiu/hle/glide_vertex.h"

#include <cmath>
#include <cstring>

namespace repiu::hle
{
namespace
{

// A field the guest never writes still reads back as denormal garbage rather
// than a clean zero, so `!= 0` cannot separate "populated" from "left alone".
// Task 433 settled this floor while finding which field carries depth.
constexpr float kGlideVertexDepthMeaningfulMagnitude = 1.0e-6F;

}  // namespace

bool DecodeGlideProducerVertex(const std::uint32_t* producer_dwords,
                               const std::size_t dword_count,
                               GlideDrawVertex* vertex)
{
    if (producer_dwords == nullptr || vertex == nullptr ||
        dword_count < kGlideProducerVertexDwordCount)
    {
        return false;
    }
    float fields[kGlideProducerVertexDwordCount] = {};
    std::memcpy(fields, producer_dwords, sizeof(fields));
    vertex->x = fields[0];
    vertex->y = fields[1];
    vertex->r = fields[3] / 255.0F;
    vertex->g = fields[4] / 255.0F;
    vertex->b = fields[5] / 255.0F;
    vertex->a = fields[7] / 255.0F;
    vertex->s = fields[9];
    vertex->t = fields[10];
    vertex->fog_oow = std::isfinite(fields[8]) && fields[8] > 1.0e-20F
        ? fields[8]
        : 1.0F;
    vertex->texture_oow = vertex->fog_oow;
    // Task 433: a vertex the guest never gave a depth reads back as denormal
    // garbage, which is finite and would sort as "nearly zero" rather than
    // being ignored. Treating anything below the meaningful floor as zero puts
    // those at one end of the range instead of scattering them through it.
    vertex->ooz = std::isfinite(fields[6]) &&
            std::fabs(fields[6]) > kGlideVertexDepthMeaningfulMagnitude
        ? fields[6]
        : 0.0F;
    return true;
}

}  // namespace repiu::hle
