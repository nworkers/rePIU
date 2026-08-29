#include "repiu/engine/aot_code_cache.h"

#include "repiu/runtime/dos_low_memory.h"
#include "repiu/runtime/aot_translation_plan.h"
#include "repiu/platform/atomic_ops.h"
#include "repiu/platform/virtual_memory.h"
#include "aot/aot_generation_failure_policy.h"
#include "aot/aot_dbt_hle_dispatch.h"
#include "aot/aot_dbt_direct_edge_dispatch.h"
#include "aot/aot_dbt_indirect_dispatch.h"
#include "aot/aot_dbt_return_dispatch.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <limits>
#include <algorithm>
#include <unordered_map>
#include <utility>

namespace repiu::engine
{
namespace
{

void IndexAotBreakpointProvenance(
    const runtime::AotCodeCacheImage& image,
    std::uint32_t append_offset,
    AotCodeCachePlacement* placement)
{
    if (placement == nullptr)
    {
        return;
    }
    auto& index = placement->breakpoint_provenance_by_cache_offset;
    for (const runtime::AotCodeCacheFixup& fixup : image.fixups)
    {
        const std::uint32_t offset = append_offset + fixup.cache_patch_offset;
        if (fixup.kind == runtime::AotFixupKind::kHleBoundary)
        {
            index[offset] = AotCacheBreakpointProvenance::kPlannerHle;
        }
        else if (fixup.kind == runtime::AotFixupKind::kIndirectExit ||
                 (fixup.kind == runtime::AotFixupKind::kConditionalBranch &&
                  !fixup.resolved))
        {
            index[offset] =
                AotCacheBreakpointProvenance::kOtherPlannerFixup;
        }
    }
    for (const runtime::AotSegmentOverrideSite& site :
         image.segment_override_sites)
    {
        // Unresolved/HLE policy replaces the first byte with INT3. A resolved
        // selector mismatch reaches the fixed fallback INT3 at +13.
        index[append_offset + site.cache_offset] =
            AotCacheBreakpointProvenance::kSegmentGuard;
        index[append_offset + site.cache_offset + 13U] =
            AotCacheBreakpointProvenance::kSegmentGuard;
    }
    for (const runtime::AotGuardedSegmentPopSite& site :
         image.guarded_segment_pop_sites)
    {
        index[append_offset + site.cache_offset] =
            AotCacheBreakpointProvenance::kPlannerHle;
        index[append_offset + site.fallback_offset] =
            AotCacheBreakpointProvenance::kPlannerHle;
    }
    for (const runtime::AotGuardedSegmentReadSite& site :
         image.guarded_segment_read_sites)
    {
        index[append_offset + site.cache_offset] =
            AotCacheBreakpointProvenance::kPlannerHle;
        index[append_offset + site.fallback_offset] =
            AotCacheBreakpointProvenance::kPlannerHle;
    }
    for (const runtime::AotIndirectInlineCacheSite& site :
         image.indirect_inline_cache_sites)
    {
        index[append_offset + site.miss_cache_offset] =
            AotCacheBreakpointProvenance::kInlineCacheFallback;
        index[append_offset + site.miss_cache_offset + 1U] =
            AotCacheBreakpointProvenance::kInlineCacheFallback;
    }
    for (const runtime::AotDbtReturnDispatchSite& site :
         image.dbt_return_dispatch_sites)
    {
        index[append_offset + site.fallback_cache_offset + 4U] =
            AotCacheBreakpointProvenance::kInlineCacheFallback;
    }
    for (const runtime::AotDbtHleDispatchSite& site :
         image.dbt_hle_dispatch_sites)
    {
        index[append_offset + site.fallback_cache_offset + 4U] =
            AotCacheBreakpointProvenance::kPlannerHle;
    }
    for (const runtime::AotDbtIndirectDispatchSite& site :
         image.dbt_indirect_dispatch_sites)
    {
        index[append_offset + site.fallback_cache_offset + 4U] =
            AotCacheBreakpointProvenance::kInlineCacheFallback;
    }
    for (const runtime::AotDbtDirectEdgeDispatchSite& site :
         image.dbt_direct_edge_dispatch_sites)
    {
        index[append_offset + site.fallback_cache_offset] =
            AotCacheBreakpointProvenance::kOtherPlannerFixup;
    }
    for (const runtime::AotJumpTableSite& site : image.jump_table_sites)
    {
        index[append_offset + site.fallback_offset] =
            AotCacheBreakpointProvenance::kJumpTableFallback;
    }
}

bool ResolveAotTimerSafePoints(
    const runtime::AotCodeCacheImage& image,
    std::uint8_t* image_bytes,
    AotCodeCachePlacement* placement)
{
    if (image.timer_safe_point_sites.empty())
    {
        return true;
    }
    if (image_bytes == nullptr || placement == nullptr)
    {
        return false;
    }
    const std::uintptr_t request_value = reinterpret_cast<std::uintptr_t>(
        &placement->timer_safe_point_request);
    if (request_value > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    const std::uint32_t request_address =
        static_cast<std::uint32_t>(request_value);
    for (const runtime::AotTimerSafePointSite& site :
         image.timer_safe_point_sites)
    {
        std::memcpy(image_bytes + site.request_address_offset,
                    &request_address, sizeof(request_address));
    }
    return true;
}

// Jump-table slots carry image-relative offsets; once the image bytes have a
// final absolute base the displacement and every table entry become absolute
// cache addresses. Targets missing from the image fall back to the slot's
// INT3 so the dispatcher re-executes the original guest branch.
void ResolveAotJumpTables(const runtime::AotCodeCacheImage& image,
                               std::uint8_t* image_bytes,
                               std::uint32_t image_absolute_base)
{
    if (image.jump_table_sites.empty() || image_bytes == nullptr)
    {
        return;
    }
    std::unordered_map<std::uint32_t, std::uint32_t> guest_to_cache;
    guest_to_cache.reserve(image.address_map.size());
    for (const runtime::AotAddressMapEntry& entry : image.address_map)
    {
        guest_to_cache.emplace(entry.guest_address, entry.cache_offset);
    }
    for (const runtime::AotJumpTableSite& site : image.jump_table_sites)
    {
        const std::uint32_t table_address =
            image_absolute_base + site.table_cache_offset;
        std::memcpy(image_bytes + site.displacement_patch_offset,
                    &table_address, sizeof(table_address));
        for (std::size_t index = 0;
             index < site.guest_targets.size(); ++index)
        {
            const auto target =
                guest_to_cache.find(site.guest_targets[index]);
            const std::uint32_t entry_address = image_absolute_base +
                (target != guest_to_cache.end() ? target->second
                                                : site.fallback_offset);
            std::memcpy(image_bytes + site.table_cache_offset + index * 4U,
                        &entry_address, sizeof(entry_address));
        }
    }
}

// Task 499. Point every emitted probe at the one memo table. The table is
// allocated on first use and never reallocated afterwards, because its address
// is baked into cache bytes; invalidation clears it in place.
bool ResolveAotDirectReturnProbes(
    const runtime::AotCodeCacheImage& image,
    std::uint8_t* image_bytes,
    std::size_t byte_count,
    AotCodeCachePlacement* placement)
{
    if (image.direct_return_probe_sites.empty())
    {
        return true;
    }
    if (image_bytes == nullptr || placement == nullptr)
    {
        return false;
    }
    if (placement->direct_return_table.entries.empty())
    {
        runtime::ResetAotDirectReturnTable(
            &placement->direct_return_table,
            image.direct_return_table_bits);
    }
    if (placement->direct_return_table.entries.empty())
    {
        return false;
    }
    const std::uintptr_t key_value = reinterpret_cast<std::uintptr_t>(
        placement->direct_return_table.entries.data());
    const std::uintptr_t counter_value = reinterpret_cast<std::uintptr_t>(
        &placement->direct_return_table.hit_count);
    if (key_value > std::numeric_limits<std::uint32_t>::max() ||
        counter_value > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    for (const runtime::AotDirectReturnProbeSite& site :
         image.direct_return_probe_sites)
    {
        if (!runtime::PatchAotDirectReturnProbe(
                image_bytes, byte_count, site,
                static_cast<std::uint32_t>(key_value),
                placement->direct_return_table.mask,
                static_cast<std::uint32_t>(counter_value)))
        {
            return false;
        }
    }
    return true;
}

bool ResolveAotDbtReturnDispatchSites(
    const runtime::AotCodeCacheImage& image,
    std::uint8_t* image_bytes,
    std::uint32_t image_absolute_base)
{
    if (image.dbt_return_dispatch_sites.empty())
    {
        return true;
    }
    const std::uintptr_t thunk_value = reinterpret_cast<std::uintptr_t>(
        GetAotDbtReturnMissThunkAddress());
    if (image_bytes == nullptr || thunk_value == 0U ||
        thunk_value > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    const std::uint32_t thunk = static_cast<std::uint32_t>(thunk_value);
    for (const runtime::AotDbtReturnDispatchSite& site :
         image.dbt_return_dispatch_sites)
    {
        const std::uint32_t miss_address =
            image_absolute_base + site.miss_cache_offset;
        std::memcpy(image_bytes + site.miss_address_immediate_offset,
                    &miss_address, sizeof(miss_address));
        const std::uint32_t next_instruction = image_absolute_base +
            site.thunk_displacement_offset + 4U;
        const std::int32_t displacement = static_cast<std::int32_t>(
            thunk - next_instruction);
        std::memcpy(image_bytes + site.thunk_displacement_offset,
                    &displacement, sizeof(displacement));
    }
    return true;
}

bool ResolveAotDbtDirectEdgeDispatchSites(
    const runtime::AotCodeCacheImage& image,
    std::uint8_t* image_bytes,
    std::uint32_t image_absolute_base)
{
    if (image.dbt_direct_edge_dispatch_sites.empty())
    {
        return true;
    }
    const std::uintptr_t thunk_value = reinterpret_cast<std::uintptr_t>(
        GetAotDbtDirectEdgeDispatchThunkAddress());
    if (image_bytes == nullptr || thunk_value == 0U ||
        thunk_value > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    const std::uint32_t thunk = static_cast<std::uint32_t>(thunk_value);
    for (const runtime::AotDbtDirectEdgeDispatchSite& site :
         image.dbt_direct_edge_dispatch_sites)
    {
        const std::uint32_t dispatch_address =
            image_absolute_base + site.dispatch_cache_offset;
        std::memcpy(
            image_bytes + site.dispatch_address_immediate_offset,
            &dispatch_address, sizeof(dispatch_address));
        const std::uint32_t next_instruction = image_absolute_base +
            site.thunk_displacement_offset + 4U;
        const std::int32_t displacement = static_cast<std::int32_t>(
            thunk - next_instruction);
        std::memcpy(image_bytes + site.thunk_displacement_offset,
                    &displacement, sizeof(displacement));
    }
    return true;
}
bool ResolveAotDbtHleDispatchSites(
    const runtime::AotCodeCacheImage& image,
    std::uint8_t* image_bytes,
    std::uint32_t image_absolute_base)
{
    if (image.dbt_hle_dispatch_sites.empty())
    {
        return true;
    }
    const std::uintptr_t thunk_value = reinterpret_cast<std::uintptr_t>(
        GetAotDbtHleDispatchThunkAddress());
    if (image_bytes == nullptr || thunk_value == 0U ||
        thunk_value > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    const std::uint32_t thunk = static_cast<std::uint32_t>(thunk_value);
    for (const runtime::AotDbtHleDispatchSite& site :
         image.dbt_hle_dispatch_sites)
    {
        const std::uint32_t dispatch_address =
            image_absolute_base + site.dispatch_cache_offset;
        std::memcpy(
            image_bytes + site.dispatch_address_immediate_offset,
            &dispatch_address, sizeof(dispatch_address));
        const std::uint32_t next_instruction = image_absolute_base +
            site.thunk_displacement_offset + 4U;
        const std::int32_t displacement = static_cast<std::int32_t>(
            thunk - next_instruction);
        std::memcpy(image_bytes + site.thunk_displacement_offset,
                    &displacement, sizeof(displacement));
    }
    return true;
}

bool ResolveAotDbtIndirectDispatchSites(
    const runtime::AotCodeCacheImage& image,
    std::uint8_t* image_bytes,
    std::uint32_t image_absolute_base)
{
    if (image.dbt_indirect_dispatch_sites.empty())
    {
        return true;
    }
    const std::uintptr_t thunk_value = reinterpret_cast<std::uintptr_t>(
        GetAotDbtIndirectMissThunkAddress());
    if (image_bytes == nullptr || thunk_value == 0U ||
        thunk_value > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    const std::uint32_t thunk = static_cast<std::uint32_t>(thunk_value);
    for (const runtime::AotDbtIndirectDispatchSite& site :
         image.dbt_indirect_dispatch_sites)
    {
        const std::uint32_t miss_address =
            image_absolute_base + site.miss_cache_offset;
        std::memcpy(image_bytes + site.miss_address_immediate_offset,
                    &miss_address, sizeof(miss_address));
        const std::uint32_t next_instruction = image_absolute_base +
            site.thunk_displacement_offset + 4U;
        const std::int32_t displacement = static_cast<std::int32_t>(
            thunk - next_instruction);
        std::memcpy(image_bytes + site.thunk_displacement_offset,
                    &displacement, sizeof(displacement));
    }
    return true;
}

// Task 264 Phase 3a: fold each natively-translated segment-override access's
// selector and base into the emitted guard and displacement, in place, while the
// image bytes are still writable. Offsets are image-relative, matching the
// pointer the caller passes (the image's placed location).
void ResolveAotSegmentOverrides(
    const runtime::AotCodeCacheImage& image,
    std::uint8_t* image_bytes,
    const AotSegmentTable* segment_table)
{
    if (image.segment_override_sites.empty() || image_bytes == nullptr)
    {
        return;
    }
    for (const runtime::AotSegmentOverrideSite& site :
         image.segment_override_sites)
    {
        const std::uint8_t seg = site.segment_register;
        if (segment_table == nullptr || seg >= 6U ||
            segment_table->segments[seg].shadow_address == 0U)
        {
            // Cannot resolve: preserve the original INT3/VEH semantic path.
            image_bytes[site.cache_offset] = 0xCCU;
            continue;
        }
        const AotSegmentResolution& resolution =
            segment_table->segments[seg];
        if (resolution.policy == AotSegmentAccessPolicy::kHleLowMemory)
        {
            if (site.dispatch_cache_offset == 0U)
            {
                image_bytes[site.cache_offset] = 0xCCU;
            }
            else
            {
                image_bytes[site.cache_offset] = 0xE9U;
                const std::int32_t relative = static_cast<std::int32_t>(
                    site.dispatch_cache_offset - (site.cache_offset + 5U));
                std::memcpy(image_bytes + site.cache_offset + 1U,
                            &relative, sizeof(relative));
            }
            continue;
        }
        if (resolution.policy != AotSegmentAccessPolicy::kNativeFolded)
        {
            image_bytes[site.cache_offset] = 0xCCU;
            continue;
        }
        image_bytes[site.cache_offset] = 0x9CU;
        image_bytes[site.cache_offset + 1U] = 0x66U;
        image_bytes[site.cache_offset + 2U] = 0x81U;
        image_bytes[site.cache_offset + 3U] = 0x3DU;
        std::memcpy(image_bytes + site.guard_address_offset,
                    &resolution.shadow_address, sizeof(std::uint32_t));
        std::memcpy(image_bytes + site.guard_selector_offset,
                    &resolution.selector, sizeof(std::uint16_t));
        const std::uint32_t displacement =
            static_cast<std::uint32_t>(site.original_displacement) +
            resolution.base;
        std::memcpy(image_bytes + site.displacement_offset, &displacement,
                    sizeof(displacement));
    }
}

void ResolveAotGuardedSegmentPops(
    const runtime::AotCodeCacheImage& image,
    std::uint8_t* image_bytes,
    AotCodeCachePlacement* placement,
    const AotSegmentTable* segment_table)
{
    if (image.guarded_segment_pop_sites.empty() || image_bytes == nullptr ||
        placement == nullptr)
    {
        return;
    }
    const std::uintptr_t success_address = reinterpret_cast<std::uintptr_t>(
        &placement->guarded_segment_pop_success_count);
    const std::uintptr_t fallback_address = reinterpret_cast<std::uintptr_t>(
        &placement->guarded_segment_pop_fallback_count);
    for (const runtime::AotGuardedSegmentPopSite& site :
         image.guarded_segment_pop_sites)
    {
        const std::uint8_t seg = site.segment_register;
        if (segment_table == nullptr || seg >= 6U ||
            segment_table->segments[seg].shadow_address == 0U ||
            success_address > UINT32_MAX || fallback_address > UINT32_MAX)
        {
            image_bytes[site.cache_offset] = 0xCCU;
            continue;
        }
        image_bytes[site.cache_offset] = 0x9CU;
        const std::uint32_t shadow_address =
            segment_table->segments[seg].shadow_address;
        const std::uint32_t success =
            static_cast<std::uint32_t>(success_address);
        const std::uint32_t fallback =
            static_cast<std::uint32_t>(fallback_address);
        std::memcpy(image_bytes + site.shadow_address_offset,
                    &shadow_address, sizeof(shadow_address));
        std::memcpy(image_bytes + site.success_counter_address_offset,
                    &success, sizeof(success));
        std::memcpy(image_bytes + site.fallback_counter_address_offset,
                    &fallback, sizeof(fallback));
    }
}

void ResolveAotGuardedSegmentLoads(
    const runtime::AotCodeCacheImage& image,
    std::uint8_t* image_bytes,
    AotCodeCachePlacement* placement,
    const AotSegmentTable* segment_table)
{
    if (image.guarded_segment_load_sites.empty() || image_bytes == nullptr ||
        placement == nullptr)
    {
        return;
    }
    const std::uintptr_t success_address = reinterpret_cast<std::uintptr_t>(
        &placement->guarded_segment_load_success_count);
    const std::uintptr_t fallback_address = reinterpret_cast<std::uintptr_t>(
        &placement->guarded_segment_load_fallback_count);
    for (const runtime::AotGuardedSegmentLoadSite& site :
         image.guarded_segment_load_sites)
    {
        const std::uint8_t seg = site.segment_register;
        if (segment_table == nullptr || seg >= 6U ||
            segment_table->segments[seg].shadow_address == 0U ||
            success_address > UINT32_MAX || fallback_address > UINT32_MAX)
        {
            image_bytes[site.cache_offset] = 0xCCU;
            continue;
        }
        image_bytes[site.cache_offset] = 0x9CU;
        const std::uint32_t shadow_address =
            segment_table->segments[seg].shadow_address;
        const std::uint32_t success =
            static_cast<std::uint32_t>(success_address);
        const std::uint32_t fallback =
            static_cast<std::uint32_t>(fallback_address);
        std::memcpy(image_bytes + site.shadow_address_offset,
                    &shadow_address, sizeof(shadow_address));
        std::memcpy(image_bytes + site.success_counter_address_offset,
                    &success, sizeof(success));
        std::memcpy(image_bytes + site.fallback_counter_address_offset,
                    &fallback, sizeof(fallback));
    }
}

void ResolveAotGuardedSegmentReads(
    const runtime::AotCodeCacheImage& image,
    std::uint8_t* image_bytes,
    const AotSegmentTable* segment_table)
{
    if (image.guarded_segment_read_sites.empty() || image_bytes == nullptr)
    {
        return;
    }
    for (const runtime::AotGuardedSegmentReadSite& site :
         image.guarded_segment_read_sites)
    {
        const std::uint8_t seg = site.segment_register;
        if (segment_table == nullptr || seg >= 6U ||
            segment_table->segments[seg].shadow_address == 0U)
        {
            image_bytes[site.cache_offset] = 0xCCU;
            continue;
        }
        image_bytes[site.cache_offset] = 0x9CU;
        const std::uint32_t shadow_address =
            segment_table->segments[seg].shadow_address;
        std::memcpy(image_bytes + site.shadow_address_offset,
                    &shadow_address, sizeof(shadow_address));
        std::memcpy(image_bytes + site.load_shadow_address_offset,
                    &shadow_address, sizeof(shadow_address));
    }
}
// Task 329: referencing the live arena gives up the failure return that
// `ReadProcessMemory` provided, so the range is checked once per process
// instead of copied once per translation. One check is enough because nothing
// decommits guest memory (every release is a whole-reservation teardown) and
// guest page protection only moves among readable values, never no-access.
//
// Worker-thread only, like the rest of this path, so the cache is plain state.
bool VerifyGuestArenaDirectlyReadable(std::uint32_t runtime_base,
                                           std::uint32_t runtime_size)
{
    static std::uint32_t verified_base = 0;
    static std::uint32_t verified_size = 0;
    static bool verified_result = false;
    if (verified_size != 0U && verified_base == runtime_base &&
        verified_size == runtime_size)
    {
        return verified_result;
    }

    const std::uint64_t end =
        static_cast<std::uint64_t>(runtime_base) + runtime_size;
    bool covered = true;
    std::uint64_t address = runtime_base;
    while (address < end)
    {
        const repiu::platform::MemoryRegion region =
            repiu::platform::QueryMemory(reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(address)));
        if (!region.valid || !region.committed || !region.readable)
        {
            covered = false;
            break;
        }
        const std::uint64_t region_end =
            static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(region.base)) + region.size;
        if (region_end <= address)
        {
            covered = false;
            break;
        }
        address = region_end;
    }

    verified_base = runtime_base;
    verified_size = runtime_size;
    verified_result = covered;
    return covered;
}

// Task 413. An inline-cache patch writes fourteen bytes, but the patch path
// flipped the protection of the whole 16 MB cache twice to do it. Measured on
// this host, that pair costs about 4.2 ms (11.5 M cycles), and a stalled
// pumpit3 run makes over 12,288 patches -- the dominant cost in the run. The
// window below covers only the pages actually written. `REPIU_AOT_PATCH_WIDE_PROTECT=1`
// restores the whole-cache behaviour so the two can be compared in one binary.
// See docs/design/20260804-413-aot-patch-protection-window.md.
struct AotCachePatchWindow
{
    void* base = nullptr;
    std::size_t size = 0;
};

bool AotPatchWideProtectEnabled()
{
    static const bool enabled = [] {
        const char* value = std::getenv("REPIU_AOT_PATCH_WIDE_PROTECT");
        return value != nullptr && std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

std::size_t AotPatchPageSize()
{
    static const std::size_t page_size = repiu::platform::SystemPageSize();
    return page_size;
}

// Task 417. `CanActivateAotAddressMapEntry` refuses an entry that spans a
// retired page unless that page is the requested one. pumpit3's entry at
// `0x0301DFFE` straddles the boundary into `0x0301E000`, so once the neighbour
// is retired the entry can never be re-translated, execution falls back to the
// arena, and the guest is stepped 2.36 M times without a way back (Task 416).
// The image being appended here is freshly translated from current guest bytes,
// and `RegisterAddressMapPages` records the entry under *every* page it spans,
// so a later write to either page still retires it. The only state that must
// still block activation is a quarantined page.
// `REPIU_AOT_STRICT_SPANNING_ENTRY=1` restores the old rule.
// See docs/design/20260804-417-spanning-entry-activation.md.
bool AotStrictSpanningEntryEnabled()
{
    static const bool enabled = [] {
        const char* value = std::getenv("REPIU_AOT_STRICT_SPANNING_ENTRY");
        return value != nullptr && std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

bool EntrySpansQuarantinedPage(const AotCodeCachePlacement& placement,
                               const runtime::AotAddressMapEntry& entry)
{
    if (entry.guest_length == 0U)
    {
        return true;
    }
    const std::uint64_t last =
        static_cast<std::uint64_t>(entry.guest_address) +
        entry.guest_length - 1U;
    if (last > std::numeric_limits<std::uint32_t>::max())
    {
        return true;
    }
    const std::uint32_t first_page = AotGuestPage(entry.guest_address);
    const std::uint32_t last_page =
        AotGuestPage(static_cast<std::uint32_t>(last));
    for (std::uint32_t page = first_page;; page += 0x1000U)
    {
        if (IsAotGuestPageQuarantined(placement, page))
        {
            return true;
        }
        if (page == last_page || page > 0xFFFFEFFFU)
        {
            break;
        }
    }
    return false;
}

// Page-aligned cover of [first_offset, last_offset_exclusive), clamped to the
// cache. Falls back to the whole cache when the range is unusable, so a
// mis-computed window can only be as wide as the old behaviour, never narrower
// than what is written.
AotCachePatchWindow ComputeAotCachePatchWindow(
    const AotCodeCachePlacement& placement,
    std::uint32_t first_offset,
    std::uint32_t last_offset_exclusive)
{
    auto* cache = reinterpret_cast<std::uint8_t*>(
        static_cast<std::uintptr_t>(placement.base_address));
    const AotCachePatchWindow whole{cache,
                                    static_cast<std::size_t>(
                                        placement.capacity)};
    if (AotPatchWideProtectEnabled() ||
        last_offset_exclusive <= first_offset ||
        last_offset_exclusive > placement.capacity)
    {
        return whole;
    }
    const std::size_t page_size = AotPatchPageSize();
    const std::size_t start = (first_offset / page_size) * page_size;
    std::size_t end =
        ((last_offset_exclusive + page_size - 1U) / page_size) * page_size;
    if (end > placement.capacity)
    {
        end = placement.capacity;
    }
    if (end <= start)
    {
        return whole;
    }
    return AotCachePatchWindow{cache + start, end - start};
}
}  // namespace

void BuildAotSegmentResolution(
    const runtime::SelectorTable& selector_table,
    std::uint32_t shadow_address,
    std::uint16_t selector,
    AotSegmentResolution* resolution)
{
    if (resolution == nullptr)
    {
        return;
    }
    *resolution = AotSegmentResolution{};
    resolution->shadow_address = shadow_address;
    resolution->selector = selector;
    if (shadow_address == 0U)
    {
        return;
    }
    if (selector == 0U)
    {
        resolution->policy = AotSegmentAccessPolicy::kHleLowMemory;
        return;
    }
    const runtime::GuestDescriptor* descriptor =
        runtime::FindDescriptor(selector_table, selector);
    if (descriptor == nullptr)
    {
        return;
    }
    resolution->base = descriptor->base;
    resolution->limit = descriptor->limit;
    resolution->flags = descriptor->flags;
    const std::uint64_t end =
        static_cast<std::uint64_t>(descriptor->base) + descriptor->limit;
    if (end > UINT32_MAX)
    {
        return;
    }
    if (descriptor->base < runtime::kDosLowMemorySize &&
        end < runtime::kDosLowMemorySize)
    {
        resolution->policy = AotSegmentAccessPolicy::kHleLowMemory;
        return;
    }
    resolution->policy = AotSegmentAccessPolicy::kNativeFolded;
}

bool PlaceAotCodeCache(const runtime::AotCodeCacheImage& image,
                            AotCodeCachePlacement* placement)
{
    if (placement == nullptr)
    {
        return false;
    }
    *placement = AotCodeCachePlacement{};
    InitializeAotTimerSourceProfile(
        AotTimerSourceProfileEnabled(),
        &placement->timer_source_profile);
    if (!image.valid || image.executable || image.bytes.empty())
    {
        placement->message = "AOT byte image is not ready for placement";
        return false;
    }
    if (image.bytes.size() > std::numeric_limits<std::uint32_t>::max())
    {
        placement->message = "AOT code cache exceeds Win32 address size";
        return false;
    }
    constexpr std::uint32_t kDynamicCacheCapacity = 16U * 1024U * 1024U;
    const std::size_t capacity = std::max<std::size_t>(
        image.bytes.size(), kDynamicCacheCapacity);
    const repiu::platform::MemoryReservation reservation =
        repiu::platform::ReserveMemory(
            nullptr, capacity, true,
            repiu::platform::MemoryProtection::kReadWrite);
    void* memory = reservation.base;
    placement->valid = true;
    if (memory == nullptr)
    {
        placement->windows_error = reservation.error;
        placement->message = "AOT code-cache allocation failed";
        return true;
    }
    const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(memory);
    if (base > std::numeric_limits<std::uint32_t>::max())
    {
        repiu::platform::ReleaseMemory(memory, capacity);
        placement->message = "AOT code cache is outside the x86 address range";
        return true;
    }
    std::memcpy(memory, image.bytes.data(), image.bytes.size());
    if (!ResolveAotTimerSafePoints(
            image, static_cast<std::uint8_t*>(memory), placement))
    {
        repiu::platform::ReleaseMemory(memory, capacity);
        placement->message = "AOT timer safe-point request is unavailable";
        return true;
    }
    ResolveAotJumpTables(image, static_cast<std::uint8_t*>(memory),
                              static_cast<std::uint32_t>(base));
    // The static placement has no live segment table, so any segment-override
    // sites fall back to boundaries (single-step) instead of a faulting guard.
    ResolveAotSegmentOverrides(image, static_cast<std::uint8_t*>(memory),
                                    nullptr);
    ResolveAotGuardedSegmentPops(
        image, static_cast<std::uint8_t*>(memory), placement, nullptr);
    ResolveAotGuardedSegmentReads(
        image, static_cast<std::uint8_t*>(memory), nullptr);
    ResolveAotGuardedSegmentLoads(
        image, static_cast<std::uint8_t*>(memory), placement, nullptr);
    if (!ResolveAotDbtReturnDispatchSites(
            image, static_cast<std::uint8_t*>(memory),
            static_cast<std::uint32_t>(base)))
    {
        repiu::platform::ReleaseMemory(memory, capacity);
        placement->message = "AOT-DBT return thunk is unavailable";
        return true;
    }
    if (!ResolveAotDirectReturnProbes(
            image, static_cast<std::uint8_t*>(memory), image.bytes.size(),
            placement))
    {
        repiu::platform::ReleaseMemory(memory, capacity);
        placement->message = "AOT direct-return table is unavailable";
        return true;
    }
    if (!ResolveAotDbtDirectEdgeDispatchSites(
            image, static_cast<std::uint8_t*>(memory),
            static_cast<std::uint32_t>(base)))
    {
        repiu::platform::ReleaseMemory(memory, capacity);
        placement->message =
            "AOT-DBT direct-edge thunk is unavailable";
        return true;
    }
    if (!ResolveAotDbtHleDispatchSites(
            image, static_cast<std::uint8_t*>(memory),
            static_cast<std::uint32_t>(base)))
    {
        repiu::platform::ReleaseMemory(memory, capacity);
        placement->message = "AOT-DBT HLE thunk is unavailable";
        return true;
    }
    if (!ResolveAotDbtIndirectDispatchSites(
            image, static_cast<std::uint8_t*>(memory),
            static_cast<std::uint32_t>(base)))
    {
        repiu::platform::ReleaseMemory(memory, capacity);
        placement->message = "AOT-DBT indirect thunk is unavailable";
        return true;
    }
    repiu::platform::MemoryProtection old_protection =
        repiu::platform::MemoryProtection::kNoAccess;
    if (!repiu::platform::ProtectMemory(
            memory, capacity,
            repiu::platform::MemoryProtection::kExecuteRead,
            &old_protection))
    {
        placement->windows_error = static_cast<std::uint32_t>(errno);
        repiu::platform::ReleaseMemory(memory, capacity);
        placement->message = "AOT code-cache execute protection failed";
        return true;
    }
    repiu::platform::FlushInstructionCacheRange(memory, image.bytes.size());
    placement->base_address = static_cast<std::uint32_t>(base);
    placement->size = static_cast<std::uint32_t>(image.bytes.size());
    placement->capacity = static_cast<std::uint32_t>(capacity);
    placement->entry_address = placement->base_address +
                               image.entry_cache_offset;
    placement->address_map = image.address_map;
    InitializeAotPageCoherence(placement, 1U);
    placement->fixups = image.fixups;
    placement->indirect_inline_cache_sites =
        image.indirect_inline_cache_sites;
    placement->dbt_return_dispatch_sites = image.dbt_return_dispatch_sites;
    SyncAotReturnPatchPolicy(placement);
    placement->dbt_hle_dispatch_sites = image.dbt_hle_dispatch_sites;
    placement->dbt_indirect_dispatch_sites =
        image.dbt_indirect_dispatch_sites;
    placement->dbt_direct_edge_dispatch_sites =
        image.dbt_direct_edge_dispatch_sites;
    placement->jump_table_sites = image.jump_table_sites;
    placement->segment_override_sites = image.segment_override_sites;
    placement->guarded_segment_pop_sites = image.guarded_segment_pop_sites;
    placement->guarded_segment_read_sites = image.guarded_segment_read_sites;
    placement->guarded_segment_load_sites = image.guarded_segment_load_sites;
    placement->timer_safe_point_sites = image.timer_safe_point_sites;
    placement->indirect_inline_cache_entry_count =
        image.indirect_inline_cache_entry_count;
    placement->dbt_return_miss_dispatch_enabled =
        image.dbt_return_miss_dispatch_enabled;
    placement->dbt_hle_dispatch_enabled =
        image.dbt_hle_dispatch_enabled;
    placement->dbt_port_io_dispatch_enabled =
        image.dbt_port_io_dispatch_enabled;
    placement->dbt_segment_override_dispatch_enabled =
        image.dbt_segment_override_dispatch_enabled;
    placement->dbt_indirect_miss_dispatch_enabled =
        image.dbt_indirect_miss_dispatch_enabled;
    placement->dbt_direct_edge_dispatch_enabled =
        image.dbt_direct_edge_dispatch_enabled;
    placement->guarded_segment_pop_enabled =
        image.guarded_segment_pop_enabled;
    placement->guarded_segment_read_enabled =
        image.guarded_segment_read_enabled;
    placement->guarded_segment_load_enabled =
        image.guarded_segment_load_enabled;
    placement->timer_safe_points_enabled = image.timer_safe_points_enabled;
    placement->direct_return_table_enabled =
        image.direct_return_table_enabled;
    placement->direct_return_table_bits = image.direct_return_table_bits;
    placement->direct_return_probe_sites = image.direct_return_probe_sites;
    for (const runtime::AotTimerSafePointSite& site :
         image.timer_safe_point_sites)
    {
        placement->timer_safe_point_cache_offsets.insert(
            site.breakpoint_offset);
        placement
            ->timer_safe_point_guest_source_by_breakpoint_offset[
                site.breakpoint_offset] = site.guest_source;
    }
    IndexAotBreakpointProvenance(image, 0U, placement);
    placement->placed = true;
    placement->message = "AOT code cache placed as Win32 execute-read memory";
    return true;
}

bool AppendDynamicAotTranslation(
    std::uint32_t runtime_base,
    std::uint32_t runtime_size,
    std::uint32_t guest_entry,
    const std::vector<runtime::AotExcludedGuestRange>& excluded_ranges,
    AotPageWriteWatchSet* write_watch_set,
    AotCodeCachePlacement* placement,
    const AotSegmentTable* segment_table,
    AotDynamicAppendResult* result,
    AotWorkerTimingProfile* timing)
{
    if (placement == nullptr || result == nullptr)
    {
        return false;
    }
    *result = AotDynamicAppendResult{};
    result->attempted = true;
    result->guest_entry = guest_entry;

    // Task 328: phases are accumulated on every exit path, including the early
    // returns below, so a failed append still reports what it spent. Worker
    // thread only, so no atomics.
    AotAppendPhaseSample append_phases;
    AotAppendScaleSample append_scale;
    // Placement runs from its start to whichever exit is taken, so it is closed
    // by the same destructor that commits the sample.
    std::uint64_t placement_phase_start = 0;
    struct AppendPhaseCommit
    {
        AotWorkerTimingProfile* timing;
        AotAppendPhaseSample* phases;
        const AotAppendScaleSample* scale;
        const std::uint64_t* placement_start;
        ~AppendPhaseCommit()
        {
            if (timing != nullptr && *placement_start != 0U)
            {
                phases->placement_cycles = AotWorkerTimingDelta(
                    timing, *placement_start, ReadAotWorkerTimingCycles());
            }
            RecordAotAppendPhases(timing, *phases);
            RecordAotAppendScale(timing, *scale);
        }
    } append_phase_commit{timing, &append_phases, &append_scale,
                          &placement_phase_start};
    const auto phase_now = [timing]() {
        return timing != nullptr ? ReadAotWorkerTimingCycles() : 0U;
    };
    if (!placement->placed || runtime_size == 0U ||
        guest_entry < runtime_base || guest_entry - runtime_base >= runtime_size)
    {
        result->message = "dynamic AOT target is outside the guest arena";
        return true;
    }
    // Task 329 (was Task 328 phase 1): the arena is referenced, not copied. The
    // visible range is still the whole arena, so the plan is byte-for-byte what
    // the snapshot produced; what disappears is a 133.8MB zero-fill, copy, and
    // free per translation. This is safe only because the guest thread is
    // blocked in the synchronous rendezvous and no other thread writes the
    // arena — see docs/design/20260727-329 sections 2 and 8, and the contract on
    // `RelocatedRuntimeObject::external_bytes`.
    const std::uint64_t arena_view_start = phase_now();
    const bool arena_readable =
        VerifyGuestArenaDirectlyReadable(runtime_base, runtime_size);
    runtime::RelocatedRuntimeImage arena_view;
    arena_view.valid = true;
    arena_view.relocated_image_base = runtime_base;
    arena_view.relocated_entry_linear_address = guest_entry;
    runtime::RelocatedRuntimeObject object;
    object.relocated_base_address = runtime_base;
    object.virtual_size = runtime_size;
    object.external_bytes = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(runtime_base));
    object.external_byte_count = runtime_size;
    append_phases.arena_snapshot_cycles =
        AotWorkerTimingDelta(timing, arena_view_start, phase_now());
    // Nothing is copied any more, so the scale axis reports the bytes actually
    // copied, which is zero. A nonzero value here would mean a copy returned.
    append_scale.snapshot_bytes = 0U;
    if (!arena_readable)
    {
        result->message =
            "guest arena is not fully readable for direct translation";
        return true;
    }
    arena_view.objects.push_back(std::move(object));
    runtime::AotTranslationPlan plan;
    runtime::AotCodeCacheImage image;
    runtime::AotCodeCacheBuildOptions build_options;
    build_options.indirect_inline_cache_entry_count =
        placement->indirect_inline_cache_entry_count;
    build_options.enable_dbt_return_miss_dispatch =
        placement->dbt_return_miss_dispatch_enabled;
    build_options.enable_dbt_hle_dispatch =
        placement->dbt_hle_dispatch_enabled;
    build_options.enable_dbt_port_io_dispatch =
        placement->dbt_port_io_dispatch_enabled;
    build_options.enable_dbt_segment_override_dispatch =
        placement->dbt_segment_override_dispatch_enabled;
    build_options.enable_dbt_indirect_miss_dispatch =
        placement->dbt_indirect_miss_dispatch_enabled;
    build_options.enable_dbt_direct_edge_dispatch =
        placement->dbt_direct_edge_dispatch_enabled;
    build_options.enable_guarded_segment_pop =
        placement->guarded_segment_pop_enabled;
    build_options.enable_guarded_segment_read =
        placement->guarded_segment_read_enabled;
    build_options.enable_guarded_segment_load =
        placement->guarded_segment_load_enabled;
    build_options.enable_timer_safe_points =
        placement->timer_safe_points_enabled;
    build_options.enable_direct_return_table =
        placement->direct_return_table_enabled;
    build_options.direct_return_table_bits =
        placement->direct_return_table_bits;
    // Task 328 phases 2 and 3. Split out of the shared short-circuit into
    // sequential locals; the image build still runs only when the plan build
    // succeeded, exactly as before.
    // Task 330: the builder attributes its own stages when profiling is on, so
    // `plan_build_cycles` can be decomposed without this layer knowing how the
    // builder is structured. Accumulation happens after the phase boundary
    // below, so it is never counted inside the phase it describes.
    runtime::AotPlanBuildProfile plan_profile;
    const std::uint64_t plan_start = phase_now();
    const bool plan_built = runtime::BuildAotTranslationPlanFromEntry(
        arena_view, guest_entry, excluded_ranges, &plan,
        timing != nullptr ? &plan_profile : nullptr);
    const std::uint64_t emit_start = phase_now();
    append_phases.plan_build_cycles =
        AotWorkerTimingDelta(timing, plan_start, emit_start);
    RecordAotPlanBuildProfile(timing, plan_profile);
    append_scale.plan_block_count = plan.block_count;
    append_scale.plan_instruction_count = plan.instruction_count;
    const bool image_built =
        plan_built && runtime::BuildAotCodeCacheImage(plan, build_options,
                                                      &image);
    const std::uint64_t validate_start = phase_now();
    append_phases.image_emit_cycles =
        AotWorkerTimingDelta(timing, emit_start, validate_start);
    append_scale.emitted_bytes =
        static_cast<std::uint32_t>(image.bytes.size());
    if (!plan_built || !image_built)
    {
        result->message = "failed to translate dynamic guest target";
        return true;
    }
    std::uint32_t unsafe_hle_address = 0U;
    const bool hle_covered = runtime::ValidateAotCodeCacheHleCoverage(
        plan, image, &unsafe_hle_address);
    placement_phase_start = phase_now();
    append_phases.validate_cycles =
        AotWorkerTimingDelta(timing, validate_start, placement_phase_start);
    if (!hle_covered)
    {
        result->message = "dynamic AOT CFG lacks complete HLE/selector-guard coverage";
        return true;
    }
    if (image.bytes.size() > placement->capacity - placement->size)
    {
        result->message = "dynamic AOT cache capacity is exhausted";
        return true;
    }
    const std::uint32_t append_offset = placement->size;
    const std::uint32_t requested_page = AotGuestPage(guest_entry);
    std::vector<bool> image_active(image.address_map.size(), true);
    std::vector<bool> image_tracks_guest_bytes(
        image.address_map.size(), true);
    std::vector<std::uint32_t> candidate_active_pages;
    std::size_t entry_index = image.address_map.size();
    for (std::size_t index = 0; index < image.address_map.size(); ++index)
    {
        runtime::AotAddressMapEntry& entry = image.address_map[index];
        image_tracks_guest_bytes[index] =
            AotAddressMapTracksGuestBytes(entry, excluded_ranges);
        bool can_activate = CanActivateAotAddressMapEntry(
            *placement, entry, requested_page);
        // Task 417: the entry this request exists to produce is allowed to span
        // a retired page, because this image was just built from that page's
        // current bytes. Everything else keeps the original rule.
        if (!can_activate && entry.guest_address == guest_entry &&
            !AotStrictSpanningEntryEnabled() &&
            !EntrySpansQuarantinedPage(*placement, entry))
        {
            can_activate = true;
            ++AotSpanningEntryActivationCount();
        }
        if (!can_activate)
        {
            image_active[index] = false;
            image.bytes[entry.cache_offset] = 0xCCU;
        }
        if (image_active[index] && entry.guest_address == guest_entry)
        {
            entry_index = index;
        }
        if (!image_active[index] || !image_tracks_guest_bytes[index] ||
            entry.guest_length == 0U)
        {
            continue;
        }
        const std::uint64_t last =
            static_cast<std::uint64_t>(entry.guest_address) +
            entry.guest_length - 1U;
        if (last > std::numeric_limits<std::uint32_t>::max())
        {
            result->message = "dynamic AOT mapping crosses address space";
            return true;
        }
        const std::uint32_t first_page =
            AotGuestPage(entry.guest_address);
        const std::uint32_t last_page = AotGuestPage(
            static_cast<std::uint32_t>(last));
        for (std::uint32_t page = first_page;; page += 0x1000U)
        {
            const auto position = std::lower_bound(
                candidate_active_pages.begin(),
                candidate_active_pages.end(), page);
            if (position == candidate_active_pages.end() ||
                *position != page)
            {
                candidate_active_pages.insert(position, page);
            }
            if (page == last_page || page > 0xFFFFEFFFU)
            {
                break;
            }
        }
    }
    if (entry_index == image.address_map.size())
    {
        result->message = "dynamic AOT entry was not active in the new image";
        return true;
    }
    if (write_watch_set != nullptr &&
        !InstallAotGuestPageWriteWatches(
            *placement, &candidate_active_pages, write_watch_set))
    {
        result->message =
            "failed to install write watches before AOT publication";
        return true;
    }
    const std::uint32_t generation =
        AllocateAotGeneration(placement);
    void* cache = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(placement->base_address));
    if (!repiu::platform::ProtectMemory(
            cache, placement->capacity,
            repiu::platform::MemoryProtection::kReadWrite, nullptr))
    {
        result->message = "failed to make AOT cache writable";
        return true;
    }
    std::memcpy(static_cast<std::uint8_t*>(cache) + append_offset,
                image.bytes.data(), image.bytes.size());
    if (!ResolveAotTimerSafePoints(
            image, static_cast<std::uint8_t*>(cache) + append_offset,
            placement))
    {
        repiu::platform::ProtectMemory(
            cache, placement->capacity,
            repiu::platform::MemoryProtection::kExecuteRead, nullptr);
        result->unsafe_failure = true;
        result->message = "AOT timer safe-point request is unavailable";
        return true;
    }
    ResolveAotJumpTables(
        image, static_cast<std::uint8_t*>(cache) + append_offset,
        placement->base_address + append_offset);
    ResolveAotSegmentOverrides(
        image, static_cast<std::uint8_t*>(cache) + append_offset,
        segment_table);
    ResolveAotGuardedSegmentPops(
        image, static_cast<std::uint8_t*>(cache) + append_offset,
        placement, segment_table);
    ResolveAotGuardedSegmentReads(
        image, static_cast<std::uint8_t*>(cache) + append_offset,
        segment_table);
    ResolveAotGuardedSegmentLoads(
        image, static_cast<std::uint8_t*>(cache) + append_offset,
        placement, segment_table);
    if (!ResolveAotDbtReturnDispatchSites(
            image, static_cast<std::uint8_t*>(cache) + append_offset,
            placement->base_address + append_offset))
    {
        repiu::platform::ProtectMemory(
            cache, placement->capacity,
            repiu::platform::MemoryProtection::kExecuteRead, nullptr);
        result->unsafe_failure = true;
        result->message = "AOT-DBT return thunk is unavailable";
        return true;
    }
    if (!ResolveAotDirectReturnProbes(
            image, static_cast<std::uint8_t*>(cache) + append_offset,
            image.bytes.size(), placement))
    {
        repiu::platform::ProtectMemory(
            cache, placement->capacity,
            repiu::platform::MemoryProtection::kExecuteRead, nullptr);
        result->unsafe_failure = true;
        result->message = "AOT direct-return table is unavailable";
        return true;
    }
    if (!ResolveAotDbtDirectEdgeDispatchSites(
            image, static_cast<std::uint8_t*>(cache) + append_offset,
            placement->base_address + append_offset))
    {
        repiu::platform::ProtectMemory(
            cache, placement->capacity,
            repiu::platform::MemoryProtection::kExecuteRead, nullptr);
        result->unsafe_failure = true;
        result->message =
            "AOT-DBT direct-edge thunk is unavailable";
        return true;
    }
    if (!ResolveAotDbtHleDispatchSites(
            image, static_cast<std::uint8_t*>(cache) + append_offset,
            placement->base_address + append_offset))
    {
        repiu::platform::ProtectMemory(
            cache, placement->capacity,
            repiu::platform::MemoryProtection::kExecuteRead, nullptr);
        result->unsafe_failure = true;
        result->message = "AOT-DBT HLE thunk is unavailable";
        return true;
    }
    if (!ResolveAotDbtIndirectDispatchSites(
            image, static_cast<std::uint8_t*>(cache) + append_offset,
            placement->base_address + append_offset))
    {
        repiu::platform::ProtectMemory(
            cache, placement->capacity,
            repiu::platform::MemoryProtection::kExecuteRead, nullptr);
        result->unsafe_failure = true;
        result->message = "AOT-DBT indirect thunk is unavailable";
        return true;
    }
    std::vector<std::uint32_t> relinked_cache_offsets;
    for (std::size_t image_index = 0;
         image_index < image.address_map.size(); ++image_index)
    {
        const runtime::AotAddressMapEntry& fresh =
            image.address_map[image_index];
        if (!image_active[image_index])
        {
            continue;
        }
        const auto inactive =
            placement->inactive_map_indices_by_guest_address.find(
                fresh.guest_address);
        if (inactive ==
            placement->inactive_map_indices_by_guest_address.end())
        {
            continue;
        }
        const std::uint32_t fresh_cache_offset =
            append_offset + fresh.cache_offset;
        for (std::uint32_t old_index : inactive->second)
        {
            if (old_index >= placement->address_map.size())
            {
                continue;
            }
            const runtime::AotAddressMapEntry& stale =
                placement->address_map[old_index];
            if (placement->address_map_states[old_index].active ||
                stale.emitted_length < 5U)
            {
                continue;
            }
            const std::int64_t relative =
                static_cast<std::int64_t>(fresh_cache_offset) -
                (static_cast<std::int64_t>(stale.cache_offset) + 5U);
            if (relative < std::numeric_limits<std::int32_t>::min() ||
                relative > std::numeric_limits<std::int32_t>::max())
            {
                continue;
            }
            auto* cache_bytes = static_cast<std::uint8_t*>(cache);
            cache_bytes[stale.cache_offset] = 0xE9U;
            const std::int32_t displacement =
                static_cast<std::int32_t>(relative);
            std::memcpy(cache_bytes + stale.cache_offset + 1U,
                        &displacement, sizeof(displacement));
            relinked_cache_offsets.push_back(stale.cache_offset);
            ++result->relinked_entry_count;
        }
    }
    const bool protected_rx = repiu::platform::ProtectMemory(
        cache, placement->capacity,
        repiu::platform::MemoryProtection::kExecuteRead, nullptr);
    repiu::platform::FlushInstructionCacheRange(
        static_cast<std::uint8_t*>(cache) + append_offset,
        image.bytes.size());
    for (std::uint32_t cache_offset : relinked_cache_offsets)
    {
        repiu::platform::FlushInstructionCacheRange(
            static_cast<std::uint8_t*>(cache) + cache_offset, 5U);
    }
    if (!protected_rx)
    {
        result->unsafe_failure = true;
        result->message = "failed to restore AOT cache execute protection";
        return true;
    }

    const std::size_t previous_map_count = placement->address_map.size();
    for (std::size_t image_index = 0;
         image_index < image.address_map.size(); ++image_index)
    {
        runtime::AotAddressMapEntry entry = image.address_map[image_index];
        entry.cache_offset += append_offset;
        placement->address_map.push_back(entry);
        const std::uint32_t map_index = static_cast<std::uint32_t>(
            previous_map_count + image_index);
        RegisterAotAddressMap(
            placement, map_index, generation, image_active[image_index],
            image_tracks_guest_bytes[image_index], requested_page,
            &result->active_guest_pages);
    }
    // Task 324 safety net: the registrations above normally link incrementally,
    // but rebuild here if any declined so the next lookup is not left on the
    // linear fallback.
    EnsureAotCacheAddressIndex(placement);
    for (runtime::AotCodeCacheFixup fixup : image.fixups)
    {
        fixup.cache_patch_offset += append_offset;
        placement->fixups.push_back(fixup);
    }
    for (runtime::AotIndirectInlineCacheSite site :
         image.indirect_inline_cache_sites)
    {
        site.cache_offset += append_offset;
        site.miss_cache_offset += append_offset;
        if (site.miss_probe_cache_offset != 0U)
        {
            site.miss_probe_cache_offset += append_offset;
        }
        site.target_immediate_offset += append_offset;
        site.guard_offset += append_offset;
        site.jump_displacement_offset += append_offset;
        for (runtime::AotInlineCacheEntry& entry : site.entries)
        {
            entry.compare_offset += append_offset;
            entry.target_immediate_offset += append_offset;
            entry.guard_offset += append_offset;
            entry.jump_displacement_offset += append_offset;
        }
        placement->indirect_inline_cache_sites.push_back(site);
    }
    for (runtime::AotDbtReturnDispatchSite site :
         image.dbt_return_dispatch_sites)
    {
        site.miss_cache_offset += append_offset;
        site.miss_address_immediate_offset += append_offset;
        site.thunk_displacement_offset += append_offset;
        site.fallback_cache_offset += append_offset;
        site.success_cache_offset += append_offset;
        placement->dbt_return_dispatch_sites.push_back(site);
    }
    SyncAotReturnPatchPolicy(placement);
    for (runtime::AotDbtHleDispatchSite site :
         image.dbt_hle_dispatch_sites)
    {
        site.dispatch_cache_offset += append_offset;
        site.dispatch_address_immediate_offset += append_offset;
        site.thunk_displacement_offset += append_offset;
        site.fallback_cache_offset += append_offset;
        site.success_cache_offset += append_offset;
        placement->dbt_hle_dispatch_sites.push_back(site);
    }
    for (runtime::AotDbtIndirectDispatchSite site :
         image.dbt_indirect_dispatch_sites)
    {
        site.miss_cache_offset += append_offset;
        site.miss_address_immediate_offset += append_offset;
        site.thunk_displacement_offset += append_offset;
        site.fallback_cache_offset += append_offset;
        site.success_cache_offset += append_offset;
        placement->dbt_indirect_dispatch_sites.push_back(site);
    }
    for (runtime::AotDbtDirectEdgeDispatchSite site :
         image.dbt_direct_edge_dispatch_sites)
    {
        site.dispatch_cache_offset += append_offset;
        site.dispatch_address_immediate_offset += append_offset;
        site.thunk_displacement_offset += append_offset;
        site.fallback_cache_offset += append_offset;
        site.success_cache_offset += append_offset;
        placement->dbt_direct_edge_dispatch_sites.push_back(site);
    }
    for (runtime::AotJumpTableSite site : image.jump_table_sites)
    {
        site.cache_offset += append_offset;
        site.displacement_patch_offset += append_offset;
        site.fallback_offset += append_offset;
        site.table_cache_offset += append_offset;
        placement->jump_table_sites.push_back(std::move(site));
    }
    for (runtime::AotSegmentOverrideSite site : image.segment_override_sites)
    {
        site.cache_offset += append_offset;
        site.displacement_offset += append_offset;
        site.guard_address_offset += append_offset;
        site.guard_selector_offset += append_offset;
        if (site.dispatch_cache_offset != 0U)
        {
            site.dispatch_cache_offset += append_offset;
        }
        placement->segment_override_sites.push_back(site);
    }
    for (runtime::AotGuardedSegmentPopSite site :
         image.guarded_segment_pop_sites)
    {
        site.cache_offset += append_offset;
        site.shadow_address_offset += append_offset;
        site.success_counter_address_offset += append_offset;
        site.fallback_counter_address_offset += append_offset;
        site.fallback_offset += append_offset;
        placement->guarded_segment_pop_sites.push_back(site);
    }
    for (runtime::AotGuardedSegmentLoadSite site :
         image.guarded_segment_load_sites)
    {
        site.cache_offset += append_offset;
        site.shadow_address_offset += append_offset;
        site.success_counter_address_offset += append_offset;
        site.fallback_counter_address_offset += append_offset;
        site.fallback_offset += append_offset;
        placement->guarded_segment_load_sites.push_back(site);
    }
    for (runtime::AotGuardedSegmentReadSite site :
         image.guarded_segment_read_sites)
    {
        site.cache_offset += append_offset;
        site.shadow_address_offset += append_offset;
        site.load_shadow_address_offset += append_offset;
        site.fallback_offset += append_offset;
        placement->guarded_segment_read_sites.push_back(site);
    }
    for (runtime::AotDirectReturnProbeSite site :
         image.direct_return_probe_sites)
    {
        site.cache_offset += append_offset;
        site.mask_immediate_offset += append_offset;
        site.key_address_offset += append_offset;
        site.target_address_offset += append_offset;
        site.hit_counter_address_offset += append_offset;
        placement->direct_return_probe_sites.push_back(site);
    }
    for (runtime::AotTimerSafePointSite site :
         image.timer_safe_point_sites)
    {
        site.cache_offset += append_offset;
        site.request_address_offset += append_offset;
        site.breakpoint_offset += append_offset;
        placement->timer_safe_point_cache_offsets.insert(
            site.breakpoint_offset);
        placement
            ->timer_safe_point_guest_source_by_breakpoint_offset[
                site.breakpoint_offset] = site.guest_source;
        placement->timer_safe_point_sites.push_back(site);
    }
    IndexAotBreakpointProvenance(image, append_offset, placement);
    placement->size += static_cast<std::uint32_t>(image.bytes.size());
    result->cache_entry = placement->base_address + append_offset +
                          image.address_map[entry_index].cache_offset;
    result->generation = generation;
    result->added_bytes = static_cast<std::uint32_t>(image.bytes.size());
    result->added_mappings = static_cast<std::uint32_t>(
        image.address_map.size());
    result->appended = true;
    result->message = "dynamic AOT translation appended";
    return true;
}

std::uint32_t ReResolveWin32AotSegmentOverrides(
    AotCodeCachePlacement* placement,
    const AotSegmentTable* segment_table,
    AotSegmentPatchStats* stats)
{
    if (placement == nullptr || !placement->placed ||
        segment_table == nullptr ||
        (placement->segment_override_sites.empty() &&
         placement->guarded_segment_pop_sites.empty() &&
         placement->guarded_segment_read_sites.empty() &&
         placement->guarded_segment_load_sites.empty()))
    {
        return 0U;
    }
    void* cache = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(placement->base_address));
    if (!repiu::platform::ProtectMemory(
            cache, placement->capacity,
            repiu::platform::MemoryProtection::kReadWrite, nullptr))
    {
        return 0U;
    }
    auto* bytes = static_cast<std::uint8_t*>(cache);
    if (stats != nullptr)
    {
        *stats = AotSegmentPatchStats{};
    }
    std::uint32_t processed = 0U;
    for (const runtime::AotSegmentOverrideSite& site :
         placement->segment_override_sites)
    {
        const std::uint8_t seg = site.segment_register;
        if (seg >= 6U)
        {
            continue;
        }
        const AotSegmentResolution& resolution =
            segment_table->segments[seg];
        ++processed;
        if (resolution.shadow_address == 0U)
        {
            bytes[site.cache_offset] = 0xCCU;
            if (stats != nullptr)
            {
                ++stats->unresolved_site_count;
            }
            continue;
        }
        if (resolution.policy == AotSegmentAccessPolicy::kHleLowMemory)
        {
            if (site.dispatch_cache_offset == 0U)
            {
                bytes[site.cache_offset] = 0xCCU;
            }
            else
            {
                bytes[site.cache_offset] = 0xE9U;
                const std::int32_t relative = static_cast<std::int32_t>(
                    site.dispatch_cache_offset - (site.cache_offset + 5U));
                std::memcpy(bytes + site.cache_offset + 1U,
                            &relative, sizeof(relative));
            }
            if (stats != nullptr)
            {
                ++stats->hle_site_count;
            }
            continue;
        }
        if (resolution.policy != AotSegmentAccessPolicy::kNativeFolded)
        {
            bytes[site.cache_offset] = 0xCCU;
            if (stats != nullptr)
            {
                ++stats->unresolved_site_count;
            }
            continue;
        }
        // Restore the full guard prefix because HLE routing overwrites its
        // first five bytes with JMP rel32.
        bytes[site.cache_offset] = 0x9CU;
        bytes[site.cache_offset + 1U] = 0x66U;
        bytes[site.cache_offset + 2U] = 0x81U;
        bytes[site.cache_offset + 3U] = 0x3DU;
        std::memcpy(bytes + site.guard_address_offset,
                    &resolution.shadow_address, sizeof(std::uint32_t));
        std::memcpy(bytes + site.guard_selector_offset,
                    &resolution.selector, sizeof(std::uint16_t));
        const std::uint32_t displacement =
            static_cast<std::uint32_t>(site.original_displacement) +
            resolution.base;
        std::memcpy(bytes + site.displacement_offset, &displacement,
                    sizeof(displacement));
        if (stats != nullptr)
        {
            ++stats->native_site_count;
        }
    }
    const std::uintptr_t load_success_address = reinterpret_cast<std::uintptr_t>(
        &placement->guarded_segment_load_success_count);
    const std::uintptr_t load_fallback_address = reinterpret_cast<std::uintptr_t>(
        &placement->guarded_segment_load_fallback_count);
    for (const runtime::AotGuardedSegmentLoadSite& site :
         placement->guarded_segment_load_sites)
    {
        ++processed;
        const std::uint8_t seg = site.segment_register;
        if (seg >= 6U ||
            segment_table->segments[seg].shadow_address == 0U ||
            load_success_address > UINT32_MAX ||
            load_fallback_address > UINT32_MAX)
        {
            bytes[site.cache_offset] = 0xCCU;
            continue;
        }
        bytes[site.cache_offset] = 0x9CU;
        const std::uint32_t shadow_address =
            segment_table->segments[seg].shadow_address;
        const std::uint32_t success =
            static_cast<std::uint32_t>(load_success_address);
        const std::uint32_t fallback =
            static_cast<std::uint32_t>(load_fallback_address);
        std::memcpy(bytes + site.shadow_address_offset,
                    &shadow_address, sizeof(shadow_address));
        std::memcpy(bytes + site.success_counter_address_offset,
                    &success, sizeof(success));
        std::memcpy(bytes + site.fallback_counter_address_offset,
                    &fallback, sizeof(fallback));
        if (stats != nullptr)
        {
            ++stats->guarded_load_site_count;
        }
    }
    const std::uintptr_t success_address = reinterpret_cast<std::uintptr_t>(
        &placement->guarded_segment_pop_success_count);
    const std::uintptr_t fallback_address = reinterpret_cast<std::uintptr_t>(
        &placement->guarded_segment_pop_fallback_count);
    for (const runtime::AotGuardedSegmentPopSite& site :
         placement->guarded_segment_pop_sites)
    {
        ++processed;
        const std::uint8_t seg = site.segment_register;
        if (seg >= 6U ||
            segment_table->segments[seg].shadow_address == 0U ||
            success_address > UINT32_MAX || fallback_address > UINT32_MAX)
        {
            bytes[site.cache_offset] = 0xCCU;
            continue;
        }
        bytes[site.cache_offset] = 0x9CU;
        const std::uint32_t shadow_address =
            segment_table->segments[seg].shadow_address;
        const std::uint32_t success =
            static_cast<std::uint32_t>(success_address);
        const std::uint32_t fallback =
            static_cast<std::uint32_t>(fallback_address);
        std::memcpy(bytes + site.shadow_address_offset,
                    &shadow_address, sizeof(shadow_address));
        std::memcpy(bytes + site.success_counter_address_offset,
                    &success, sizeof(success));
        std::memcpy(bytes + site.fallback_counter_address_offset,
                    &fallback, sizeof(fallback));
        if (stats != nullptr)
        {
            ++stats->guarded_pop_site_count;
        }
    }
    for (const runtime::AotGuardedSegmentReadSite& site :
         placement->guarded_segment_read_sites)
    {
        ++processed;
        const std::uint8_t seg = site.segment_register;
        if (seg >= 6U ||
            segment_table->segments[seg].shadow_address == 0U)
        {
            bytes[site.cache_offset] = 0xCCU;
            continue;
        }
        bytes[site.cache_offset] = 0x9CU;
        const std::uint32_t shadow_address =
            segment_table->segments[seg].shadow_address;
        std::memcpy(bytes + site.shadow_address_offset,
                    &shadow_address, sizeof(shadow_address));
        std::memcpy(bytes + site.load_shadow_address_offset,
                    &shadow_address, sizeof(shadow_address));
        if (stats != nullptr)
        {
            ++stats->guarded_read_site_count;
        }
    }
    repiu::platform::ProtectMemory(
        cache, placement->capacity,
        repiu::platform::MemoryProtection::kExecuteRead, nullptr);
    repiu::platform::FlushInstructionCacheRange(cache, placement->size);
    return processed;
}

bool PatchAotIndirectInlineCache(
    AotCodeCachePlacement* placement,
    std::uint32_t cache_miss_address,
    std::uint32_t guest_target,
    std::uint32_t cache_target,
    AotInlineCachePatchResult* result)
{
    if (placement == nullptr || result == nullptr)
    {
        return false;
    }
    *result = AotInlineCachePatchResult{};
    result->attempted = true;
    result->cache_miss_address = cache_miss_address;
    result->guest_target = guest_target;
    result->cache_target = cache_target;
    if (!placement->placed ||
        cache_miss_address < placement->base_address)
    {
        result->message = "AOT inline-cache placement is unavailable";
        return true;
    }
    const std::uint32_t miss_offset =
        cache_miss_address - placement->base_address;
    // Task 479. The scan below cost about 75,100 cycles per patch on pumpit8:
    // 8,019 sites of about 44 bytes, half of them streamed before the first
    // match. The index answers the same question in two cache-line touches and
    // stays a cache rather than a precondition -- a stale index, a site that
    // does not really carry this key, and a "not found" answer all fall through
    // to the scan, so the selected site is identical and only the speed differs.
    EnsureAotInlineCacheSiteIndex(placement);
    runtime::AotIndirectInlineCacheSite* selected = nullptr;
    const AotInlineCacheSiteLookup indexed =
        LookupAotInlineCacheSiteIndex(*placement, miss_offset);
    if (indexed.usable && indexed.found &&
        indexed.site_index < placement->indirect_inline_cache_sites.size())
    {
        runtime::AotIndirectInlineCacheSite& site =
            placement->indirect_inline_cache_sites[indexed.site_index];
        if (miss_offset == site.miss_cache_offset ||
            miss_offset == site.miss_cache_offset + 1U)
        {
            selected = &site;
            ++placement->inline_cache_site_index.lookup_count;
        }
    }
    if (selected == nullptr)
    {
        ++placement->inline_cache_site_index.fallback_scan_count;
        for (auto& site : placement->indirect_inline_cache_sites)
        {
            if (miss_offset == site.miss_cache_offset ||
                miss_offset == site.miss_cache_offset + 1U)
            {
                selected = &site;
                break;
            }
        }
    }
    bool entry_offsets_valid = selected != nullptr;
    if (selected != nullptr)
    {
        for (const runtime::AotInlineCacheEntry& entry : selected->entries)
        {
            entry_offsets_valid = entry_offsets_valid &&
                entry.jump_displacement_offset + 4U <= placement->size &&
                entry.target_immediate_offset + 4U <= placement->size &&
                entry.guard_offset + 6U <= placement->size;
        }
    }
    if (selected == nullptr || !entry_offsets_valid ||
        selected->jump_displacement_offset + 4U > placement->size ||
        selected->target_immediate_offset + 4U > placement->size ||
        selected->guard_offset + 6U > placement->size)
    {
        static long miss_lookup_failure_count = 0;
        const long failure_index =
            repiu::platform::AtomicIncrement(&miss_lookup_failure_count);
        if (failure_index <= 16 || (failure_index & 0xFFF) == 0)
        {
            fprintf(stderr,
                    "[repiu-live-debug] icache patch lookup failed #%ld"
                    " miss=0x%08X guest=0x%08X\n",
                    failure_index, cache_miss_address, guest_target);
        }
        result->message = "AOT inline-cache miss site was not found";
        return true;
    }
    void* cache = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(placement->base_address));
    auto* bytes = static_cast<std::uint8_t*>(cache);
    // Multi-entry indirect/return sites pick an entry from the current cache
    // bytes: refresh the entry already holding this target, else fill the
    // first empty one, else round-robin replace. Entry i's JNE chains to
    // entry i+1's compare; the last entry's JNE goes to the miss tail.
    std::uint32_t immediate_offset = selected->target_immediate_offset;
    std::uint32_t displacement_offset = selected->jump_displacement_offset;
    std::uint32_t guard_offset = selected->guard_offset;
    std::uint32_t guard_target_offset =
        runtime::AotInlineCacheGuardTargetOffset(*selected);
    std::size_t chosen_entry = 0;
    if (!selected->entries.empty())
    {
        const std::size_t entry_count = selected->entries.size();
        std::size_t chosen = entry_count;
        for (std::size_t index = 0; index < entry_count; ++index)
        {
            const runtime::AotInlineCacheEntry& entry =
                selected->entries[index];
            std::uint32_t immediate = 0;
            std::memcpy(&immediate,
                        bytes + entry.target_immediate_offset,
                        sizeof(immediate));
            if (bytes[entry.guard_offset] == 0x0FU &&
                immediate == guest_target)
            {
                chosen = index;
                break;
            }
        }
        if (chosen == entry_count)
        {
            for (std::size_t index = 0; index < entry_count; ++index)
            {
                if (bytes[selected->entries[index].guard_offset] == 0xE9U)
                {
                    chosen = index;
                    break;
                }
            }
        }
        if (chosen == entry_count)
        {
            chosen = selected->replace_cursor % entry_count;
            selected->replace_cursor =
                static_cast<std::uint32_t>((chosen + 1U) % entry_count);
        }
        const runtime::AotInlineCacheEntry& entry =
            selected->entries[chosen];
        immediate_offset = entry.target_immediate_offset;
        displacement_offset = entry.jump_displacement_offset;
        guard_offset = entry.guard_offset;
        guard_target_offset = chosen + 1U < entry_count
            ? selected->entries[chosen + 1U].compare_offset
            : runtime::AotInlineCacheGuardTargetOffset(*selected);
        chosen_entry = chosen;
    }
    // Temporary Task 220 diagnostics: sample the first patches and every
    // 4096th so a patch that lands on dead bytes (stale site offsets after a
    // generation republish) becomes visible without flooding stderr.
    static long patch_call_count = 0;
    const long patch_call_index = repiu::platform::AtomicIncrement(&patch_call_count);
    if (patch_call_index <= 16 || (patch_call_index & 0xFFF) == 0)
    {
        fprintf(stderr,
                "[repiu-live-debug] icache patch #%ld miss=0x%08X guest=0x%08X"
                " cache=0x%08X site_guest=0x%08X entries=%u chosen=%u"
                " guard=0x%02X\n",
                patch_call_index, cache_miss_address, guest_target,
                cache_target, selected->guest_source,
                static_cast<unsigned>(selected->entries.size()),
                static_cast<unsigned>(chosen_entry), bytes[guard_offset]);
    }
    // Task 413: the three writes below span the chosen entry's immediate, its
    // jump displacement, and its six-byte guard, so the window is the pages
    // those cover rather than all 16 MB.
    const std::uint32_t patch_first_offset =
        std::min({immediate_offset, displacement_offset, guard_offset});
    const std::uint32_t patch_last_offset = std::max(
        {immediate_offset + static_cast<std::uint32_t>(sizeof(guest_target)),
         displacement_offset + 4U,
         guard_offset + 6U});
    const AotCachePatchWindow patch_window =
        ComputeAotCachePatchWindow(*placement, patch_first_offset,
                                   patch_last_offset);
    if (!repiu::platform::ProtectMemory(
            patch_window.base, patch_window.size,
            repiu::platform::MemoryProtection::kReadWrite, nullptr))
    {
        result->windows_error = static_cast<std::uint32_t>(errno);
        result->message = "failed to make AOT inline cache writable";
        return true;
    }
    std::memcpy(bytes + immediate_offset,
                &guest_target, sizeof(guest_target));
    const std::int64_t relative =
        static_cast<std::int64_t>(cache_target) -
        (static_cast<std::int64_t>(placement->base_address) +
         displacement_offset + 4U);
    if (relative < std::numeric_limits<std::int32_t>::min() ||
        relative > std::numeric_limits<std::int32_t>::max())
    {
        repiu::platform::ProtectMemory(
            patch_window.base, patch_window.size,
            repiu::platform::MemoryProtection::kExecuteRead, nullptr);
        result->message = "AOT inline-cache target is outside rel32 range";
        return true;
    }
    const std::int32_t displacement = static_cast<std::int32_t>(relative);
    std::memcpy(bytes + displacement_offset,
                &displacement, sizeof(displacement));
    const std::int32_t miss_displacement = static_cast<std::int32_t>(
        guard_target_offset - (guard_offset + 6U));
    bytes[guard_offset] = 0x0FU;
    bytes[guard_offset + 1U] = 0x85U;
    std::memcpy(bytes + guard_offset + 2U,
                &miss_displacement, sizeof(miss_displacement));
    if (!repiu::platform::ProtectMemory(
            patch_window.base, patch_window.size,
            repiu::platform::MemoryProtection::kExecuteRead, nullptr))
    {
        result->windows_error = static_cast<std::uint32_t>(errno);
        result->message = "failed to restore AOT inline-cache RX protection";
        return true;
    }
    repiu::platform::FlushInstructionCacheRange(
        bytes + selected->cache_offset,
        selected->miss_cache_offset + 2U - selected->cache_offset);
    result->patched = true;
    result->message = "AOT indirect inline cache patched";
    return true;
}

void ReleaseAotCodeCache(AotCodeCachePlacement* placement)
{
    if (placement == nullptr)
    {
        return;
    }
    if (placement->placed && placement->base_address != 0U)
    {
        repiu::platform::ReleaseMemory(
            reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(placement->base_address)),
            placement->capacity);
    }
    *placement = AotCodeCachePlacement{};
}

bool FindAotGuestAddress(const AotCodeCachePlacement& placement,
                         std::uint32_t cache_address,
                         std::uint32_t* guest_address)
{
    if (!placement.placed || guest_address == nullptr ||
        cache_address < placement.base_address)
    {
        return false;
    }
    const std::uint32_t offset = cache_address - placement.base_address;

    // Task 334. Release measurement put this scan at 96.00% of the reentry
    // handler and roughly 44% of all guest wall clock, at 551,864 ticks per
    // call over a map of about 100,000 entries. The index answers in O(log n)
    // and returns the same entry; when it is stale or the map is not sorted by
    // cache offset the scan below runs unchanged, so this degrades to slow
    // rather than wrong, exactly as Task 324's index does.
    const AotGuestAddressLookup indexed =
        LookupAotGuestAddressIndex(placement, offset);
    if (indexed.usable)
    {
        if (!indexed.found)
        {
            return false;
        }
        *guest_address =
            placement.address_map[indexed.map_index].guest_address;
        return true;
    }

    for (const runtime::AotAddressMapEntry& entry : placement.address_map)
    {
        if (offset >= entry.cache_offset &&
            offset < entry.cache_offset + entry.emitted_length)
        {
            *guest_address = entry.guest_address;
            return true;
        }
    }
    return false;
}

bool FindAotCacheAddress(const AotCodeCachePlacement& placement,
                         std::uint32_t guest_address,
                         std::uint32_t* cache_address)
{
    if (!placement.placed || cache_address == nullptr)
    {
        return false;
    }

    // Task 324. The two rules below reduce to: newest ACTIVE entry when this
    // guest address has a retired generation, oldest entry otherwise. The index
    // reproduces exactly that; when it is stale the original scan runs
    // unchanged, so a placement built without the update hooks (several probes
    // construct one directly) degrades to slow rather than wrong.
    {
        const bool newest_active =
            !placement.retired_guest_addresses.empty() &&
            std::binary_search(
                placement.retired_guest_addresses.begin(),
                placement.retired_guest_addresses.end(), guest_address);
        std::uint32_t map_index = 0;
        if (placement.cache_address_index.indexed_entry_count ==
                static_cast<std::uint32_t>(placement.address_map.size()) &&
            !placement.address_map.empty())
        {
            if (LookupAotCacheAddressIndex(
                    placement, guest_address, newest_active, &map_index))
            {
                *cache_address = placement.base_address +
                    placement.address_map[map_index].cache_offset;
                return true;
            }
            return false;
        }
    }

    if (placement.retired_guest_addresses.empty())
    {
        for (const runtime::AotAddressMapEntry& entry :
             placement.address_map)
        {
            if (entry.guest_address == guest_address)
            {
                *cache_address = placement.base_address + entry.cache_offset;
                return true;
            }
        }
        return false;
    }
    const bool has_retired_generation = std::binary_search(
        placement.retired_guest_addresses.begin(),
        placement.retired_guest_addresses.end(), guest_address);
    if (has_retired_generation)
    {
        for (std::size_t index = placement.address_map.size();
             index-- > 0U;)
        {
            const runtime::AotAddressMapEntry& entry =
                placement.address_map[index];
            if (placement.address_map_states[index].active &&
                entry.guest_address == guest_address)
            {
                *cache_address = placement.base_address + entry.cache_offset;
                return true;
            }
        }
    }
    else
    {
        for (std::size_t index = 0;
             index < placement.address_map.size(); ++index)
        {
            const runtime::AotAddressMapEntry& entry =
                placement.address_map[index];
            if (entry.guest_address == guest_address)
            {
                *cache_address = placement.base_address + entry.cache_offset;
                return true;
            }
        }
    }
    return false;
}

bool InstallAotProbeSentinel(AotCodeCachePlacement* placement,
                                  std::uint32_t guest_address)
{
    if (placement == nullptr || !placement->placed)
    {
        return false;
    }
    std::uint32_t cache_address = 0;
    if (!FindAotCacheAddress(*placement, guest_address, &cache_address))
    {
        return false;
    }
    void* cache = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(placement->base_address));
    if (!repiu::platform::ProtectMemory(
            cache, placement->capacity,
            repiu::platform::MemoryProtection::kReadWrite, nullptr))
    {
        return false;
    }
    *reinterpret_cast<std::uint8_t*>(
        static_cast<std::uintptr_t>(cache_address)) = 0xCCU;
    const bool restored = repiu::platform::ProtectMemory(
        cache, placement->capacity,
        repiu::platform::MemoryProtection::kExecuteRead, nullptr);
    repiu::platform::FlushInstructionCacheRange(
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(cache_address)), 1);
    return restored;
}

}  // namespace repiu::engine
