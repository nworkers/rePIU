#include "selector_guard_probe.h"

#include "repiu/platform/win32/aot_code_cache_win32.h"
#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_translation_plan.h"
#include "repiu/runtime/selector_table.h"
#include "aot/aot_dbt_dispatch.h"
#include "aot/aot_dbt_glide_gate_dispatch.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace repiu::tools
{
namespace
{

bool IsPolicy(
    const platform::win32::Win32AotSegmentResolution& resolution,
    platform::win32::Win32AotSegmentAccessPolicy expected)
{
    return resolution.policy == expected;
}

}  // namespace

bool RunSelectorGuardProbe()
{
#if !defined(_WIN32)
    return true;
#else
    runtime::SelectorTable selector_table;
    runtime::InitializeSelectorTable(&selector_table);
    const bool descriptors_ready =
        runtime::RegisterDescriptor(
            &selector_table, {0x0080U, 0U, 0xFFFFFFFFU, 0x0092U, true}) &&
        runtime::RegisterDescriptor(
            &selector_table,
            {0x0088U, 0x02000000U, 0x0000FFFFU, 0x0092U, true}) &&
        runtime::RegisterDescriptor(
            &selector_table,
            {0x0090U, 0x00002000U, 0x00000FFFU, 0x0092U, true});

    platform::win32::Win32AotSegmentResolution flat;
    platform::win32::Win32AotSegmentResolution nonflat;
    platform::win32::Win32AotSegmentResolution selector_zero;
    platform::win32::Win32AotSegmentResolution low_memory;
    platform::win32::Win32AotSegmentResolution unresolved;
    constexpr std::uint32_t kShadowAddress = 0x00123456U;
    platform::win32::BuildWin32AotSegmentResolution(
        selector_table, kShadowAddress, 0x0080U, &flat);
    platform::win32::BuildWin32AotSegmentResolution(
        selector_table, kShadowAddress, 0x0088U, &nonflat);
    platform::win32::BuildWin32AotSegmentResolution(
        selector_table, kShadowAddress, 0U, &selector_zero);
    platform::win32::BuildWin32AotSegmentResolution(
        selector_table, kShadowAddress, 0x0090U, &low_memory);
    platform::win32::BuildWin32AotSegmentResolution(
        selector_table, kShadowAddress, 0x0098U, &unresolved);

    const bool descriptor_policy =
        descriptors_ready &&
        IsPolicy(flat,
                 platform::win32::Win32AotSegmentAccessPolicy::kNativeFolded) &&
        IsPolicy(nonflat,
                 platform::win32::Win32AotSegmentAccessPolicy::kNativeFolded) &&
        IsPolicy(selector_zero,
                 platform::win32::Win32AotSegmentAccessPolicy::kHleLowMemory) &&
        IsPolicy(low_memory,
                 platform::win32::Win32AotSegmentAccessPolicy::kHleLowMemory) &&
        IsPolicy(unresolved,
                 platform::win32::Win32AotSegmentAccessPolicy::kUnresolved);

    runtime::AotInstructionRecord record;
    record.guest_address = 0x00101000U;
    record.fallthrough_target = 0x00101003U;
    record.kind = runtime::AotInstructionKind::kSegmentOverrideMem;
    record.length = 3U;
    record.segment_override_register = 5U;
    record.bytes = {0x65U, 0x8BU, 0x00U};
    runtime::AotBasicBlock block;
    block.guest_address = record.guest_address;
    block.instructions.push_back(record);
    runtime::AotInstructionRecord return_record;
    return_record.guest_address = record.fallthrough_target;
    return_record.kind = runtime::AotInstructionKind::kReturn;
    return_record.length = 1U;
    return_record.bytes = {0xC3U};
    runtime::AotBasicBlock return_block;
    return_block.guest_address = return_record.guest_address;
    return_block.instructions.push_back(return_record);
    runtime::AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = record.guest_address;
    plan.block_count = 2U;
    plan.instruction_count = 2U;
    plan.source_code_bytes = record.length + return_record.length;
    plan.hle_boundary_count = 1U;
    plan.return_count = 1U;
    plan.blocks.push_back(block);
    plan.blocks.push_back(return_block);
    runtime::AotCodeCacheImage image;
    const bool image_ready =
        runtime::BuildAotCodeCacheImage(plan, &image) &&
        image.segment_override_sites.size() == 1U;
    bool mismatch_fails_closed = false;
    bool whole_cfg_coverage = false;
    bool missing_guard_rejected = false;
    if (image_ready)
    {
        const runtime::AotSegmentOverrideSite& site =
            image.segment_override_sites[0];
        const std::uint32_t fallback =
            site.guard_selector_offset + 5U;
        mismatch_fails_closed =
            site.cache_offset < image.bytes.size() &&
            fallback < image.bytes.size() &&
            image.bytes[site.cache_offset] == 0x9CU &&
            image.bytes[site.guard_selector_offset + 2U] == 0x74U &&
            image.bytes[site.guard_selector_offset + 3U] == 0x02U &&
            image.bytes[site.guard_selector_offset + 4U] == 0x9DU &&
            image.bytes[fallback] == 0xCCU;
        whole_cfg_coverage = runtime::ValidateAotCodeCacheHleCoverage(
            plan, image);
        runtime::AotCodeCacheImage missing_guard = image;
        missing_guard.bytes[fallback] = 0x90U;
        std::uint32_t failure_guest = 0U;
        missing_guard_rejected =
            !runtime::ValidateAotCodeCacheHleCoverage(
                plan, missing_guard, &failure_guest) &&
            failure_guest == record.guest_address;
    }

    runtime::RelocatedRuntimeImage pop_runtime;
    pop_runtime.valid = true;
    pop_runtime.relocated_image_base = 0x00102000U;
    pop_runtime.relocated_entry_linear_address = 0x00102000U;
    runtime::RelocatedRuntimeObject pop_object;
    pop_object.relocated_base_address = 0x00102000U;
    pop_object.memory = {0x07U, 0xC3U};
    pop_object.memory.resize(16U, 0x90U);
    pop_object.virtual_size =
        static_cast<std::uint32_t>(pop_object.memory.size());
    pop_runtime.objects.push_back(std::move(pop_object));
    runtime::AotTranslationPlan pop_plan;
    runtime::AotCodeCacheImage pop_image;
    runtime::AotCodeCacheBuildOptions pop_options;
    pop_options.enable_guarded_segment_pop = true;
    const bool guarded_pop_ready =
        runtime::BuildAotTranslationPlanFromEntry(
            pop_runtime, pop_runtime.relocated_entry_linear_address,
            &pop_plan) &&
        !pop_plan.blocks.empty() &&
        !pop_plan.blocks[0].instructions.empty() &&
        pop_plan.blocks[0].instructions[0].kind ==
            runtime::AotInstructionKind::kGuardedSegmentPop &&
        pop_plan.blocks[0].instructions[0].segment_register == 0U &&
        runtime::BuildAotCodeCacheImage(
            pop_plan, pop_options, &pop_image) &&
        pop_image.guarded_segment_pop_sites.size() == 1U;
    bool guarded_pop_layout = false;
    bool guarded_pop_coverage = false;
    bool guarded_pop_missing_fallback_rejected = false;
    bool guarded_pop_disabled_falls_back = false;
    if (guarded_pop_ready)
    {
        const runtime::AotGuardedSegmentPopSite& site =
            pop_image.guarded_segment_pop_sites[0];
        const std::uint32_t first_branch_next = site.cache_offset + 11U;
        const std::uint32_t second_branch_next = site.cache_offset + 20U;
        const std::uint32_t fallback_counter =
            site.fallback_counter_address_offset - 2U;
        guarded_pop_layout =
            site.cache_offset + 46U <= pop_image.bytes.size() &&
            pop_image.bytes[site.cache_offset] == 0x9CU &&
            pop_image.bytes[site.cache_offset + 1U] == 0x50U &&
            pop_image.bytes[site.cache_offset + 2U] == 0x8CU &&
            pop_image.bytes[site.cache_offset + 3U] == 0xC0U &&
            first_branch_next + pop_image.bytes[site.cache_offset + 10U] ==
                fallback_counter &&
            second_branch_next + pop_image.bytes[site.cache_offset + 19U] ==
                fallback_counter &&
            pop_image.bytes[site.fallback_offset] == 0xCCU;
        runtime::AotCodeCacheImage broken_guard = pop_image;
        broken_guard.bytes[site.cache_offset + 9U] = 0x90U;
        std::uint32_t broken_guard_guest = 0U;
        guarded_pop_coverage =
            runtime::ValidateAotCodeCacheHleCoverage(pop_plan, pop_image) &&
            !runtime::ValidateAotCodeCacheHleCoverage(
                pop_plan, broken_guard, &broken_guard_guest) &&
            broken_guard_guest == pop_runtime.relocated_entry_linear_address;
        runtime::AotCodeCacheImage missing_fallback = pop_image;
        missing_fallback.bytes[site.fallback_offset] = 0x90U;
        std::uint32_t failure_guest = 0U;
        guarded_pop_missing_fallback_rejected =
            !runtime::ValidateAotCodeCacheHleCoverage(
                pop_plan, missing_fallback, &failure_guest) &&
            failure_guest == pop_runtime.relocated_entry_linear_address;
        runtime::AotCodeCacheImage disabled_pop_image;
        guarded_pop_disabled_falls_back =
            runtime::BuildAotCodeCacheImage(
                pop_plan, &disabled_pop_image) &&
            disabled_pop_image.guarded_segment_pop_sites.empty() &&
            !disabled_pop_image.address_map.empty() &&
            disabled_pop_image.bytes[
                disabled_pop_image.address_map[0].cache_offset] == 0xCCU;
    }

    runtime::RelocatedRuntimeImage read_runtime;
    read_runtime.valid = true;
    read_runtime.relocated_image_base = 0x00102500U;
    read_runtime.relocated_entry_linear_address = 0x00102500U;
    runtime::RelocatedRuntimeObject read_object;
    read_object.relocated_base_address = 0x00102500U;
    read_object.memory = {0x8CU, 0xD8U, 0xC3U};
    read_object.memory.resize(32U, 0x90U);
    read_object.virtual_size =
        static_cast<std::uint32_t>(read_object.memory.size());
    read_runtime.objects.push_back(std::move(read_object));
    runtime::AotTranslationPlan read_plan;
    runtime::AotCodeCacheImage read_image;
    runtime::AotCodeCacheBuildOptions read_options;
    read_options.enable_guarded_segment_read = true;
    const bool guarded_read_ready =
        runtime::BuildAotTranslationPlanFromEntry(
            read_runtime, read_runtime.relocated_entry_linear_address,
            &read_plan) &&
        !read_plan.blocks.empty() &&
        !read_plan.blocks[0].instructions.empty() &&
        read_plan.blocks[0].instructions[0].kind ==
            runtime::AotInstructionKind::kGuardedSegmentRead &&
        read_plan.blocks[0].instructions[0].segment_register == 3U &&
        read_plan.blocks[0].instructions[0].gpr_register == 0U &&
        runtime::BuildAotCodeCacheImage(
            read_plan, read_options, &read_image) &&
        read_image.guarded_segment_read_sites.size() == 1U &&
        runtime::ValidateAotCodeCacheHleCoverage(read_plan, read_image);
    bool guarded_read_layout = false;
    bool guarded_read_patch = false;
    bool guarded_read_disabled_falls_back = false;
    if (guarded_read_ready)
    {
        const runtime::AotGuardedSegmentReadSite& site =
            read_image.guarded_segment_read_sites[0];
        guarded_read_layout =
            site.cache_offset + 31U <= read_image.bytes.size() &&
            site.shadow_address_offset == site.cache_offset + 8U &&
            site.load_shadow_address_offset == site.cache_offset + 19U &&
            site.fallback_offset == site.cache_offset + 28U &&
            read_image.bytes[site.cache_offset] == 0x9CU &&
            read_image.bytes[site.cache_offset + 1U] == 0x50U &&
            read_image.bytes[site.cache_offset + 2U] == 0x66U &&
            read_image.bytes[site.cache_offset + 3U] == 0x8CU &&
            read_image.bytes[site.cache_offset + 4U] == 0xD8U &&
            read_image.bytes[site.cache_offset + 5U] == 0x66U &&
            read_image.bytes[site.cache_offset + 6U] == 0x3BU &&
            read_image.bytes[site.cache_offset + 7U] == 0x05U &&
            read_image.bytes[site.cache_offset + 12U] == 0x75U &&
            read_image.bytes[site.cache_offset + 13U] == 0x0EU &&
            read_image.bytes[site.cache_offset + 16U] == 0x66U &&
            read_image.bytes[site.cache_offset + 17U] == 0x8BU &&
            read_image.bytes[site.cache_offset + 18U] == 0x05U &&
            read_image.bytes[site.cache_offset + 23U] == 0xE9U &&
            read_image.bytes[site.fallback_offset] == 0x58U &&
            read_image.bytes[site.fallback_offset + 1U] == 0x9DU &&
            read_image.bytes[site.fallback_offset + 2U] == 0xCCU;
        runtime::AotCodeCacheImage disabled_read_image;
        guarded_read_disabled_falls_back =
            runtime::BuildAotCodeCacheImage(
                read_plan, &disabled_read_image) &&
            disabled_read_image.guarded_segment_read_sites.empty() &&
            !disabled_read_image.address_map.empty() &&
            disabled_read_image.bytes[
                disabled_read_image.address_map[0].cache_offset] == 0xCCU;
        platform::win32::Win32AotCodeCachePlacement read_placement;
        if (platform::win32::PlaceWin32AotCodeCache(
                read_image, &read_placement) && read_placement.placed)
        {
            std::uint16_t shadow_selector = 0x0088U;
            const std::uintptr_t shadow_pointer =
                reinterpret_cast<std::uintptr_t>(&shadow_selector);
            platform::win32::Win32AotSegmentTable segment_table;
            segment_table.segments[3].shadow_address =
                static_cast<std::uint32_t>(shadow_pointer);
            platform::win32::Win32AotSegmentPatchStats stats;
            const std::uint32_t processed =
                platform::win32::ReResolveWin32AotSegmentOverrides(
                    &read_placement, &segment_table, &stats);
            const auto* cache = reinterpret_cast<const std::uint8_t*>(
                static_cast<std::uintptr_t>(read_placement.base_address));
            std::uint32_t patched_shadow = 0U;
            std::uint32_t patched_load_shadow = 0U;
            std::memcpy(&patched_shadow,
                        cache + site.shadow_address_offset,
                        sizeof(patched_shadow));
            std::memcpy(&patched_load_shadow,
                        cache + site.load_shadow_address_offset,
                        sizeof(patched_load_shadow));
            guarded_read_patch = shadow_pointer <= UINT32_MAX &&
                processed == 1U && stats.guarded_read_site_count == 1U &&
                cache[site.cache_offset] == 0x9CU &&
                patched_shadow == static_cast<std::uint32_t>(shadow_pointer) &&
                patched_load_shadow ==
                    static_cast<std::uint32_t>(shadow_pointer);
        }
        platform::win32::ReleaseWin32AotCodeCache(&read_placement);
    }
    runtime::RelocatedRuntimeImage load_runtime;
    load_runtime.valid = true;
    load_runtime.relocated_image_base = 0x00102800U;
    load_runtime.relocated_entry_linear_address = 0x00102800U;
    runtime::RelocatedRuntimeObject load_object;
    load_object.relocated_base_address = 0x00102800U;
    load_object.memory = {0x8EU, 0xC1U, 0xC3U};
    load_object.memory.resize(32U, 0x90U);
    load_object.virtual_size =
        static_cast<std::uint32_t>(load_object.memory.size());
    load_runtime.objects.push_back(std::move(load_object));
    runtime::AotTranslationPlan load_plan;
    runtime::AotCodeCacheImage load_image;
    runtime::AotCodeCacheBuildOptions load_options;
    load_options.enable_guarded_segment_load = true;
    const bool guarded_load_ready =
        runtime::BuildAotTranslationPlanFromEntry(
            load_runtime, load_runtime.relocated_entry_linear_address,
            &load_plan) &&
        !load_plan.blocks.empty() &&
        !load_plan.blocks[0].instructions.empty() &&
        load_plan.blocks[0].instructions[0].kind ==
            runtime::AotInstructionKind::kGuardedSegmentLoad &&
        load_plan.blocks[0].instructions[0].segment_register == 0U &&
        load_plan.blocks[0].instructions[0].gpr_register == 1U &&
        runtime::BuildAotCodeCacheImage(
            load_plan, load_options, &load_image) &&
        load_image.guarded_segment_load_sites.size() == 1U &&
        runtime::ValidateAotCodeCacheHleCoverage(load_plan, load_image);
    bool guarded_load_layout = false;
    bool guarded_load_coverage = false;
    bool guarded_load_missing_fallback_rejected = false;
    bool guarded_load_patch = false;
    bool guarded_load_disabled_falls_back = false;
    if (guarded_load_ready)
    {
        const runtime::AotGuardedSegmentLoadSite& site =
            load_image.guarded_segment_load_sites[0];
        guarded_load_layout =
            site.cache_offset + 42U <= load_image.bytes.size() &&
            site.shadow_address_offset == site.cache_offset + 14U &&
            site.success_counter_address_offset == site.cache_offset + 22U &&
            site.fallback_counter_address_offset == site.cache_offset + 35U &&
            site.fallback_offset == site.cache_offset + 41U &&
            load_image.bytes[site.cache_offset] == 0x9CU &&
            load_image.bytes[site.cache_offset + 1U] == 0x50U &&
            load_image.bytes[site.cache_offset + 2U] == 0x66U &&
            load_image.bytes[site.cache_offset + 3U] == 0x8CU &&
            load_image.bytes[site.cache_offset + 4U] == 0xC0U &&
            load_image.bytes[site.cache_offset + 5U] == 0x66U &&
            load_image.bytes[site.cache_offset + 6U] == 0x3BU &&
            load_image.bytes[site.cache_offset + 7U] == 0xC1U &&
            load_image.bytes[site.cache_offset + 8U] == 0x90U &&
            load_image.bytes[site.cache_offset + 9U] == 0x75U &&
            load_image.bytes[site.cache_offset + 10U] == 0x16U &&
            load_image.bytes[site.cache_offset + 18U] == 0x75U &&
            load_image.bytes[site.cache_offset + 19U] == 0x0DU &&
            load_image.bytes[site.cache_offset + 28U] == 0xE9U &&
            load_image.bytes[site.fallback_offset] == 0xCCU;
        runtime::AotCodeCacheImage broken_load_guard = load_image;
        broken_load_guard.bytes[site.cache_offset + 9U] = 0x90U;
        std::uint32_t broken_load_guest = 0U;
        guarded_load_coverage =
            runtime::ValidateAotCodeCacheHleCoverage(load_plan, load_image) &&
            !runtime::ValidateAotCodeCacheHleCoverage(
                load_plan, broken_load_guard, &broken_load_guest) &&
            broken_load_guest == load_runtime.relocated_entry_linear_address;
        runtime::AotCodeCacheImage missing_load_fallback = load_image;
        missing_load_fallback.bytes[site.fallback_offset] = 0x90U;
        std::uint32_t missing_load_guest = 0U;
        guarded_load_missing_fallback_rejected =
            !runtime::ValidateAotCodeCacheHleCoverage(
                load_plan, missing_load_fallback, &missing_load_guest) &&
            missing_load_guest == load_runtime.relocated_entry_linear_address;
        runtime::AotCodeCacheImage disabled_load_image;
        guarded_load_disabled_falls_back =
            runtime::BuildAotCodeCacheImage(
                load_plan, &disabled_load_image) &&
            disabled_load_image.guarded_segment_load_sites.empty() &&
            !disabled_load_image.address_map.empty() &&
            disabled_load_image.bytes[
                disabled_load_image.address_map[0].cache_offset] == 0xCCU;
        platform::win32::Win32AotCodeCachePlacement load_placement;
        if (platform::win32::PlaceWin32AotCodeCache(
                load_image, &load_placement) && load_placement.placed)
        {
            std::uint16_t shadow_selector = 0x002BU;
            const std::uintptr_t shadow_pointer =
                reinterpret_cast<std::uintptr_t>(&shadow_selector);
            platform::win32::Win32AotSegmentTable segment_table;
            segment_table.segments[0].shadow_address =
                static_cast<std::uint32_t>(shadow_pointer);
            platform::win32::Win32AotSegmentPatchStats stats;
            const std::uint32_t processed =
                platform::win32::ReResolveWin32AotSegmentOverrides(
                    &load_placement, &segment_table, &stats);
            const auto* cache = reinterpret_cast<const std::uint8_t*>(
                static_cast<std::uintptr_t>(load_placement.base_address));
            std::uint32_t patched_shadow = 0U;
            std::memcpy(&patched_shadow,
                        cache + site.shadow_address_offset,
                        sizeof(patched_shadow));
            guarded_load_patch = shadow_pointer <= UINT32_MAX &&
                processed == 1U && stats.guarded_load_site_count == 1U &&
                cache[site.cache_offset] == 0x9CU &&
                patched_shadow == static_cast<std::uint32_t>(shadow_pointer);
        }
        platform::win32::ReleaseWin32AotCodeCache(&load_placement);
    }
    const auto remains_hle = [](std::vector<std::uint8_t> bytes) {
        bytes.resize(bytes.size() + 15U, 0x90U);
        runtime::RelocatedRuntimeImage runtime_image;
        runtime_image.valid = true;
        runtime_image.relocated_image_base = 0x00103000U;
        runtime_image.relocated_entry_linear_address = 0x00103000U;
        runtime::RelocatedRuntimeObject object;
        object.relocated_base_address = 0x00103000U;
        object.virtual_size = static_cast<std::uint32_t>(bytes.size());
        object.memory = std::move(bytes);
        runtime_image.objects.push_back(std::move(object));
        runtime::AotTranslationPlan candidate;
        return runtime::BuildAotTranslationPlanFromEntry(
                   runtime_image, runtime_image.relocated_entry_linear_address,
                   &candidate) &&
            !candidate.blocks.empty() &&
            !candidate.blocks[0].instructions.empty() &&
            candidate.blocks[0].instructions[0].kind ==
                runtime::AotInstructionKind::kHleBoundary;
    };
    const bool guarded_load_rejected_forms =
        remains_hle({0x8EU, 0xD1U, 0xC3U}) &&
        remains_hle({0x8EU, 0xC4U, 0xC3U}) &&
        remains_hle({0x8EU, 0x00U, 0xC3U});
    const bool guarded_pop_rejected_forms =
        remains_hle({0x17U, 0xC3U}) &&
        remains_hle({0x66U, 0x07U, 0xC3U});
    const auto becomes_guarded_pop = [](
        std::vector<std::uint8_t> bytes, std::uint8_t expected_segment) {
        bytes.resize(bytes.size() + 15U, 0x90U);
        runtime::RelocatedRuntimeImage runtime_image;
        runtime_image.valid = true;
        runtime_image.relocated_image_base = 0x00104000U;
        runtime_image.relocated_entry_linear_address = 0x00104000U;
        runtime::RelocatedRuntimeObject object;
        object.relocated_base_address = 0x00104000U;
        object.virtual_size = static_cast<std::uint32_t>(bytes.size());
        object.memory = std::move(bytes);
        runtime_image.objects.push_back(std::move(object));
        runtime::AotTranslationPlan candidate;
        return runtime::BuildAotTranslationPlanFromEntry(
                   runtime_image, runtime_image.relocated_entry_linear_address,
                   &candidate) &&
            !candidate.blocks.empty() &&
            !candidate.blocks[0].instructions.empty() &&
            candidate.blocks[0].instructions[0].kind ==
                runtime::AotInstructionKind::kGuardedSegmentPop &&
            candidate.blocks[0].instructions[0].segment_register ==
                expected_segment;
    };
    const bool guarded_pop_supported_forms =
        becomes_guarded_pop({0x07U, 0xC3U}, 0U) &&
        becomes_guarded_pop({0x1FU, 0xC3U}, 3U) &&
        becomes_guarded_pop({0x0FU, 0xA1U, 0xC3U}, 4U) &&
        becomes_guarded_pop({0x0FU, 0xA9U, 0xC3U}, 5U);

    void* memory = VirtualAlloc(
        nullptr, 4096U, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    bool native_patch = false;
    bool hle_patch = false;
    bool guarded_pop_patch = false;
    if (memory != nullptr)
    {
        auto* bytes = static_cast<std::uint8_t*>(memory);
        bytes[0] = 0xCCU;
        platform::win32::Win32AotCodeCachePlacement placement;
        placement.valid = true;
        placement.placed = true;
        placement.base_address = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(memory));
        placement.size = 78U;
        placement.capacity = 4096U;
        runtime::AotSegmentOverrideSite site;
        site.cache_offset = 0U;
        site.guard_address_offset = 4U;
        site.guard_selector_offset = 8U;
        site.displacement_offset = 12U;
        site.original_displacement = -4;
        site.segment_register = 5U;
        placement.segment_override_sites.push_back(site);
        bytes[32U] = 0xCCU;
        runtime::AotGuardedSegmentPopSite pop_site;
        pop_site.cache_offset = 32U;
        pop_site.shadow_address_offset = 46U;
        pop_site.success_counter_address_offset = 54U;
        pop_site.fallback_counter_address_offset = 71U;
        pop_site.fallback_offset = 77U;
        pop_site.segment_register = 5U;
        placement.guarded_segment_pop_sites.push_back(pop_site);

        platform::win32::Win32AotSegmentTable segment_table;
        segment_table.segments[5] = nonflat;
        platform::win32::Win32AotSegmentPatchStats native_stats;
        const std::uint32_t native_processed =
            platform::win32::ReResolveWin32AotSegmentOverrides(
                &placement, &segment_table, &native_stats);
        std::uint32_t patched_shadow = 0;
        std::uint16_t patched_selector = 0;
        std::uint32_t patched_displacement = 0;
        std::memcpy(&patched_shadow, bytes + 4U, sizeof(patched_shadow));
        std::memcpy(&patched_selector, bytes + 8U, sizeof(patched_selector));
        std::memcpy(
            &patched_displacement, bytes + 12U,
            sizeof(patched_displacement));
        std::uint32_t patched_pop_shadow = 0U;
        std::uint32_t patched_success_counter = 0U;
        std::uint32_t patched_fallback_counter = 0U;
        std::memcpy(&patched_pop_shadow, bytes + 46U,
                    sizeof(patched_pop_shadow));
        std::memcpy(&patched_success_counter, bytes + 54U,
                    sizeof(patched_success_counter));
        std::memcpy(&patched_fallback_counter, bytes + 71U,
                    sizeof(patched_fallback_counter));
        const std::uint32_t expected_success_counter =
            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
                &placement.guarded_segment_pop_success_count));
        const std::uint32_t expected_fallback_counter =
            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(
                &placement.guarded_segment_pop_fallback_count));
        native_patch =
            native_processed == 2U && native_stats.native_site_count == 1U &&
            native_stats.guarded_pop_site_count == 1U &&
            bytes[0] == 0x9CU && patched_shadow == kShadowAddress &&
            patched_selector == 0x0088U &&
            patched_displacement == 0x01FFFFFCU;
        guarded_pop_patch = bytes[32U] == 0x9CU &&
            patched_pop_shadow == kShadowAddress &&
            patched_success_counter == expected_success_counter &&
            patched_fallback_counter == expected_fallback_counter;

        segment_table.segments[5] = selector_zero;
        platform::win32::Win32AotSegmentPatchStats hle_stats;
        const std::uint32_t hle_processed =
            platform::win32::ReResolveWin32AotSegmentOverrides(
                &placement, &segment_table, &hle_stats);
        hle_patch =
            hle_processed == 2U && hle_stats.hle_site_count == 1U &&
            hle_stats.guarded_pop_site_count == 1U &&
            bytes[0] == 0xCCU && bytes[32U] == 0x9CU;
        VirtualFree(memory, 0, MEM_RELEASE);
    }

    runtime::AotInstructionRecord hle_record;
    hle_record.guest_address = 0x00105000U;
    hle_record.fallthrough_target = 0x00105001U;
    hle_record.kind = runtime::AotInstructionKind::kHleBoundary;
    hle_record.length = 1U;
    hle_record.bytes = {0xFAU};
    runtime::AotInstructionRecord hle_return;
    hle_return.guest_address = hle_record.fallthrough_target;
    hle_return.kind = runtime::AotInstructionKind::kReturn;
    hle_return.length = 1U;
    hle_return.bytes = {0xC3U};
    runtime::AotBasicBlock hle_block;
    hle_block.guest_address = hle_record.guest_address;
    hle_block.instructions.push_back(hle_record);
    runtime::AotBasicBlock hle_return_block;
    hle_return_block.guest_address = hle_return.guest_address;
    hle_return_block.instructions.push_back(hle_return);
    runtime::AotTranslationPlan hle_plan;
    hle_plan.valid = true;
    hle_plan.entry_address = hle_record.guest_address;
    hle_plan.block_count = 2U;
    hle_plan.instruction_count = 2U;
    hle_plan.source_code_bytes = 2U;
    hle_plan.hle_boundary_count = 1U;
    hle_plan.return_count = 1U;
    hle_plan.blocks.push_back(hle_block);
    hle_plan.blocks.push_back(hle_return_block);
    runtime::AotCodeCacheBuildOptions hle_options;
    hle_options.enable_dbt_hle_dispatch = true;
    runtime::AotCodeCacheImage hle_image;
    const bool hle_dispatch_ready =
        runtime::BuildAotCodeCacheImage(
            hle_plan, hle_options, &hle_image) &&
        hle_image.dbt_hle_dispatch_sites.size() == 1U;
    bool hle_dispatch_layout = false;
    bool hle_dispatch_coverage = false;
    bool hle_dispatch_placement = false;
    if (hle_dispatch_ready)
    {
        const runtime::AotDbtHleDispatchSite& site =
            hle_image.dbt_hle_dispatch_sites[0];
        const std::uint32_t slot = site.dispatch_cache_offset;
        hle_dispatch_layout =
            slot + 21U <= hle_image.bytes.size() &&
            hle_image.bytes[slot] == 0x68U &&
            hle_image.bytes[slot + 5U] == 0x68U &&
            hle_image.bytes[slot + 10U] == 0xE9U &&
            hle_image.bytes[slot + 19U] == 0xCCU &&
            hle_image.bytes[slot + 20U] == 0xC3U;
        runtime::AotCodeCacheImage broken_hle = hle_image;
        broken_hle.bytes[slot + 19U] = 0x90U;
        std::uint32_t failure_guest = 0U;
        hle_dispatch_coverage =
            runtime::ValidateAotCodeCacheHleCoverage(
                hle_plan, hle_image) &&
            !runtime::ValidateAotCodeCacheHleCoverage(
                hle_plan, broken_hle, &failure_guest) &&
            failure_guest == hle_record.guest_address;

        platform::win32::Win32AotCodeCachePlacement hle_placement;
        if (platform::win32::PlaceWin32AotCodeCache(
                hle_image, &hle_placement) &&
            hle_placement.placed &&
            hle_placement.dbt_hle_dispatch_sites.size() == 1U)
        {
            std::uint32_t patched_dispatch = 0U;
            std::memcpy(
                &patched_dispatch,
                reinterpret_cast<const void*>(
                    static_cast<std::uintptr_t>(
                        hle_placement.base_address +
                        site.dispatch_address_immediate_offset)),
                sizeof(patched_dispatch));
            hle_dispatch_placement =
                patched_dispatch ==
                    hle_placement.base_address + site.dispatch_cache_offset;
        }
        platform::win32::ReleaseWin32AotCodeCache(&hle_placement);
    }

    runtime::AotTranslationPlan port_plan = hle_plan;
    port_plan.blocks[0].instructions[0].kind =
        runtime::AotInstructionKind::kPortIo;
    runtime::AotCodeCacheBuildOptions port_options;
    port_options.enable_dbt_port_io_dispatch = true;
    runtime::AotCodeCacheImage port_image;
    runtime::AotCodeCacheImage ordinary_hle_under_port_option;
    const bool port_io_dispatch_specific =
        runtime::BuildAotCodeCacheImage(
            port_plan, port_options, &port_image) &&
        port_image.dbt_hle_dispatch_sites.size() == 1U &&
        port_image.dbt_port_io_dispatch_enabled &&
        runtime::ValidateAotCodeCacheHleCoverage(port_plan, port_image) &&
        runtime::BuildAotCodeCacheImage(
            hle_plan, port_options, &ordinary_hle_under_port_option) &&
        ordinary_hle_under_port_option.dbt_hle_dispatch_sites.empty() &&
        !ordinary_hle_under_port_option.address_map.empty() &&
        ordinary_hle_under_port_option.bytes[
            ordinary_hle_under_port_option.address_map[0].cache_offset] ==
                0xCCU;
    runtime::AotTranslationPlan segment_dispatch_plan = hle_plan;
    auto& segment_dispatch_record =
        segment_dispatch_plan.blocks[0].instructions[0];
    segment_dispatch_record.kind =
        runtime::AotInstructionKind::kSegmentOverrideMem;
    segment_dispatch_record.length = 3U;
    segment_dispatch_record.bytes = {0x26U, 0x8AU, 0x13U};
    segment_dispatch_record.segment_override_register = 0U;
    segment_dispatch_record.fallthrough_target =
        segment_dispatch_record.guest_address + 3U;
    auto& segment_dispatch_return =
        segment_dispatch_plan.blocks[1].instructions[0];
    segment_dispatch_return.guest_address =
        segment_dispatch_record.fallthrough_target;
    segment_dispatch_plan.blocks[1].guest_address =
        segment_dispatch_return.guest_address;
    runtime::AotCodeCacheBuildOptions segment_dispatch_options;
    segment_dispatch_options.enable_dbt_segment_override_dispatch = true;
    runtime::AotCodeCacheImage segment_dispatch_image;
    runtime::AotCodeCacheImage segment_dispatch_disabled_image;
    bool segment_override_dispatch_specific =
        runtime::BuildAotCodeCacheImage(
            segment_dispatch_plan, segment_dispatch_options,
            &segment_dispatch_image) &&
        segment_dispatch_image.dbt_segment_override_dispatch_enabled &&
        segment_dispatch_image.dbt_hle_dispatch_sites.size() == 1U &&
        segment_dispatch_image.segment_override_sites.size() == 1U &&
        runtime::ValidateAotCodeCacheHleCoverage(
            segment_dispatch_plan, segment_dispatch_image) &&
        runtime::BuildAotCodeCacheImage(
            segment_dispatch_plan, runtime::AotCodeCacheBuildOptions{},
            &segment_dispatch_disabled_image) &&
        !segment_dispatch_disabled_image.
            dbt_segment_override_dispatch_enabled &&
        segment_dispatch_disabled_image.dbt_hle_dispatch_sites.empty() &&
        segment_dispatch_disabled_image.segment_override_sites.size() == 1U &&
        runtime::ValidateAotCodeCacheHleCoverage(
            segment_dispatch_plan, segment_dispatch_disabled_image);
    if (segment_override_dispatch_specific)
    {
        runtime::AotCodeCacheImage broken_segment_dispatch =
            segment_dispatch_image;
        const auto& segment_dispatch_site =
            broken_segment_dispatch.dbt_hle_dispatch_sites[0];
        broken_segment_dispatch.bytes[
            segment_dispatch_site.fallback_cache_offset + 4U] = 0x90U;
        std::uint32_t failure_guest = 0U;
        segment_override_dispatch_specific =
            !runtime::ValidateAotCodeCacheHleCoverage(
                segment_dispatch_plan, broken_segment_dispatch,
                &failure_guest) &&
            failure_guest == segment_dispatch_record.guest_address;
    }
    bool segment_override_hybrid_patch = false;
    if (segment_override_dispatch_specific)
    {
        platform::win32::Win32AotCodeCachePlacement hybrid_placement;
        if (platform::win32::PlaceWin32AotCodeCache(
                segment_dispatch_image, &hybrid_placement) &&
            hybrid_placement.placed &&
            hybrid_placement.segment_override_sites.size() == 1U &&
            hybrid_placement.dbt_hle_dispatch_sites.size() == 1U)
        {
            const auto hybrid_site =
                hybrid_placement.segment_override_sites[0];
            auto* hybrid_bytes = reinterpret_cast<std::uint8_t*>(
                static_cast<std::uintptr_t>(hybrid_placement.base_address));
            platform::win32::Win32AotSegmentTable hybrid_table{};
            hybrid_table.segments[0] = nonflat;
            platform::win32::Win32AotSegmentPatchStats hybrid_native_stats;
            const std::uint32_t hybrid_native_processed =
                platform::win32::ReResolveWin32AotSegmentOverrides(
                    &hybrid_placement, &hybrid_table,
                    &hybrid_native_stats);
            const bool native_routed = hybrid_native_processed == 1U &&
                hybrid_native_stats.native_site_count == 1U &&
                hybrid_bytes[hybrid_site.cache_offset] == 0x9CU;

            hybrid_table.segments[0] = selector_zero;
            platform::win32::Win32AotSegmentPatchStats hybrid_hle_stats;
            const std::uint32_t hybrid_hle_processed =
                platform::win32::ReResolveWin32AotSegmentOverrides(
                    &hybrid_placement, &hybrid_table, &hybrid_hle_stats);
            std::int32_t hybrid_relative = 0;
            std::memcpy(&hybrid_relative,
                        hybrid_bytes + hybrid_site.cache_offset + 1U,
                        sizeof(hybrid_relative));
            const std::uint32_t hybrid_target = static_cast<std::uint32_t>(
                hybrid_site.cache_offset + 5U + hybrid_relative);
            const bool hle_routed = hybrid_hle_processed == 1U &&
                hybrid_hle_stats.hle_site_count == 1U &&
                hybrid_bytes[hybrid_site.cache_offset] == 0xE9U &&
                hybrid_target == hybrid_site.dispatch_cache_offset;

            hybrid_table.segments[0] = unresolved;
            platform::win32::Win32AotSegmentPatchStats hybrid_unresolved_stats;
            const std::uint32_t hybrid_unresolved_processed =
                platform::win32::ReResolveWin32AotSegmentOverrides(
                    &hybrid_placement, &hybrid_table,
                    &hybrid_unresolved_stats);
            const bool unresolved_routed =
                hybrid_unresolved_processed == 1U &&
                hybrid_unresolved_stats.unresolved_site_count == 1U &&
                hybrid_bytes[hybrid_site.cache_offset] == 0xCCU;
            segment_override_hybrid_patch =
                native_routed && hle_routed && unresolved_routed;
        }
        platform::win32::ReleaseWin32AotCodeCache(&hybrid_placement);
    }
    hle::GlideGatePlan glide_direct_plan;
    glide_direct_plan.valid = true;
    glide_direct_plan.first_gate_offset = 0x100U;
    glide_direct_plan.gate_stride = 8U;
    glide_direct_plan.exports.push_back(
        {"_TEST@12", 2U, hle::GlideGateId::kUnknown, 12U, 0x110U});
    glide_direct_plan.image.assign(24U, 0x90U);
    glide_direct_plan.image[16U] = 0x0FU;
    glide_direct_plan.image[17U] = 0x0BU;
    glide_direct_plan.image[18U] = 0x02U;
    glide_direct_plan.image[19U] = 0x00U;
    glide_direct_plan.image[20U] = 0xC3U;
    hle::GlideGatePlan invalid_glide_direct_plan = glide_direct_plan;
    invalid_glide_direct_plan.image[18U] = 0x03U;
    const auto invalid_before = invalid_glide_direct_plan.image;
    constexpr std::uint32_t kSyntheticGateBase = 0x03000000U;
    const bool glide_direct_patched =
        platform::win32::PatchWin32GlideGatePlanForDirectDispatch(
            kSyntheticGateBase, &glide_direct_plan);
    std::int32_t glide_call_displacement = 0;
    if (glide_direct_patched)
    {
        std::memcpy(&glide_call_displacement,
                    glide_direct_plan.image.data() + 17U,
                    sizeof(glide_call_displacement));
    }
    const std::uint32_t glide_call_target = static_cast<std::uint32_t>(
        kSyntheticGateBase + 0x110U + 5U + glide_call_displacement);
    const bool glide_direct_dispatch_layout = glide_direct_patched &&
        glide_direct_plan.image[16U] == 0xE8U &&
        glide_call_target == static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(platform::win32::
                GetWin32GlideGateDirectDispatchThunkAddress())) &&
        glide_direct_plan.image[21U] == 0xC2U &&
        glide_direct_plan.image[22U] == 0x0CU &&
        glide_direct_plan.image[23U] == 0x00U &&
        !platform::win32::PatchWin32GlideGatePlanForDirectDispatch(
            kSyntheticGateBase, &invalid_glide_direct_plan) &&
        invalid_glide_direct_plan.image == invalid_before;
    const bool glide_direct_dispatch_policy =
        platform::win32::ResolveWin32GlideGateDirectDispatchEnabled(nullptr) &&
        platform::win32::ResolveWin32GlideGateDirectDispatchEnabled("1") &&
        platform::win32::ResolveWin32GlideGateDirectDispatchEnabled("on") &&
        platform::win32::ResolveWin32GlideGateDirectDispatchEnabled("true") &&
        !platform::win32::ResolveWin32GlideGateDirectDispatchEnabled("") &&
        !platform::win32::ResolveWin32GlideGateDirectDispatchEnabled("0") &&
        !platform::win32::ResolveWin32GlideGateDirectDispatchEnabled("off") &&
        !platform::win32::ResolveWin32GlideGateDirectDispatchEnabled("false") &&
        !platform::win32::ResolveWin32GlideGateDirectDispatchEnabled("invalid");
    const bool policy =
        !platform::win32::ResolveAotDbtPostHleTranslationEnabled("") &&
        platform::win32::ResolveAotDbtPostHleTranslationEnabled("1") &&
        platform::win32::ResolveAotDbtPostHleTranslationEnabled("on") &&
        platform::win32::ResolveAotDbtPostHleTranslationEnabled("true") &&
        !platform::win32::ResolveAotDbtPostHleTranslationEnabled("0") &&
        !platform::win32::ResolveAotDbtPostHleTranslationEnabled("invalid");
    const bool all = descriptor_policy && mismatch_fails_closed &&
        whole_cfg_coverage && missing_guard_rejected && native_patch &&
        hle_patch && guarded_pop_patch && guarded_pop_ready &&
        guarded_pop_layout &&
        guarded_pop_coverage && guarded_pop_missing_fallback_rejected &&
        guarded_pop_disabled_falls_back && guarded_load_ready &&
        guarded_load_layout && guarded_load_coverage &&
        guarded_load_missing_fallback_rejected && guarded_load_patch &&
        guarded_load_disabled_falls_back && guarded_load_rejected_forms &&
        guarded_read_ready &&
        guarded_read_layout && guarded_read_patch &&
        guarded_read_disabled_falls_back && guarded_pop_rejected_forms &&
        guarded_pop_supported_forms && hle_dispatch_ready &&
        hle_dispatch_layout && hle_dispatch_coverage &&
        hle_dispatch_placement && port_io_dispatch_specific &&
        segment_override_dispatch_specific &&
        segment_override_hybrid_patch && glide_direct_dispatch_layout && glide_direct_dispatch_policy && policy;
    std::cout << "selector_guard_descriptor_policy="
              << (descriptor_policy ? "true" : "false")
              << "\nselector_guard_mismatch_fail_closed="
              << (mismatch_fails_closed ? "true" : "false")
              << "\nselector_guard_whole_cfg_coverage="
              << (whole_cfg_coverage ? "true" : "false")
              << "\nselector_guard_missing_guard_rejected="
              << (missing_guard_rejected ? "true" : "false")
              << "\nselector_guard_native_patch="
              << (native_patch ? "true" : "false")
              << "\nselector_guard_hle_patch="
              << (hle_patch ? "true" : "false")
              << "\nguarded_segment_pop_patch="
              << (guarded_pop_patch ? "true" : "false")
              << "\nguarded_segment_pop_ready="
              << (guarded_pop_ready ? "true" : "false")
              << "\nguarded_segment_pop_layout="
              << (guarded_pop_layout ? "true" : "false")
              << "\nguarded_segment_pop_coverage="
              << (guarded_pop_coverage ? "true" : "false")
              << "\nguarded_segment_pop_missing_fallback_rejected="
              << (guarded_pop_missing_fallback_rejected ? "true" : "false")
              << "\nguarded_segment_pop_disabled_falls_back="
              << (guarded_pop_disabled_falls_back ? "true" : "false")
              << "\nguarded_segment_load_ready="
              << (guarded_load_ready ? "true" : "false")
              << "\nguarded_segment_load_layout="
              << (guarded_load_layout ? "true" : "false")
              << "\nguarded_segment_load_coverage="
              << (guarded_load_coverage ? "true" : "false")
              << "\nguarded_segment_load_missing_fallback_rejected="
              << (guarded_load_missing_fallback_rejected ? "true" : "false")
              << "\nguarded_segment_load_patch="
              << (guarded_load_patch ? "true" : "false")
              << "\nguarded_segment_load_disabled_falls_back="
              << (guarded_load_disabled_falls_back ? "true" : "false")
              << "\nguarded_segment_load_rejected_forms="
              << (guarded_load_rejected_forms ? "true" : "false")
              << "\nguarded_segment_read_ready="
              << (guarded_read_ready ? "true" : "false")
              << "\nguarded_segment_read_layout="
              << (guarded_read_layout ? "true" : "false")
              << "\nguarded_segment_read_patch="
              << (guarded_read_patch ? "true" : "false")
              << "\nguarded_segment_read_disabled_falls_back="
              << (guarded_read_disabled_falls_back ? "true" : "false")
              << "\nguarded_segment_pop_rejected_forms="
              << (guarded_pop_rejected_forms ? "true" : "false")
              << "\nguarded_segment_pop_supported_forms="
              << (guarded_pop_supported_forms ? "true" : "false")
              << "\nglide_direct_dispatch_layout="
              << (glide_direct_dispatch_layout ? "true" : "false")
              << "\nglide_direct_dispatch_policy="
              << (glide_direct_dispatch_policy ? "true" : "false")
              << "\nport_io_dispatch_specific="
              << (port_io_dispatch_specific ? "true" : "false")
              << "\nsegment_override_dispatch_specific="
              << (segment_override_dispatch_specific ? "true" : "false")
              << "\nsegment_override_hybrid_patch="
              << (segment_override_hybrid_patch ? "true" : "false")
              << "\nsuperblock_hle_dispatch_ready="
              << (hle_dispatch_ready ? "true" : "false")
              << "\nsuperblock_hle_dispatch_layout="
              << (hle_dispatch_layout ? "true" : "false")
              << "\nsuperblock_hle_dispatch_coverage="
              << (hle_dispatch_coverage ? "true" : "false")
              << "\nsuperblock_hle_dispatch_placement="
              << (hle_dispatch_placement ? "true" : "false")
              << "\nselector_guard_post_hle_policy="
              << (policy ? "true" : "false")
              << "\nselector_guard_all=" << (all ? "true" : "false")
              << "\n";
    return all;
#endif
}

}  // namespace repiu::tools
