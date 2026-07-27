#ifndef REPIU_PLATFORM_WIN32_AOT_CODE_CACHE_WIN32_H_
#define REPIU_PLATFORM_WIN32_AOT_CODE_CACHE_WIN32_H_

#include "repiu/platform/win32/aot_page_coherence_win32.h"
#include "repiu/platform/win32/aot_boundary_provenance.h"
#include "repiu/platform/win32/aot_cache_address_index.h"
#include "repiu/platform/win32/aot_worker_timing.h"
#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/selector_table.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace repiu::platform::win32
{

struct Win32AotCodeCachePlacement
{
    bool valid = false;
    bool placed = false;
    std::uint32_t base_address = 0;
    std::uint32_t size = 0;
    std::uint32_t capacity = 0;
    std::uint32_t entry_address = 0;
    std::uint32_t windows_error = 0;
    std::vector<runtime::AotAddressMapEntry> address_map;
    std::vector<Win32AotAddressMapState> address_map_states;
    std::vector<runtime::AotCodeCacheFixup> fixups;
    std::vector<runtime::AotIndirectInlineCacheSite>
        indirect_inline_cache_sites;
    std::vector<runtime::AotDbtReturnDispatchSite>
        dbt_return_dispatch_sites;
    std::vector<runtime::AotDbtHleDispatchSite>
        dbt_hle_dispatch_sites;
    std::vector<runtime::AotDbtIndirectDispatchSite>
        dbt_indirect_dispatch_sites;
    std::vector<runtime::AotJumpTableSite> jump_table_sites;
    // Task 264 Phase 3a: natively-translated segment-override accesses, carried
    // so they can be re-resolved (guard selector + folded base) once the guest
    // configures the segment register, or on any later reload.
    std::vector<runtime::AotSegmentOverrideSite> segment_override_sites;
    std::vector<runtime::AotGuardedSegmentPopSite>
        guarded_segment_pop_sites;
    // Incremented directly by guarded cache slots; placement outlives execution.
    volatile std::uint32_t guarded_segment_pop_success_count = 0;
    volatile std::uint32_t guarded_segment_pop_fallback_count = 0;

    // Task 324: O(1) guest-address lookup over address_map, replacing the
    // linear scan Task 323 measured at 87.75% of kAotResume. Treated as a
    // cache: FindAotCacheAddress falls back to the scan when it is stale.
    Win32AotCacheAddressIndex cache_address_index;

    std::vector<Win32AotGuestPageState> guest_pages;
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
    std::uint32_t next_generation = 1;
    std::uint32_t indirect_inline_cache_entry_count =
        runtime::kDefaultAotIndirectInlineCacheEntryCount;
    bool dbt_return_miss_dispatch_enabled = false;
    bool dbt_hle_dispatch_enabled = false;
    bool dbt_indirect_miss_dispatch_enabled = false;
    bool guarded_segment_pop_enabled = false;
    std::string message;
};

struct Win32AotDynamicAppendResult
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

struct Win32AotInlineCachePatchResult
{
    bool attempted = false;
    bool patched = false;
    std::uint32_t cache_miss_address = 0;
    std::uint32_t guest_target = 0;
    std::uint32_t cache_target = 0;
    std::uint32_t windows_error = 0;
    std::string message;
};

bool PlaceWin32AotCodeCache(const runtime::AotCodeCacheImage& image,
                            Win32AotCodeCachePlacement* placement);
void ReleaseWin32AotCodeCache(Win32AotCodeCachePlacement* placement);
bool FindAotGuestAddress(const Win32AotCodeCachePlacement& placement,
                         std::uint32_t cache_address,
                         std::uint32_t* guest_address);
bool FindAotCacheAddress(const Win32AotCodeCachePlacement& placement,
                         std::uint32_t guest_address,
                         std::uint32_t* cache_address);
bool InstallWin32AotProbeSentinel(Win32AotCodeCachePlacement* placement,
                                  std::uint32_t guest_address);
// Task 264 Phase 3a: per-segment resolution the translation path folds into
// natively-emitted segment-override accesses. Indexed by segment register
// (0=ES,2=SS,3=DS,4=FS,5=GS); CS (1) is unused. shadow_address is the absolute
// address of the guest's shadow selector (for the guard's memory compare),
// selector is its value at translation time, and base is its descriptor base
// (0 when the selector is flat/unresolved).
enum class Win32AotSegmentAccessPolicy : std::uint8_t
{
    kUnresolved = 0,
    kNativeFolded,
    kHleLowMemory,
};

struct Win32AotSegmentResolution
{
    std::uint32_t shadow_address = 0;
    std::uint16_t selector = 0;
    std::uint32_t base = 0;
    std::uint32_t limit = 0;
    std::uint32_t flags = 0;
    Win32AotSegmentAccessPolicy policy =
        Win32AotSegmentAccessPolicy::kUnresolved;
};

struct Win32AotSegmentTable
{
    Win32AotSegmentResolution segments[6];
};

struct Win32AotSegmentPatchStats
{
    std::uint32_t native_site_count = 0;
    std::uint32_t hle_site_count = 0;
    std::uint32_t unresolved_site_count = 0;
    std::uint32_t guarded_pop_site_count = 0;
};

// Resolve one live shadow selector into an explicit native/HLE/unresolved
// policy. Selector zero and descriptors wholly backed by DOS low memory must
// retain the HLE boundary; valid higher-memory descriptors may use the existing
// guarded base-folded code path.
void BuildWin32AotSegmentResolution(
    const runtime::SelectorTable& selector_table,
    std::uint32_t shadow_address,
    std::uint16_t selector,
    Win32AotSegmentResolution* resolution);

bool AppendWin32DynamicAotTranslation(
    std::uint32_t runtime_base,
    std::uint32_t runtime_size,
    std::uint32_t guest_entry,
    const std::vector<runtime::AotExcludedGuestRange>& excluded_ranges,
    Win32AotPageWriteWatchSet* write_watch_set,
    Win32AotCodeCachePlacement* placement,
    const Win32AotSegmentTable* segment_table,
    Win32AotDynamicAppendResult* result,
    // Task 328: optional worker-thread phase attribution. Trailing and
    // defaulted so existing call sites, including probes, are unchanged.
    Win32AotWorkerTimingProfile* timing = nullptr);

// Task 264 Phase 3a: re-apply the guard selector and folded base to every carried
// segment-override site, activating any that were left as boundaries (static
// image) and refreshing any whose segment was reloaded. Flips the cache to
// writable, patches, restores execute protection, and flushes. Returns the
// number of sites (re)activated.
std::uint32_t ReResolveWin32AotSegmentOverrides(
    Win32AotCodeCachePlacement* placement,
    const Win32AotSegmentTable* segment_table,
    Win32AotSegmentPatchStats* stats = nullptr);
bool PatchWin32AotIndirectInlineCache(
    Win32AotCodeCachePlacement* placement,
    std::uint32_t cache_miss_address,
    std::uint32_t guest_target,
    std::uint32_t cache_target,
    Win32AotInlineCachePatchResult* result);
}  // namespace repiu::platform::win32

#endif
