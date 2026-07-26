#include "selector_guard_probe.h"

#include "repiu/platform/win32/aot_code_cache_win32.h"
#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_translation_plan.h"
#include "repiu/runtime/selector_table.h"
#include "aot/aot_dbt_dispatch.h"

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
        guarded_pop_disabled_falls_back && guarded_pop_rejected_forms &&
        guarded_pop_supported_forms && hle_dispatch_ready &&
        hle_dispatch_layout && hle_dispatch_coverage &&
        hle_dispatch_placement && policy;
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
              << "\nguarded_segment_pop_rejected_forms="
              << (guarded_pop_rejected_forms ? "true" : "false")
              << "\nguarded_segment_pop_supported_forms="
              << (guarded_pop_supported_forms ? "true" : "false")
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
