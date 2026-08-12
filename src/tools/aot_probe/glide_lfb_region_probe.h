#ifndef REPIU_TOOLS_AOT_PROBE_GLIDE_LFB_REGION_PROBE_H_
#define REPIU_TOOLS_AOT_PROBE_GLIDE_LFB_REGION_PROBE_H_

namespace repiu::tools
{

// Task 476: checks the grLfbWriteRegion / grLfbReadRegion transfer rules --
// the GrLfbSrcFmt_t table, the zero-stride derivation, per-format pixel
// conversion, clipping at the surface edge, and the write/read round trip.
bool RunGlideLfbRegionProbe();

}  // namespace repiu::tools

#endif  // REPIU_TOOLS_AOT_PROBE_GLIDE_LFB_REGION_PROBE_H_
