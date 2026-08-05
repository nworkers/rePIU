#include "repiu/hle/glide_vertex_depth_census.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace repiu::hle
{
namespace
{

bool ResolveEnabled()
{
    const char* value = std::getenv("REPIU_GLIDE_VERTEX_DEPTH_CENSUS");
    return value != nullptr && std::strcmp(value, "0") != 0;
}

void AccumulateField(GlideVertexDepthFieldStats* stats, const float value)
{
    if (!std::isfinite(value))
    {
        return;
    }
    if (!stats->seen)
    {
        stats->seen = true;
        stats->minimum = value;
        stats->maximum = value;
    }
    else
    {
        if (value < stats->minimum)
        {
            stats->minimum = value;
        }
        if (value > stats->maximum)
        {
            stats->maximum = value;
        }
    }
    if (value != 0.0F)
    {
        ++stats->nonzero_count;
    }
    const float magnitude = std::fabs(value);
    if (magnitude > kGlideVertexDepthMeaningfulMagnitude)
    {
        ++stats->meaningful_count;
        if (!stats->meaningful_seen || magnitude < stats->meaningful_minimum)
        {
            stats->meaningful_seen = true;
            stats->meaningful_minimum = magnitude;
        }
    }
}

GlideVertexDepthCensus g_census;

}  // namespace

bool GlideVertexDepthCensusEnabled()
{
    static const bool enabled = ResolveEnabled();
    return enabled;
}

GlideVertexDepthCensus* ActiveGlideVertexDepthCensus()
{
    return &g_census;
}

void RecordGlideVertexDepthSample(GlideVertexDepthCensus* census,
                                  const float z,
                                  const float ooz,
                                  const float oow)
{
    if (census == nullptr)
    {
        return;
    }
    census->enabled = true;
    ++census->sample_count;
    AccumulateField(&census->z, z);
    AccumulateField(&census->ooz, ooz);
    AccumulateField(&census->oow, oow);
    if (census->raw_count < kGlideVertexDepthCensusRawSamples)
    {
        census->raw_z[census->raw_count] = z;
        census->raw_ooz[census->raw_count] = ooz;
        census->raw_oow[census->raw_count] = oow;
        ++census->raw_count;
    }
}

}  // namespace repiu::hle
