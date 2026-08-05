#ifndef REPIU_HLE_GLIDE_VERTEX_DEPTH_CENSUS_H_
#define REPIU_HLE_GLIDE_VERTEX_DEPTH_CENSUS_H_

#include <cstddef>
#include <cstdint>

namespace repiu::hle
{

// Task 433. The 3D model renders corrupted because vertex depth never reaches
// OpenGL: the decoder drops `z` and `ooz`, and every vertex is emitted at zero.
// Before wiring one of them through, this records *which* field the guest
// actually populates -- mapping from an empty field would only break the screen
// a different way.
//
// Counts non-zero samples separately from the range, because a field that is
// always zero and a field whose real value happens to be zero are the same
// number but opposite conclusions.
// See docs/design/20260806-433-glide-vertex-depth.md.

constexpr std::size_t kGlideVertexDepthCensusRawSamples = 16U;

// A field the guest never writes still reads back as denormal garbage rather
// than a clean zero, so `!= 0` cannot separate "populated" from "left alone".
// `meaningful_count` applies a magnitude floor, which is the number the verdict
// actually rests on.
constexpr float kGlideVertexDepthMeaningfulMagnitude = 1.0e-6F;

struct GlideVertexDepthFieldStats
{
    std::uint64_t nonzero_count = 0;
    std::uint64_t meaningful_count = 0;
    float minimum = 0.0F;
    float maximum = 0.0F;
    // Smallest magnitude above the floor, so the populated range is readable
    // without the denormals dragging the minimum to zero.
    float meaningful_minimum = 0.0F;
    bool seen = false;
    bool meaningful_seen = false;
};

struct GlideVertexDepthCensus
{
    bool enabled = false;
    std::uint64_t sample_count = 0;
    GlideVertexDepthFieldStats z;
    GlideVertexDepthFieldStats ooz;
    GlideVertexDepthFieldStats oow;
    std::size_t raw_count = 0;
    float raw_z[kGlideVertexDepthCensusRawSamples] = {};
    float raw_ooz[kGlideVertexDepthCensusRawSamples] = {};
    float raw_oow[kGlideVertexDepthCensusRawSamples] = {};
};

bool GlideVertexDepthCensusEnabled();

// Called from the producer-vertex decoder, which is the one place all three
// candidate fields are still visible.
void RecordGlideVertexDepthSample(GlideVertexDepthCensus* census,
                                  float z,
                                  float ooz,
                                  float oow);

GlideVertexDepthCensus* ActiveGlideVertexDepthCensus();

}  // namespace repiu::hle

#endif  // REPIU_HLE_GLIDE_VERTEX_DEPTH_CENSUS_H_
