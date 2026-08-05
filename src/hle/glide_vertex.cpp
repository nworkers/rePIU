#include "repiu/hle/glide_vertex.h"

#include "repiu/hle/glide_vertex_depth_census.h"

#include <cmath>
#include <cstring>

namespace repiu::hle
{

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
    // Task 433: this is the only place all three depth candidates are still
    // visible -- `z`, `ooz` and `oow` -- so the census that decides which one
    // the guest populates is taken here, before they are discarded.
    if (GlideVertexDepthCensusEnabled())
    {
        RecordGlideVertexDepthSample(ActiveGlideVertexDepthCensus(),
                                     fields[2], fields[6], fields[8]);
    }
    return true;
}

}  // namespace repiu::hle
