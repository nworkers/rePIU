#include "inline_cache_probe.h"

#include "repiu/platform/win32/aot_code_cache_win32.h"
#include "repiu/platform/win32/aot_page_coherence_win32.h"
#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_translation_plan.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

namespace repiu::tools
{
namespace
{

constexpr std::uint32_t kGuestPage = 0x00101000U;
constexpr std::size_t kExpectedEntryCount = 4U;

std::uint32_t ReadUint32(const std::uint8_t* bytes,
                         std::uint32_t offset)
{
    std::uint32_t value = 0;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

std::int32_t ReadInt32(const std::uint8_t* bytes,
                       std::uint32_t offset)
{
    std::int32_t value = 0;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

runtime::AotInstructionRecord MakeIndirectInstruction(
    std::uint32_t guest_address,
    std::uint8_t modrm)
{
    runtime::AotInstructionRecord instruction;
    instruction.guest_address = guest_address;
    instruction.kind = runtime::AotInstructionKind::kIndirectExit;
    instruction.length = 2U;
    instruction.bytes = {0xFFU, modrm};
    return instruction;
}

bool ValidateInitialSite(const runtime::AotCodeCacheImage& image,
                         const runtime::AotIndirectInlineCacheSite& site,
                         bool is_call,
                         std::size_t expected_entry_count)
{
    if (site.entries.size() != expected_entry_count ||
        site.is_call != is_call || site.is_return ||
        site.target_immediate_offset !=
            site.entries[0].target_immediate_offset ||
        site.guard_offset != site.entries[0].guard_offset ||
        site.jump_displacement_offset !=
            site.entries[0].jump_displacement_offset ||
        site.miss_cache_offset + 2U > image.bytes.size() ||
        image.bytes[site.cache_offset] != 0x9CU ||
        image.bytes[site.miss_cache_offset] != 0x9DU ||
        image.bytes[site.miss_cache_offset + 1U] != 0xCCU)
    {
        return false;
    }

    for (std::size_t index = 0; index < site.entries.size(); ++index)
    {
        const runtime::AotInlineCacheEntry& entry = site.entries[index];
        if (entry.compare_offset + 6U > image.bytes.size() ||
            entry.guard_offset + 7U > image.bytes.size() ||
            entry.jump_displacement_offset + 4U > image.bytes.size() ||
            image.bytes[entry.compare_offset] != 0x81U ||
            image.bytes[entry.compare_offset + 1U] != 0xF8U ||
            entry.target_immediate_offset != entry.compare_offset + 2U ||
            entry.guard_offset != entry.target_immediate_offset + 4U ||
            image.bytes[entry.guard_offset] != 0xE9U ||
            image.bytes[entry.guard_offset + 5U] != 0x90U ||
            image.bytes[entry.guard_offset + 6U] != 0x9DU)
        {
            return false;
        }
        const std::int32_t miss_displacement =
            ReadInt32(image.bytes.data(), entry.guard_offset + 1U);
        if (static_cast<std::int64_t>(entry.guard_offset) + 5U +
                miss_displacement != site.miss_cache_offset)
        {
            return false;
        }
        const std::uint32_t expected_jump_displacement_offset = is_call
            ? entry.guard_offset + 13U : entry.guard_offset + 8U;
        if (entry.jump_displacement_offset !=
            expected_jump_displacement_offset)
        {
            return false;
        }
        if (is_call)
        {
            if (image.bytes[entry.guard_offset + 7U] != 0x68U ||
                ReadUint32(image.bytes.data(), entry.guard_offset + 8U) !=
                    site.guest_source + 2U ||
                image.bytes[entry.guard_offset + 12U] != 0xE9U)
            {
                return false;
            }
        }
        else if (image.bytes[entry.guard_offset + 7U] != 0xE9U)
        {
            return false;
        }
        if (index + 1U < site.entries.size() &&
            entry.jump_displacement_offset + 4U !=
                site.entries[index + 1U].compare_offset)
        {
            return false;
        }
    }
    return true;
}

bool PatchAndValidateSite(
    platform::win32::Win32AotCodeCachePlacement* placement,
    std::size_t site_index,
    std::uint32_t first_guest_target)
{
    if (placement == nullptr ||
        site_index >= placement->indirect_inline_cache_sites.size())
    {
        return false;
    }
    const runtime::AotIndirectInlineCacheSite& initial_site =
        placement->indirect_inline_cache_sites[site_index];
    const std::uint32_t miss_address =
        placement->base_address + initial_site.miss_cache_offset;
    for (std::size_t index = 0; index < kExpectedEntryCount; ++index)
    {
        platform::win32::Win32AotInlineCachePatchResult result;
        if (!platform::win32::PatchWin32AotIndirectInlineCache(
                placement, miss_address,
                first_guest_target + static_cast<std::uint32_t>(index * 4U),
                placement->entry_address, &result) || !result.patched)
        {
            return false;
        }
    }

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(placement->base_address));
    const runtime::AotIndirectInlineCacheSite& site =
        placement->indirect_inline_cache_sites[site_index];
    for (std::size_t index = 0; index < site.entries.size(); ++index)
    {
        const runtime::AotInlineCacheEntry& entry = site.entries[index];
        const std::uint32_t expected_guest_target =
            first_guest_target + static_cast<std::uint32_t>(index * 4U);
        const std::uint32_t expected_guard_target = index + 1U <
                site.entries.size()
            ? site.entries[index + 1U].compare_offset
            : site.miss_cache_offset;
        const std::int32_t guard_displacement =
            ReadInt32(bytes, entry.guard_offset + 2U);
        const std::int32_t cache_displacement =
            ReadInt32(bytes, entry.jump_displacement_offset);
        if (bytes[entry.guard_offset] != 0x0FU ||
            bytes[entry.guard_offset + 1U] != 0x85U ||
            ReadUint32(bytes, entry.target_immediate_offset) !=
                expected_guest_target ||
            static_cast<std::int64_t>(entry.guard_offset) + 6U +
                guard_displacement != expected_guard_target ||
            static_cast<std::int64_t>(placement->base_address) +
                entry.jump_displacement_offset + 4U + cache_displacement !=
                placement->entry_address)
        {
            return false;
        }
    }
    return true;
}

}  // namespace

bool RunAotIndirectInlineCacheProbe()
{
#if !defined(_WIN32)
    std::cout << "inline_cache_probe_skipped=true\n";
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

    runtime::AotCodeCacheImage image;
    const bool built = runtime::BuildAotCodeCacheImage(plan, &image);
    const bool call_layout = built &&
        image.indirect_inline_cache_sites.size() == 2U &&
        ValidateInitialSite(image, image.indirect_inline_cache_sites[0], true,
                            kExpectedEntryCount);
    const bool jump_layout = built &&
        image.indirect_inline_cache_sites.size() == 2U &&
        ValidateInitialSite(image, image.indirect_inline_cache_sites[1], false,
                            kExpectedEntryCount);

    runtime::AotCodeCacheBuildOptions one_entry_options;
    one_entry_options.indirect_inline_cache_entry_count = 1U;
    runtime::AotCodeCacheImage one_entry_image;
    const bool one_entry_layout = runtime::BuildAotCodeCacheImage(
            plan, one_entry_options, &one_entry_image) &&
        one_entry_image.indirect_inline_cache_entry_count == 1U &&
        one_entry_image.indirect_inline_cache_sites.size() == 2U &&
        ValidateInitialSite(one_entry_image,
                            one_entry_image.indirect_inline_cache_sites[0],
                            true, 1U) &&
        ValidateInitialSite(one_entry_image,
                            one_entry_image.indirect_inline_cache_sites[1],
                            false, 1U);

    platform::win32::Win32AotCodeCachePlacement placement;
    const bool placed = call_layout && jump_layout &&
        platform::win32::PlaceWin32AotCodeCache(image, &placement) &&
        placement.placed;
    const bool call_chain = placed && PatchAndValidateSite(
        &placement, 0U, kGuestPage + 0x100U);
    const bool jump_chain = call_chain && PatchAndValidateSite(
        &placement, 1U, kGuestPage + 0x200U);

    bool replacement = false;
    if (jump_chain)
    {
        const runtime::AotIndirectInlineCacheSite& call_site =
            placement.indirect_inline_cache_sites[0];
        platform::win32::Win32AotInlineCachePatchResult result;
        const std::uint32_t replacement_target = kGuestPage + 0x300U;
        replacement = platform::win32::PatchWin32AotIndirectInlineCache(
            &placement, placement.base_address + call_site.miss_cache_offset,
            replacement_target, placement.entry_address, &result) &&
            result.patched;
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(placement.base_address));
        replacement = replacement &&
            ReadUint32(bytes,
                       call_site.entries[0].target_immediate_offset) ==
                replacement_target && call_site.replace_cursor == 1U;
    }

