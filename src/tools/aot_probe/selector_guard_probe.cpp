#include "selector_guard_probe.h"

#include "repiu/platform/win32/aot_code_cache_win32.h"
#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_translation_plan.h"
#include "repiu/runtime/selector_table.h"
#include "aot/aot_dbt_dispatch.h"

#include <cstdint>
#include <cstring>
#include <iostream>

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

    void* memory = VirtualAlloc(
        nullptr, 4096U, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    bool native_patch = false;
    bool hle_patch = false;
    if (memory != nullptr)
    {
        auto* bytes = static_cast<std::uint8_t*>(memory);
        bytes[0] = 0xCCU;
        platform::win32::Win32AotCodeCachePlacement placement;
        placement.valid = true;
        placement.placed = true;
        placement.base_address = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(memory));
        placement.size = 32U;
        placement.capacity = 4096U;
        runtime::AotSegmentOverrideSite site;
        site.cache_offset = 0U;
        site.guard_address_offset = 4U;
        site.guard_selector_offset = 8U;
        site.displacement_offset = 12U;
        site.original_displacement = -4;
        site.segment_register = 5U;
        placement.segment_override_sites.push_back(site);

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
        native_patch =
            native_processed == 1U && native_stats.native_site_count == 1U &&
            bytes[0] == 0x9CU && patched_shadow == kShadowAddress &&
            patched_selector == 0x0088U &&
            patched_displacement == 0x01FFFFFCU;

        segment_table.segments[5] = selector_zero;
        platform::win32::Win32AotSegmentPatchStats hle_stats;
        const std::uint32_t hle_processed =
            platform::win32::ReResolveWin32AotSegmentOverrides(
                &placement, &segment_table, &hle_stats);
        hle_patch =
            hle_processed == 1U && hle_stats.hle_site_count == 1U &&
            bytes[0] == 0xCCU;
        VirtualFree(memory, 0, MEM_RELEASE);
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
        hle_patch && policy;
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
              << "\nselector_guard_post_hle_policy="
              << (policy ? "true" : "false")
              << "\nselector_guard_all=" << (all ? "true" : "false")
              << "\n";
    return all;
#endif
}

}  // namespace repiu::tools
