#ifndef REPIU_ENGINE_AOT_CODE_CACHE_H_
#define REPIU_ENGINE_AOT_CODE_CACHE_H_

#include "repiu/engine/aot_page_coherence.h"
#include "repiu/engine/aot_boundary_provenance.h"
#include "repiu/engine/aot_cache_address_index.h"
#include "repiu/engine/aot_inline_cache_site_index.h"
#include "repiu/engine/aot_return_dispatch_site_index.h"
#include "repiu/engine/aot_return_patch_policy.h"
#include "repiu/engine/aot_worker_timing.h"
#include "repiu/engine/aot_timer_source_profile.h"
#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_segment_patch.h"
#include "repiu/runtime/selector_table.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace repiu::engine
{

struct AotCodeCachePlacement
{
    bool valid = false;
    bool placed = false;
    std::uint32_t base_address = 0;
    std::uint32_t size = 0;
    std::uint32_t capacity = 0;
    std::uint32_t entry_address = 0;
    std::uint32_t windows_error = 0;
    std::vector<runtime::AotAddressMapEntry> address_map;
    std::vector<AotAddressMapState> address_map_states;
    std::vector<runtime::AotCodeCacheFixup> fixups;
    std::vector<runtime::AotIndirectInlineCacheSite>
        indirect_inline_cache_sites;
    std::vector<runtime::AotDbtReturnDispatchSite>
        dbt_return_dispatch_sites;
    std::vector<runtime::AotDbtHleDispatchSite>
        dbt_hle_dispatch_sites;
    std::vector<runtime::AotDbtIndirectDispatchSite>
        dbt_indirect_dispatch_sites;
    std::vector<runtime::AotDbtDirectEdgeDispatchSite>
        dbt_direct_edge_dispatch_sites;
    std::vector<runtime::AotJumpTableSite> jump_table_sites;
    // Task 264 Phase 3a: natively-translated segment-override accesses, carried
    // so they can be re-resolved (guard selector + folded base) once the guest
    // configures the segment register, or on any later reload.
    std::vector<runtime::AotSegmentOverrideSite> segment_override_sites;
    std::vector<runtime::AotGuardedSegmentPopSite>
        guarded_segment_pop_sites;
    std::vector<runtime::AotGuardedSegmentReadSite>
        guarded_segment_read_sites;
    std::vector<runtime::AotGuardedSegmentLoadSite>
        guarded_segment_load_sites;
    std::vector<runtime::AotTimerSafePointSite> timer_safe_point_sites;
    // Incremented directly by guarded cache slots; placement outlives execution.
    volatile std::uint32_t guarded_segment_pop_success_count = 0;
    volatile std::uint32_t guarded_segment_pop_fallback_count = 0;
    volatile std::uint32_t guarded_segment_load_success_count = 0;
    volatile std::uint32_t guarded_segment_load_fallback_count = 0;

    // Task 324: O(1) guest-address lookup over address_map, replacing the
    // linear scan Task 323 measured at 87.75% of kAotResume. Treated as a
    // cache: FindAotCacheAddress falls back to the scan when it is stale.
    AotCacheAddressIndex cache_address_index;

    // Task 479: O(1) miss-offset lookup over indirect_inline_cache_sites,
    // replacing the linear scan Task 478 measured inside a patch path costing
    // about 75,100 cycles per call. Treated as a cache in the same way:
    // PatchAotIndirectInlineCache falls back to the scan when it is stale.
    AotInlineCacheSiteIndex inline_cache_site_index;

    // Task 480: exact miss-offset lookup for the return miss thunk. The index
    // remains a cache; a stale count falls back to the original scan.
    AotReturnDispatchSiteIndex return_dispatch_site_index;

    // Task 481: per-return-site miss diversity. Sites proven too diverse for
    // the four-entry PIC stop rewriting it while retaining the resolver path.
    AotReturnPatchPolicy return_patch_policy;

    std::vector<AotGuestPageState> guest_pages;
    std::vector<std::uint32_t> retired_guest_addresses;
    std::vector<std::uint32_t> inactive_map_indices;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>>
        inactive_map_indices_by_guest_address;
    std::unordered_map<std::uint32_t, std::uint32_t>
        inactive_map_index_by_cache_offset;
    // Task 289 Stage 3a: O(1) lookup for immutable emitted INT3 origins. Runtime
    // retirement and explicit probes are checked before this structural index.
    std::unordered_map<std::uint32_t, AotCacheBreakpointProvenance>
        breakpoint_provenance_by_cache_offset;
    std::unordered_set<std::uint32_t> timer_safe_point_cache_offsets;
    std::unordered_map<std::uint32_t, std::uint32_t>
        timer_safe_point_guest_source_by_breakpoint_offset;
    AotTimerSourceProfile timer_source_profile;
    std::uint32_t next_generation = 1;
    std::uint32_t indirect_inline_cache_entry_count =
        runtime::kDefaultAotIndirectInlineCacheEntryCount;
    bool dbt_return_miss_dispatch_enabled = false;
    // Task 499. The memo table generated code probes, its emission flag,
    // and the probe sites whose absolute operands point at it. The table
    // is allocated once, at first placement, because its address is baked
    // into emitted code; invalidation clears it in place and never
    // reallocates.
    bool direct_return_table_enabled = false;
    std::uint32_t direct_return_table_bits =
        runtime::kDefaultAotDirectReturnTableBits;
    runtime::AotDirectReturnTable direct_return_table;
    std::vector<runtime::AotDirectReturnProbeSite>
        direct_return_probe_sites;
    bool dbt_hle_dispatch_enabled = false;
    bool dbt_port_io_dispatch_enabled = false;
    bool dbt_segment_override_dispatch_enabled = false;
    bool dbt_indirect_miss_dispatch_enabled = false;
    bool dbt_direct_edge_dispatch_enabled = false;
    bool guarded_segment_pop_enabled = false;
    bool guarded_segment_read_enabled = false;
    bool guarded_segment_load_enabled = false;
    bool timer_safe_points_enabled = false;
    // Task 576. Carried so the dynamic-append path builds the same kind of
    // image the static placement did. Append inherits it here rather than
    // asking the host again, because two answers to one question is how the
    // static cache and the appended blocks would come to disagree.
    bool long_mode_emission_enabled = false;
    // Written by the telemetry poller and consumed only on the guest thread.
    volatile std::uint32_t timer_safe_point_request = 0;
    volatile std::uint32_t timer_safe_point_trap_count = 0;
    volatile std::uint32_t timer_safe_point_injected_count = 0;
    volatile std::uint32_t timer_safe_point_deferred_count = 0;
    std::string message;
};

struct AotDynamicAppendResult
{
    bool attempted = false;
    bool appended = false;
    bool unsafe_failure = false;
    std::uint32_t guest_entry = 0;
    std::uint32_t cache_entry = 0;
    std::uint32_t generation = 0;
    std::uint32_t added_bytes = 0;
    std::uint32_t added_mappings = 0;
    std::uint32_t relinked_entry_count = 0;
    std::vector<std::uint32_t> active_guest_pages;
    std::string message;
};

struct AotInlineCachePatchResult
{
    bool attempted = false;
    bool patched = false;
    std::uint32_t cache_miss_address = 0;
    std::uint32_t guest_target = 0;
    std::uint32_t cache_target = 0;
    std::uint32_t windows_error = 0;
    std::string message;
};

bool PlaceAotCodeCache(const runtime::AotCodeCacheImage& image,
                            AotCodeCachePlacement* placement);
void ReleaseAotCodeCache(AotCodeCachePlacement* placement);
bool FindAotGuestAddress(const AotCodeCachePlacement& placement,
                         std::uint32_t cache_address,
                         std::uint32_t* guest_address);
bool FindAotCacheAddress(const AotCodeCachePlacement& placement,
                         std::uint32_t guest_address,
                         std::uint32_t* cache_address);
bool InstallAotProbeSentinel(AotCodeCachePlacement* placement,
                                  std::uint32_t guest_address);
// Install the same opt-in probe in active entries belonging to the latest
// dynamic append. `added_bytes` identifies the append range in the placement.
bool InstallAotProbeSentinelInLatestAppend(
    AotCodeCachePlacement* placement,
    std::uint32_t guest_address,
    std::uint32_t added_bytes);
// Task 264 Phase 3a: per-segment resolution the translation path folds into
// natively-emitted segment-override accesses. Indexed by segment register
// (0=ES,2=SS,3=DS,4=FS,5=GS); CS (1) is unused. shadow_address is the absolute
// address of the guest's shadow selector (for the guard's memory compare),
// selector is its value at translation time, and base is its descriptor base
// (0 when the selector is flat/unresolved).
// Task 568. These moved to runtime, beside the emitter that produces the sites
// they describe, so the patch loop can be exercised without linking the engine
// -- which on Linux drags OpenGL into a probe built to have no platform layer.
// Aliased here because the names are used across the engine and its probes, and
// renaming those is a separate concern from moving the definition.
using AotSegmentAccessPolicy = runtime::AotSegmentAccessPolicy;
using AotSegmentResolution = runtime::AotSegmentResolution;
using AotSegmentTable = runtime::AotSegmentTable;

struct AotSegmentPatchStats
{
    std::uint32_t native_site_count = 0;
    std::uint32_t hle_site_count = 0;
    std::uint32_t unresolved_site_count = 0;
    std::uint32_t guarded_pop_site_count = 0;
    std::uint32_t guarded_read_site_count = 0;
    std::uint32_t guarded_load_site_count = 0;
};

// Resolve one live shadow selector into an explicit native/HLE/unresolved
// policy. Selector zero and descriptors wholly backed by DOS low memory must
// retain the HLE boundary; valid higher-memory descriptors may use the existing
// guarded base-folded code path.
void BuildAotSegmentResolution(
    const runtime::SelectorTable& selector_table,
    std::uint32_t shadow_address,
    std::uint16_t selector,
    AotSegmentResolution* resolution);

bool AppendDynamicAotTranslation(
    std::uint32_t runtime_base,
    std::uint32_t runtime_size,
    std::uint32_t guest_entry,
    const std::vector<runtime::AotExcludedGuestRange>& excluded_ranges,
    AotPageWriteWatchSet* write_watch_set,
    AotCodeCachePlacement* placement,
    const AotSegmentTable* segment_table,
    AotDynamicAppendResult* result,
    // Task 328: optional worker-thread phase attribution. Trailing and
    // defaulted so existing call sites, including probes, are unchanged.
    AotWorkerTimingProfile* timing = nullptr);

// Task 264 Phase 3a: re-apply the guard selector and folded base to every carried
// segment-override site, activating any that were left as boundaries (static
// image) and refreshing any whose segment was reloaded. Flips the cache to
// writable, patches, restores execute protection, and flushes. Returns the
// number of sites (re)activated.
std::uint32_t ReResolveWin32AotSegmentOverrides(
    AotCodeCachePlacement* placement,
    const AotSegmentTable* segment_table,
    AotSegmentPatchStats* stats = nullptr);
bool PatchAotIndirectInlineCache(
    AotCodeCachePlacement* placement,
    std::uint32_t cache_miss_address,
    std::uint32_t guest_target,
    std::uint32_t cache_target,
    AotInlineCachePatchResult* result);
}  // namespace repiu::engine

#endif