    bool retirement = false;
    if (replacement)
    {
        platform::win32::Win32AotGuestPageRetireResult result;
        retirement = platform::win32::RetireWin32AotGuestPage(
            &placement, kGuestPage, false, &result) && result.retired &&
            result.guard_reset_count == 2U * kExpectedEntryCount;
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(placement.base_address));
        for (const runtime::AotIndirectInlineCacheSite& site :
             placement.indirect_inline_cache_sites)
        {
            for (const runtime::AotInlineCacheEntry& entry : site.entries)
            {
                retirement = retirement &&
                    bytes[entry.guard_offset] == 0xE9U &&
                    bytes[entry.guard_offset + 5U] == 0x90U;
            }
        }
    }

    const bool all = call_layout && jump_layout && one_entry_layout && placed &&
        call_chain && jump_chain && replacement && retirement;
    std::cout << "inline_cache_call_layout="
              << (call_layout ? "true" : "false")
              << "\ninline_cache_jump_layout="
              << (jump_layout ? "true" : "false")
              << "\ninline_cache_one_entry_layout="
              << (one_entry_layout ? "true" : "false")
              << "\ninline_cache_call_chain="
              << (call_chain ? "true" : "false")
              << "\ninline_cache_jump_chain="
              << (jump_chain ? "true" : "false")
              << "\ninline_cache_round_robin="
              << (replacement ? "true" : "false")
              << "\ninline_cache_retirement="
              << (retirement ? "true" : "false")
              << "\ninline_cache_all=" << (all ? "true" : "false")
              << "\n";
    platform::win32::ReleaseWin32AotCodeCache(&placement);
    return all;
#endif
}

}  // namespace repiu::tools
