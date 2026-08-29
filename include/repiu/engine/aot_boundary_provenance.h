#ifndef REPIU_ENGINE_AOT_BOUNDARY_PROVENANCE_H_
#define REPIU_ENGINE_AOT_BOUNDARY_PROVENANCE_H_

#include <cstdint>

namespace repiu::engine
{

struct AotCodeCachePlacement;

// Structural origin of an INT3 reached inside the placed code cache. This is
// deliberately independent from the guest-opcode boundary reason: a copied
// PUSH EBP can be an inactive-generation sentinel, while a GS-prefixed guest
// instruction can be a planner HLE boundary or a selector-guard fallback.
enum class AotCacheBreakpointProvenance : std::uint32_t
{
    kPlannerHle = 0,
    kSegmentGuard,
    kInlineCacheFallback,
    kJumpTableFallback,
    kRetiredOrInactiveEntry,
    kProbeSentinel,
    kOtherPlannerFixup,
    kUnknown,
    kCount,
};

constexpr std::uint32_t kAotCacheBreakpointProvenanceCount =
    static_cast<std::uint32_t>(AotCacheBreakpointProvenance::kCount);

AotCacheBreakpointProvenance ClassifyAotCacheBreakpointProvenance(
    const AotCodeCachePlacement& placement,
    std::uint32_t cache_address,
    bool explicit_probe_sentinel);

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_AOT_BOUNDARY_PROVENANCE_H_
