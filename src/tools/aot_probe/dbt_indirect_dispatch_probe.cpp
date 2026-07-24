#include "dbt_indirect_dispatch_probe.h"

#include <iostream>

#if defined(_WIN32)
#include "repiu/platform/win32/aot_code_cache_win32.h"
#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_translation_plan.h"
#include "../../platform/win32/aot/aot_dbt_indirect_dispatch.h"
#include "../../platform/win32/execution/thread_context.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#endif

namespace repiu::tools
{
#if defined(_WIN32)
namespace
{

constexpr std::uint32_t kGuestPage = 0x00121000U;

std::uint32_t ReadUint32(const std::uint8_t* bytes, std::uint32_t offset)
{
    std::uint32_t value = 0;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

std::int32_t ReadInt32(const std::uint8_t* bytes, std::uint32_t offset)
{
    std::int32_t value = 0;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

runtime::AotInstructionRecord MakeIndirectInstruction(
    std::uint32_t guest_address, std::uint8_t modrm)
{
    runtime::AotInstructionRecord instruction;
    instruction.guest_address = guest_address;
    instruction.kind = runtime::AotInstructionKind::kIndirectExit;
    instruction.length = 2U;
    instruction.bytes = {0xFFU, modrm};
    return instruction;
}

// The host-dispatch miss tail: popfd, three pushes, jmp thunk, the fallback
// continuation (lea esp,[esp+8]; int3), then the per-kind success ret.
bool ValidateDispatchLayout(const runtime::AotCodeCacheImage& image,
                            const runtime::AotDbtIndirectDispatchSite& site,
                            bool is_call)
{
    const std::uint8_t* b = image.bytes.data();
    if (site.is_call != is_call ||
        site.miss_cache_offset + 1U > image.bytes.size() ||
        b[site.miss_cache_offset] != 0x9DU ||          // popfd
        b[site.miss_cache_offset + 1U] != 0x68U)       // push A
    {
        return false;
    }
    // push A immediate is the return address (guest_source + length) for a
    // call, zero for a jump.
    const std::uint32_t push_a = ReadUint32(b, site.miss_cache_offset + 2U);
    if (is_call)
    {
        if (push_a != site.guest_source + 2U)
        {
            return false;
        }
    }
    else if (push_a != 0U)
    {
        return false;
    }
    // Fallback continuation removes the two remaining slots then int3.
    if (site.fallback_cache_offset + 5U > image.bytes.size() ||
        b[site.fallback_cache_offset] != 0x8DU ||
        b[site.fallback_cache_offset + 1U] != 0x64U ||
        b[site.fallback_cache_offset + 2U] != 0x24U ||
        b[site.fallback_cache_offset + 3U] != 0x08U ||
        b[site.fallback_cache_offset + 4U] != 0xCCU)
    {
        return false;
    }
    // Success continuation reproduces the call/jump stack semantics.
    if (is_call)
    {
        if (site.success_cache_offset + 1U > image.bytes.size() ||
            b[site.success_cache_offset] != 0xC3U)
        {
            return false;
        }
    }
    else
    {
        if (site.success_cache_offset + 3U > image.bytes.size() ||
            b[site.success_cache_offset] != 0xC2U ||
            b[site.success_cache_offset + 1U] != 0x04U ||
            b[site.success_cache_offset + 2U] != 0x00U)
        {
            return false;
        }
    }
    return true;
}

bool ValidatePlacement(const platform::win32::Win32AotCodeCachePlacement&
                           placement)
{
    if (placement.dbt_indirect_dispatch_sites.size() != 2U)
    {
        return false;
    }
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(placement.base_address));
    const std::uint32_t thunk = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(
            platform::win32::GetAotDbtIndirectMissThunkAddress()));
    for (const runtime::AotDbtIndirectDispatchSite& site :
         placement.dbt_indirect_dispatch_sites)
    {
        const std::uint32_t miss =
            placement.base_address + site.miss_cache_offset;
        const std::uint32_t next =
            placement.base_address + site.thunk_displacement_offset + 4U;
        const std::uint32_t resolved_thunk = next +
            static_cast<std::uint32_t>(
                ReadInt32(bytes, site.thunk_displacement_offset));
        if (ReadUint32(bytes, site.miss_address_immediate_offset) != miss ||
            resolved_thunk != thunk)
        {
            return false;
        }
    }
    return true;
}

}  // namespace
#endif

bool RunAotDbtIndirectDispatchProbe()
{
#if !defined(_WIN32)
    std::cout << "dbt_indirect_dispatch_probe_skipped=true\n";
    return true;
#else
    runtime::AotTranslationPlan plan;
    plan.valid = true;
    plan.entry_address = kGuestPage;
    runtime::AotBasicBlock call_block;
    call_block.guest_address = kGuestPage;
    call_block.instructions.push_back(
        MakeIndirectInstruction(kGuestPage, 0xD0U));  // call eax
    runtime::AotBasicBlock jump_block;
    jump_block.guest_address = kGuestPage + 0x20U;
    jump_block.instructions.push_back(
        MakeIndirectInstruction(kGuestPage + 0x20U, 0xE0U));  // jmp eax
    plan.blocks.push_back(std::move(call_block));
    plan.blocks.push_back(std::move(jump_block));

    // Disabled: the legacy miss tail stays popfd; int3 with no dispatch site.
    runtime::AotCodeCacheImage legacy_image;
    const bool disabled_layout =
        runtime::BuildAotCodeCacheImage(plan, &legacy_image) &&
        legacy_image.dbt_indirect_dispatch_sites.empty() &&
        legacy_image.indirect_inline_cache_sites.size() == 2U &&
        legacy_image.bytes[
            legacy_image.indirect_inline_cache_sites[0].miss_cache_offset] ==
            0x9DU &&
        legacy_image.bytes[
            legacy_image.indirect_inline_cache_sites[0].miss_cache_offset +
            1U] == 0xCCU;

    runtime::AotCodeCacheBuildOptions options;
    options.enable_dbt_indirect_miss_dispatch = true;
    runtime::AotCodeCacheImage image;
    const bool built =
        runtime::BuildAotCodeCacheImage(plan, options, &image) &&
        image.dbt_indirect_dispatch_sites.size() == 2U;
    const bool call_layout = built &&
        ValidateDispatchLayout(image, image.dbt_indirect_dispatch_sites[0],
                               true);
    const bool jump_layout = built &&
        ValidateDispatchLayout(image, image.dbt_indirect_dispatch_sites[1],
                               false);

    platform::win32::Win32AotCodeCachePlacement placement;
    const bool placed = call_layout && jump_layout &&
        platform::win32::PlaceWin32AotCodeCache(image, &placement) &&
        placement.placed && placement.dbt_indirect_miss_dispatch_enabled;
    const bool placement_ok = placed && ValidatePlacement(placement);

    auto context =
        std::make_unique<platform::win32::ThreadContext>();
    for (std::uint32_t index = 0;
         index < platform::win32::kAotDbtDispatchFallbackReasonCount; ++index)
    {
        platform::win32::RecordAotDbtIndirectFallback(
            context.get(),
            static_cast<platform::win32::AotDbtDispatchFallbackReason>(index));
    }
    const std::uint32_t total =
        context->aot_dbt_indirect_fallback_count.load(
            std::memory_order_relaxed);
    std::uint32_t reason_total = 0;
    bool slots = true;
    for (std::uint32_t index = 0;
         index < platform::win32::kAotDbtDispatchFallbackReasonCount; ++index)
    {
        const std::uint32_t count =
            context->aot_dbt_indirect_fallback_reason_counts[index].load(
                std::memory_order_relaxed);
        reason_total += count;
        slots = slots && count == 1U;
    }
    const bool accounting =
        total == platform::win32::kAotDbtDispatchFallbackReasonCount &&
        total == reason_total;

    const bool all = disabled_layout && call_layout && jump_layout &&
        placement_ok && slots && accounting;
    std::cout << "dbt_indirect_dispatch_disabled_layout="
              << (disabled_layout ? "true" : "false")
              << "\ndbt_indirect_dispatch_call_layout="
              << (call_layout ? "true" : "false")
              << "\ndbt_indirect_dispatch_jump_layout="
              << (jump_layout ? "true" : "false")
              << "\ndbt_indirect_dispatch_placement="
              << (placement_ok ? "true" : "false")
              << "\ndbt_indirect_dispatch_slots="
              << (slots ? "true" : "false")
              << "\ndbt_indirect_dispatch_accounting="
              << (accounting ? "true" : "false")
              << "\ndbt_indirect_dispatch_all="
              << (all ? "true" : "false") << "\n";
    return all;
#endif
}

}  // namespace repiu::tools
