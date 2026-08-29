#include "boundary_provenance_probe.h"

#include "repiu/engine/aot_boundary_provenance.h"
#include "repiu/engine/aot_code_cache.h"

#include <iostream>

namespace repiu::tools
{

bool RunAotBoundaryProvenanceProbe()
{
    using repiu::engine::AotCacheBreakpointProvenance;
    using repiu::engine::ClassifyAotCacheBreakpointProvenance;
    repiu::engine::AotCodeCachePlacement placement;
    placement.placed = true;
    placement.base_address = 0x10000000U;

    placement.fixups.push_back({repiu::runtime::AotFixupKind::kHleBoundary,
                                0x20000000U, 0U, 0x10U, false});
    repiu::runtime::AotSegmentOverrideSite segment;
    segment.cache_offset = 0x20U;
    placement.segment_override_sites.push_back(segment);
    repiu::runtime::AotIndirectInlineCacheSite inline_site;
    inline_site.miss_cache_offset = 0x40U;
    placement.indirect_inline_cache_sites.push_back(inline_site);
    repiu::runtime::AotJumpTableSite jump_table;
    jump_table.fallback_offset = 0x60U;
    placement.jump_table_sites.push_back(jump_table);

    repiu::runtime::AotAddressMapEntry retired_map;
    retired_map.cache_offset = 0x80U;
    retired_map.emitted_length = 1U;
    placement.address_map.push_back(retired_map);
    placement.address_map_states.push_back({});
    placement.address_map_states[0].active = false;
    placement.inactive_map_index_by_cache_offset.emplace(0x80U, 0U);
    placement.breakpoint_provenance_by_cache_offset.emplace(
        0x10U, AotCacheBreakpointProvenance::kPlannerHle);
    placement.breakpoint_provenance_by_cache_offset.emplace(
        0x2DU, AotCacheBreakpointProvenance::kSegmentGuard);
    placement.breakpoint_provenance_by_cache_offset.emplace(
        0x41U, AotCacheBreakpointProvenance::kInlineCacheFallback);
    placement.breakpoint_provenance_by_cache_offset.emplace(
        0x60U, AotCacheBreakpointProvenance::kJumpTableFallback);
    placement.breakpoint_provenance_by_cache_offset.emplace(
        0x80U, AotCacheBreakpointProvenance::kPlannerHle);
    placement.breakpoint_provenance_by_cache_offset.emplace(
        0xA0U, AotCacheBreakpointProvenance::kOtherPlannerFixup);

    const bool passed =
        ClassifyAotCacheBreakpointProvenance(
            placement, placement.base_address + 0x10U, false) ==
            AotCacheBreakpointProvenance::kPlannerHle &&
        ClassifyAotCacheBreakpointProvenance(
            placement, placement.base_address + 0x2DU, false) ==
            AotCacheBreakpointProvenance::kSegmentGuard &&
        ClassifyAotCacheBreakpointProvenance(
            placement, placement.base_address + 0x41U, false) ==
            AotCacheBreakpointProvenance::kInlineCacheFallback &&
        ClassifyAotCacheBreakpointProvenance(
            placement, placement.base_address + 0x60U, false) ==
            AotCacheBreakpointProvenance::kJumpTableFallback &&
        ClassifyAotCacheBreakpointProvenance(
            placement, placement.base_address + 0x80U, false) ==
            AotCacheBreakpointProvenance::kRetiredOrInactiveEntry &&
        ClassifyAotCacheBreakpointProvenance(
            placement, placement.base_address + 0xA0U, false) ==
            AotCacheBreakpointProvenance::kOtherPlannerFixup &&
        ClassifyAotCacheBreakpointProvenance(
            placement, placement.base_address + 0xB0U, true) ==
            AotCacheBreakpointProvenance::kProbeSentinel &&
        ClassifyAotCacheBreakpointProvenance(
            placement, placement.base_address + 0xC0U, false) ==
            AotCacheBreakpointProvenance::kUnknown;
    std::cout << "boundary_provenance_probe="
              << (passed ? "true" : "false") << "\n";
    return passed;
}

}  // namespace repiu::tools
