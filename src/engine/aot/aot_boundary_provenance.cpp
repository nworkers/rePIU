#include "repiu/engine/aot_boundary_provenance.h"

#include "repiu/engine/aot_code_cache.h"

namespace repiu::engine
{

AotCacheBreakpointProvenance ClassifyAotCacheBreakpointProvenance(
    const AotCodeCachePlacement& placement,
    std::uint32_t cache_address,
    bool explicit_probe_sentinel)
{
    if (!placement.placed || cache_address < placement.base_address)
    {
        return AotCacheBreakpointProvenance::kUnknown;
    }
    if (IsAotCacheAddressRetired(placement, cache_address))
    {
        return AotCacheBreakpointProvenance::kRetiredOrInactiveEntry;
    }
    if (explicit_probe_sentinel)
    {
        return AotCacheBreakpointProvenance::kProbeSentinel;
    }

    const std::uint32_t offset = cache_address - placement.base_address;
    const auto found =
        placement.breakpoint_provenance_by_cache_offset.find(offset);
    if (found != placement.breakpoint_provenance_by_cache_offset.end())
    {
        return found->second;
    }
    return AotCacheBreakpointProvenance::kUnknown;
}

}  // namespace repiu::engine
