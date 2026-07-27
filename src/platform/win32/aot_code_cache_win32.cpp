#include "repiu/platform/win32/aot_code_cache_win32.h"

#include "repiu/runtime/dos_low_memory.h"
#include "repiu/runtime/aot_translation_plan.h"
#include "aot/aot_dbt_hle_dispatch.h"
#include "aot/aot_dbt_indirect_dispatch.h"
#include "aot/aot_dbt_return_dispatch.h"

#include <cstdio>
#include <cstring>
#include <limits>
#include <algorithm>
#include <unordered_map>
#include <utility>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace repiu::platform::win32
{
namespace
{

void IndexAotBreakpointProvenance(
    const runtime::AotCodeCacheImage& image,
    std::uint32_t append_offset,
    Win32AotCodeCachePlacement* placement)
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
    for (const runtime::AotJumpTableSite& site : image.jump_table_sites)
    {
        index[append_offset + site.fallback_offset] =
            AotCacheBreakpointProvenance::kJumpTableFallback;
    }
}

// Jump-table slots carry image-relative offsets; once the image bytes have a
// final absolute base the displacement and every table entry become absolute
// cache addresses. Targets missing from the image fall back to the slot's
// INT3 so the dispatcher re-executes the original guest branch.
void ResolveWin32AotJumpTables(const runtime::AotCodeCacheImage& image,
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

bool ResolveWin32AotDbtReturnDispatchSites(
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

bool ResolveWin32AotDbtHleDispatchSites(
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

bool ResolveWin32AotDbtIndirectDispatchSites(
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
void ResolveWin32AotSegmentOverrides(
    const runtime::AotCodeCacheImage& image,
    std::uint8_t* image_bytes,
    const Win32AotSegmentTable* segment_table)
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
            segment_table->segments[seg].shadow_address == 0U ||
            segment_table->segments[seg].policy !=
                Win32AotSegmentAccessPolicy::kNativeFolded)
        {
            // Cannot resolve: make the sequence a boundary at its start so the
            // original instruction is single-stepped (current behavior, safe).
            image_bytes[site.cache_offset] = 0xCCU;
            continue;
        }
        const Win32AotSegmentResolution& resolution =
            segment_table->segments[seg];
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

void ResolveWin32AotGuardedSegmentPops(
    const runtime::AotCodeCacheImage& image,
    std::uint8_t* image_bytes,
    Win32AotCodeCachePlacement* placement,
    const Win32AotSegmentTable* segment_table)
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

}  // namespace

void BuildWin32AotSegmentResolution(
    const runtime::SelectorTable& selector_table,
    std::uint32_t shadow_address,
    std::uint16_t selector,
    Win32AotSegmentResolution* resolution)
{
    if (resolution == nullptr)
    {
        return;
    }
    *resolution = Win32AotSegmentResolution{};
    resolution->shadow_address = shadow_address;
    resolution->selector = selector;
    if (shadow_address == 0U)
    {
        return;
    }
    if (selector == 0U)
    {
        resolution->policy = Win32AotSegmentAccessPolicy::kHleLowMemory;
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
        resolution->policy = Win32AotSegmentAccessPolicy::kHleLowMemory;
        return;
    }
    resolution->policy = Win32AotSegmentAccessPolicy::kNativeFolded;
}

bool PlaceWin32AotCodeCache(const runtime::AotCodeCacheImage& image,
                            Win32AotCodeCachePlacement* placement)
{
    if (placement == nullptr)
    {
        return false;
    }
    *placement = Win32AotCodeCachePlacement{};
    if (!image.valid || image.executable || image.bytes.empty())
    {
        placement->message = "AOT byte image is not ready for placement";
        return false;
    }
#if !defined(_WIN32)
    placement->message = "AOT code cache placement requires Win32";
    return false;
#else
    if (image.bytes.size() > std::numeric_limits<std::uint32_t>::max())
    {
        placement->message = "AOT code cache exceeds Win32 address size";
        return false;
    }
    constexpr std::uint32_t kDynamicCacheCapacity = 16U * 1024U * 1024U;
    const std::size_t capacity = std::max<std::size_t>(
        image.bytes.size(), kDynamicCacheCapacity);
    void* memory = VirtualAlloc(nullptr, capacity,
                                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    placement->valid = true;
    if (memory == nullptr)
    {
        placement->windows_error = GetLastError();
        placement->message = "VirtualAlloc for AOT code cache failed";
        return true;
    }
    const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(memory);
    if (base > std::numeric_limits<std::uint32_t>::max())
    {
        VirtualFree(memory, 0, MEM_RELEASE);
        placement->message = "AOT code cache is outside the x86 address range";
        return true;
    }
    std::memcpy(memory, image.bytes.data(), image.bytes.size());
    ResolveWin32AotJumpTables(image, static_cast<std::uint8_t*>(memory),
                              static_cast<std::uint32_t>(base));
    // The static placement has no live segment table, so any segment-override
    // sites fall back to boundaries (single-step) instead of a faulting guard.
    ResolveWin32AotSegmentOverrides(image, static_cast<std::uint8_t*>(memory),
                                    nullptr);
    ResolveWin32AotGuardedSegmentPops(
        image, static_cast<std::uint8_t*>(memory), placement, nullptr);
    if (!ResolveWin32AotDbtReturnDispatchSites(
            image, static_cast<std::uint8_t*>(memory),
            static_cast<std::uint32_t>(base)))
    {
        VirtualFree(memory, 0, MEM_RELEASE);
        placement->message = "AOT-DBT return thunk is unavailable";
        return true;
    }
    if (!ResolveWin32AotDbtHleDispatchSites(
            image, static_cast<std::uint8_t*>(memory),
            static_cast<std::uint32_t>(base)))
    {
        VirtualFree(memory, 0, MEM_RELEASE);
        placement->message = "AOT-DBT HLE thunk is unavailable";
        return true;
    }
    if (!ResolveWin32AotDbtIndirectDispatchSites(
            image, static_cast<std::uint8_t*>(memory),
            static_cast<std::uint32_t>(base)))
    {
        VirtualFree(memory, 0, MEM_RELEASE);
        placement->message = "AOT-DBT indirect thunk is unavailable";
        return true;
    }
    DWORD old_protection = 0;
    if (VirtualProtect(memory, capacity, PAGE_EXECUTE_READ,
                       &old_protection) == 0)
    {
        placement->windows_error = GetLastError();
        VirtualFree(memory, 0, MEM_RELEASE);
        placement->message = "VirtualProtect for AOT code cache failed";
        return true;
    }
    FlushInstructionCache(GetCurrentProcess(), memory, image.bytes.size());
    placement->base_address = static_cast<std::uint32_t>(base);
    placement->size = static_cast<std::uint32_t>(image.bytes.size());
    placement->capacity = static_cast<std::uint32_t>(capacity);
    placement->entry_address = placement->base_address +
                               image.entry_cache_offset;
    placement->address_map = image.address_map;
    InitializeWin32AotPageCoherence(placement, 1U);
    placement->fixups = image.fixups;
    placement->indirect_inline_cache_sites =
        image.indirect_inline_cache_sites;
    placement->dbt_return_dispatch_sites = image.dbt_return_dispatch_sites;
    placement->dbt_hle_dispatch_sites = image.dbt_hle_dispatch_sites;
    placement->dbt_indirect_dispatch_sites =
        image.dbt_indirect_dispatch_sites;
    placement->jump_table_sites = image.jump_table_sites;
    placement->segment_override_sites = image.segment_override_sites;
    placement->guarded_segment_pop_sites = image.guarded_segment_pop_sites;
    placement->indirect_inline_cache_entry_count =
        image.indirect_inline_cache_entry_count;
    placement->dbt_return_miss_dispatch_enabled =
        image.dbt_return_miss_dispatch_enabled;
    placement->dbt_hle_dispatch_enabled =
        image.dbt_hle_dispatch_enabled;
    placement->dbt_indirect_miss_dispatch_enabled =
        image.dbt_indirect_miss_dispatch_enabled;
    placement->guarded_segment_pop_enabled =
        image.guarded_segment_pop_enabled;
    IndexAotBreakpointProvenance(image, 0U, placement);
    placement->placed = true;
    placement->message = "AOT code cache placed as Win32 execute-read memory";
    return true;
#endif
}

bool AppendWin32DynamicAotTranslation(
    std::uint32_t runtime_base,
    std::uint32_t runtime_size,
    std::uint32_t guest_entry,
    const std::vector<runtime::AotExcludedGuestRange>& excluded_ranges,
    Win32AotPageWriteWatchSet* write_watch_set,
    Win32AotCodeCachePlacement* placement,
    const Win32AotSegmentTable* segment_table,
    Win32AotDynamicAppendResult* result,
    Win32AotWorkerTimingProfile* timing)
{
    if (placement == nullptr || result == nullptr)
    {
        return false;
    }
    *result = Win32AotDynamicAppendResult{};
    result->attempted = true;
    result->guest_entry = guest_entry;

    // Task 328: phases are accumulated on every exit path, including the early
    // returns below, so a failed append still reports what it spent. Worker
    // thread only, so no atomics.
    Win32AotAppendPhaseSample append_phases;
    Win32AotAppendScaleSample append_scale;
    // Placement runs from its start to whichever exit is taken, so it is closed
    // by the same destructor that commits the sample.
    std::uint64_t placement_phase_start = 0;
    struct AppendPhaseCommit
    {
        Win32AotWorkerTimingProfile* timing;
        Win32AotAppendPhaseSample* phases;
        const Win32AotAppendScaleSample* scale;
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
#if !defined(_WIN32)
    result->message = "dynamic AOT translation requires Win32";
    return true;
#else
    if (!placement->placed || runtime_size == 0U ||
        guest_entry < runtime_base || guest_entry - runtime_base >= runtime_size)
    {
        result->message = "dynamic AOT target is outside the guest arena";
        return true;
    }
    // Task 328 phase 1: the whole guest arena is copied here, not just the range
    // around guest_entry.
    const std::uint64_t snapshot_start = phase_now();
    runtime::RelocatedRuntimeImage snapshot;
    snapshot.valid = true;
    snapshot.relocated_image_base = runtime_base;
    snapshot.relocated_entry_linear_address = guest_entry;
    runtime::RelocatedRuntimeObject object;
    object.relocated_base_address = runtime_base;
    object.virtual_size = runtime_size;
    object.memory.resize(runtime_size);
    SIZE_T bytes_read = 0;
    const bool snapshot_failed =
        ReadProcessMemory(GetCurrentProcess(),
                          reinterpret_cast<const void*>(
                              static_cast<std::uintptr_t>(runtime_base)),
                          object.memory.data(), runtime_size,
                          &bytes_read) == 0 || bytes_read != runtime_size;
    append_phases.arena_snapshot_cycles =
        AotWorkerTimingDelta(timing, snapshot_start, phase_now());
    append_scale.snapshot_bytes = runtime_size;
    if (snapshot_failed)
    {
        result->message = "failed to snapshot live guest arena";
        return true;
    }
    snapshot.objects.push_back(std::move(object));
    runtime::AotTranslationPlan plan;
    runtime::AotCodeCacheImage image;
    runtime::AotCodeCacheBuildOptions build_options;
    build_options.indirect_inline_cache_entry_count =
        placement->indirect_inline_cache_entry_count;
    build_options.enable_dbt_return_miss_dispatch =
        placement->dbt_return_miss_dispatch_enabled;
    build_options.enable_dbt_hle_dispatch =
        placement->dbt_hle_dispatch_enabled;
    build_options.enable_dbt_indirect_miss_dispatch =
        placement->dbt_indirect_miss_dispatch_enabled;
    build_options.enable_guarded_segment_pop =
        placement->guarded_segment_pop_enabled;
    // Task 328 phases 2 and 3. Split out of the shared short-circuit into
    // sequential locals; the image build still runs only when the plan build
    // succeeded, exactly as before.
    const std::uint64_t plan_start = phase_now();
    const bool plan_built = runtime::BuildAotTranslationPlanFromEntry(
        snapshot, guest_entry, excluded_ranges, &plan);
    const std::uint64_t emit_start = phase_now();
    append_phases.plan_build_cycles =
        AotWorkerTimingDelta(timing, plan_start, emit_start);
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
    const std::uint32_t requested_page = Win32AotGuestPage(guest_entry);
    std::vector<bool> image_active(image.address_map.size(), true);
    std::vector<bool> image_tracks_guest_bytes(
        image.address_map.size(), true);
    std::vector<std::uint32_t> candidate_active_pages;
    std::size_t entry_index = image.address_map.size();
    for (std::size_t index = 0; index < image.address_map.size(); ++index)
    {
        runtime::AotAddressMapEntry& entry = image.address_map[index];
        image_tracks_guest_bytes[index] =
            Win32AotAddressMapTracksGuestBytes(entry, excluded_ranges);
        if (!CanActivateWin32AotAddressMapEntry(
                *placement, entry, requested_page))
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
            Win32AotGuestPage(entry.guest_address);
        const std::uint32_t last_page = Win32AotGuestPage(
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
        !InstallWin32AotGuestPageWriteWatches(
            *placement, &candidate_active_pages, write_watch_set))
    {
        result->message =
            "failed to install write watches before AOT publication";
        return true;
    }
    const std::uint32_t generation =
        AllocateWin32AotGeneration(placement);
    DWORD old_protection = 0;
    void* cache = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(placement->base_address));
    if (VirtualProtect(cache, placement->capacity, PAGE_READWRITE,
                       &old_protection) == 0)
    {
        result->message = "failed to make AOT cache writable";
        return true;
    }
    std::memcpy(static_cast<std::uint8_t*>(cache) + append_offset,
                image.bytes.data(), image.bytes.size());
    ResolveWin32AotJumpTables(
        image, static_cast<std::uint8_t*>(cache) + append_offset,
        placement->base_address + append_offset);
    ResolveWin32AotSegmentOverrides(
        image, static_cast<std::uint8_t*>(cache) + append_offset,
        segment_table);
    ResolveWin32AotGuardedSegmentPops(
        image, static_cast<std::uint8_t*>(cache) + append_offset,
        placement, segment_table);
    if (!ResolveWin32AotDbtReturnDispatchSites(
            image, static_cast<std::uint8_t*>(cache) + append_offset,
            placement->base_address + append_offset))
    {
        DWORD ignored = 0;
        VirtualProtect(cache, placement->capacity, PAGE_EXECUTE_READ,
                       &ignored);
        result->unsafe_failure = true;
        result->message = "AOT-DBT return thunk is unavailable";
        return true;
    }
    if (!ResolveWin32AotDbtHleDispatchSites(
            image, static_cast<std::uint8_t*>(cache) + append_offset,
            placement->base_address + append_offset))
    {
        DWORD ignored = 0;
        VirtualProtect(cache, placement->capacity, PAGE_EXECUTE_READ,
                       &ignored);
        result->unsafe_failure = true;
        result->message = "AOT-DBT HLE thunk is unavailable";
        return true;
    }
    if (!ResolveWin32AotDbtIndirectDispatchSites(
            image, static_cast<std::uint8_t*>(cache) + append_offset,
            placement->base_address + append_offset))
    {
        DWORD ignored = 0;
        VirtualProtect(cache, placement->capacity, PAGE_EXECUTE_READ,
                       &ignored);
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
    DWORD ignored = 0;
    const bool protected_rx = VirtualProtect(
        cache, placement->capacity, PAGE_EXECUTE_READ, &ignored) != 0;
    FlushInstructionCache(
        GetCurrentProcess(),
        static_cast<std::uint8_t*>(cache) + append_offset,
        image.bytes.size());
    for (std::uint32_t cache_offset : relinked_cache_offsets)
    {
        FlushInstructionCache(
            GetCurrentProcess(),
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
        RegisterWin32AotAddressMap(
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
#endif
}

std::uint32_t ReResolveWin32AotSegmentOverrides(
    Win32AotCodeCachePlacement* placement,
    const Win32AotSegmentTable* segment_table,
    Win32AotSegmentPatchStats* stats)
{
#if !defined(_WIN32)
    (void)placement;
    (void)segment_table;
    (void)stats;
    return 0U;
#else
    if (placement == nullptr || !placement->placed ||
        segment_table == nullptr ||
        (placement->segment_override_sites.empty() &&
         placement->guarded_segment_pop_sites.empty()))
    {
        return 0U;
    }
    void* cache = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(placement->base_address));
    DWORD old_protection = 0;
    if (VirtualProtect(cache, placement->capacity, PAGE_READWRITE,
                       &old_protection) == 0)
    {
        return 0U;
    }
    auto* bytes = static_cast<std::uint8_t*>(cache);
    if (stats != nullptr)
    {
        *stats = Win32AotSegmentPatchStats{};
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
        const Win32AotSegmentResolution& resolution =
            segment_table->segments[seg];
        ++processed;
        if (resolution.shadow_address == 0U ||
            resolution.policy !=
                Win32AotSegmentAccessPolicy::kNativeFolded)
        {
            bytes[site.cache_offset] = 0xCCU;
            if (stats != nullptr)
            {
                if (resolution.policy ==
                    Win32AotSegmentAccessPolicy::kHleLowMemory)
                {
                    ++stats->hle_site_count;
                }
                else
                {
                    ++stats->unresolved_site_count;
                }
            }
            continue;
        }
        // Restore the pushfd the static placement may have replaced with a
        // boundary int3, then re-apply the guard selector/address and the base.
        bytes[site.cache_offset] = 0x9CU;
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
    DWORD ignored = 0;
    VirtualProtect(cache, placement->capacity, PAGE_EXECUTE_READ, &ignored);
    FlushInstructionCache(GetCurrentProcess(), cache, placement->size);
    return processed;
#endif
}

bool PatchWin32AotIndirectInlineCache(
    Win32AotCodeCachePlacement* placement,
    std::uint32_t cache_miss_address,
    std::uint32_t guest_target,
    std::uint32_t cache_target,
    Win32AotInlineCachePatchResult* result)
{
    if (placement == nullptr || result == nullptr)
    {
        return false;
    }
    *result = Win32AotInlineCachePatchResult{};
    result->attempted = true;
    result->cache_miss_address = cache_miss_address;
    result->guest_target = guest_target;
    result->cache_target = cache_target;
#if !defined(_WIN32)
    result->message = "AOT inline-cache patching requires Win32";
    return true;
#else
    if (!placement->placed ||
        cache_miss_address < placement->base_address)
    {
        result->message = "AOT inline-cache placement is unavailable";
        return true;
    }
    const std::uint32_t miss_offset =
        cache_miss_address - placement->base_address;
    runtime::AotIndirectInlineCacheSite* selected = nullptr;
    for (auto& site : placement->indirect_inline_cache_sites)
    {
        if (miss_offset == site.miss_cache_offset ||
            miss_offset == site.miss_cache_offset + 1U)
        {
            selected = &site;
            break;
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
            InterlockedIncrement(&miss_lookup_failure_count);
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
    std::uint32_t guard_target_offset = selected->miss_cache_offset;
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
            : selected->miss_cache_offset;
        chosen_entry = chosen;
    }
    // Temporary Task 220 diagnostics: sample the first patches and every
    // 4096th so a patch that lands on dead bytes (stale site offsets after a
    // generation republish) becomes visible without flooding stderr.
    static long patch_call_count = 0;
    const long patch_call_index = InterlockedIncrement(&patch_call_count);
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
    DWORD old_protection = 0;
    if (VirtualProtect(cache, placement->capacity, PAGE_READWRITE,
                       &old_protection) == 0)
    {
        result->windows_error = GetLastError();
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
        DWORD ignored = 0;
        VirtualProtect(cache, placement->capacity, PAGE_EXECUTE_READ,
                       &ignored);
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
    DWORD ignored = 0;
    if (VirtualProtect(cache, placement->capacity, PAGE_EXECUTE_READ,
                       &ignored) == 0)
    {
        result->windows_error = GetLastError();
        result->message = "failed to restore AOT inline-cache RX protection";
        return true;
    }
    FlushInstructionCache(GetCurrentProcess(),
                          bytes + selected->cache_offset,
                          selected->miss_cache_offset + 2U -
                              selected->cache_offset);
    result->patched = true;
    result->message = "AOT indirect inline cache patched";
    return true;
#endif
}

void ReleaseWin32AotCodeCache(Win32AotCodeCachePlacement* placement)
{
    if (placement == nullptr)
    {
        return;
    }
#if defined(_WIN32)
    if (placement->placed && placement->base_address != 0U)
    {
        VirtualFree(reinterpret_cast<void*>(
                        static_cast<std::uintptr_t>(placement->base_address)),
                    0, MEM_RELEASE);
    }
#endif
    *placement = Win32AotCodeCachePlacement{};
}

bool FindAotGuestAddress(const Win32AotCodeCachePlacement& placement,
                         std::uint32_t cache_address,
                         std::uint32_t* guest_address)
{
    if (!placement.placed || guest_address == nullptr ||
        cache_address < placement.base_address)
    {
        return false;
    }
    const std::uint32_t offset = cache_address - placement.base_address;
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

bool FindAotCacheAddress(const Win32AotCodeCachePlacement& placement,
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

bool InstallWin32AotProbeSentinel(Win32AotCodeCachePlacement* placement,
                                  std::uint32_t guest_address)
{
    if (placement == nullptr || !placement->placed)
    {
        return false;
    }
#if !defined(_WIN32)
    return false;
#else
    std::uint32_t cache_address = 0;
    if (!FindAotCacheAddress(*placement, guest_address, &cache_address))
    {
        return false;
    }
    DWORD old_protection = 0;
    void* cache = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(placement->base_address));
    if (VirtualProtect(cache, placement->capacity, PAGE_READWRITE,
                       &old_protection) == 0)
    {
        return false;
    }
    *reinterpret_cast<std::uint8_t*>(
        static_cast<std::uintptr_t>(cache_address)) = 0xCCU;
    DWORD ignored = 0;
    const bool restored = VirtualProtect(cache, placement->capacity,
                                         PAGE_EXECUTE_READ, &ignored) != 0;
    FlushInstructionCache(GetCurrentProcess(),
                          reinterpret_cast<void*>(
                              static_cast<std::uintptr_t>(cache_address)), 1);
    return restored;
#endif
}

}  // namespace repiu::platform::win32
