#include "arena_view_probe.h"

#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_translation_plan.h"
#include "repiu/runtime/image_address.h"
#include "repiu/runtime/runtime_memory.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
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
#if defined(_WIN32)

constexpr std::uint32_t kPageSize = 0x1000U;
constexpr std::uint32_t kRegionBytes = 4U * kPageSize;

// A small control-flow graph exercising the paths whose results depend on
// FindBytes: a direct call, a taken conditional branch, a direct jump whose
// target leaves the object, an indirect exit, and plain copies. Written into
// live memory so both representations read exactly the same bytes.
void WriteGuestProgram(std::uint8_t* guest)
{
    std::memset(guest, 0xCC, kRegionBytes);

    // 0x00 entry block.
    std::uint32_t offset = 0;
    const std::uint8_t entry[] = {
        0xB8U, 0x78U, 0x56U, 0x34U, 0x12U,  // mov eax, 0x12345678
        0xE8U, 0x36U, 0x00U, 0x00U, 0x00U,  // call +0x36 -> 0x40
        0x85U, 0xC0U,                       // test eax, eax
        0x74U, 0x12U,                       // jz +0x12 -> 0x20
        0xB8U, 0x01U, 0x00U, 0x00U, 0x00U,  // mov eax, 1
        0xFFU, 0xD0U,                       // call eax (indirect exit)
    };
    std::memcpy(guest + offset, entry, sizeof(entry));

    // 0x20: jumps far outside the object, so outside_image_target_count moves.
    offset = 0x20U;
    const std::uint8_t leaves[] = {
        0x31U, 0xC0U,                       // xor eax, eax
        0xE9U, 0xD9U, 0xFFU, 0x0FU, 0x00U,  // jmp +0xFFFD9 -> base+0x100000
    };
    std::memcpy(guest + offset, leaves, sizeof(leaves));

    // 0x40: call target.
    offset = 0x40U;
    const std::uint8_t callee[] = {
        0xB8U, 0x02U, 0x00U, 0x00U, 0x00U,  // mov eax, 2
        0xC3U,                              // ret
    };
    std::memcpy(guest + offset, callee, sizeof(callee));
}

runtime::RelocatedRuntimeImage MakeImage(std::uint32_t base,
                                         std::uint32_t size,
                                         const std::uint8_t* live,
                                         bool external)
{
    runtime::RelocatedRuntimeImage image;
    image.valid = true;
    image.relocated_image_base = base;
    image.relocated_entry_linear_address = base;
    runtime::RelocatedRuntimeObject object;
    object.relocated_base_address = base;
    object.virtual_size = size;
    if (external)
    {
        object.external_bytes = live;
        object.external_byte_count = size;
    }
    else
    {
        // The pre-Task-329 representation, kept as the reference oracle.
        object.memory.assign(live, live + size);
    }
    image.objects.push_back(std::move(object));
    return image;
}

bool SameInstruction(const runtime::AotInstructionRecord& left,
                     const runtime::AotInstructionRecord& right)
{
    return left.guest_address == right.guest_address &&
        left.direct_target == right.direct_target &&
        left.fallthrough_target == right.fallthrough_target &&
        left.kind == right.kind && left.length == right.length &&
        left.table_index_register == right.table_index_register &&
        left.segment_override_register == right.segment_override_register &&
        left.segment_register == right.segment_register &&
        left.mnemonic == right.mnemonic && left.bytes == right.bytes &&
        left.table_targets == right.table_targets;
}

// Every field except `elapsed_microseconds`, which is wall-clock by
// construction. The instruction stream is compared byte for byte, so a plan
// that merely counted the same would still fail.
bool SamePlan(const runtime::AotTranslationPlan& left,
              const runtime::AotTranslationPlan& right)
{
    if (left.valid != right.valid ||
        left.entry_address != right.entry_address ||
        left.block_count != right.block_count ||
        left.instruction_count != right.instruction_count ||
        left.source_code_bytes != right.source_code_bytes ||
        left.estimated_emitted_bytes != right.estimated_emitted_bytes ||
        left.copy_instruction_count != right.copy_instruction_count ||
        left.direct_call_count != right.direct_call_count ||
        left.direct_jump_count != right.direct_jump_count ||
        left.conditional_branch_count != right.conditional_branch_count ||
        left.return_count != right.return_count ||
        left.hle_boundary_count != right.hle_boundary_count ||
        left.indirect_exit_count != right.indirect_exit_count ||
        left.jump_table_count != right.jump_table_count ||
        left.jump_table_target_count != right.jump_table_target_count ||
        left.outside_image_target_count != right.outside_image_target_count ||
        left.decode_failure_count != right.decode_failure_count ||
        left.analysis_limit_count != right.analysis_limit_count ||
        left.message != right.message ||
        left.blocks.size() != right.blocks.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.blocks.size(); ++index)
    {
        const runtime::AotBasicBlock& left_block = left.blocks[index];
        const runtime::AotBasicBlock& right_block = right.blocks[index];
        if (left_block.guest_address != right_block.guest_address ||
            left_block.instructions.size() !=
                right_block.instructions.size())
        {
            return false;
        }
        for (std::size_t instruction = 0;
             instruction < left_block.instructions.size(); ++instruction)
        {
            if (!SameInstruction(left_block.instructions[instruction],
                                 right_block.instructions[instruction]))
            {
                return false;
            }
        }
    }
    return true;
}

bool SameImage(const runtime::AotCodeCacheImage& left,
               const runtime::AotCodeCacheImage& right)
{
    if (left.valid != right.valid || left.executable != right.executable ||
        left.entry_cache_offset != right.entry_cache_offset ||
        left.bytes != right.bytes ||
        left.address_map.size() != right.address_map.size() ||
        left.fixups.size() != right.fixups.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.address_map.size(); ++index)
    {
        const runtime::AotAddressMapEntry& left_entry = left.address_map[index];
        const runtime::AotAddressMapEntry& right_entry =
            right.address_map[index];
        if (left_entry.guest_address != right_entry.guest_address ||
            left_entry.cache_offset != right_entry.cache_offset ||
            left_entry.guest_length != right_entry.guest_length ||
            left_entry.emitted_length != right_entry.emitted_length)
        {
            return false;
        }
    }
    for (std::size_t index = 0; index < left.fixups.size(); ++index)
    {
        const runtime::AotCodeCacheFixup& left_fixup = left.fixups[index];
        const runtime::AotCodeCacheFixup& right_fixup = right.fixups[index];
        if (left_fixup.kind != right_fixup.kind ||
            left_fixup.guest_source != right_fixup.guest_source ||
            left_fixup.guest_target != right_fixup.guest_target ||
            left_fixup.cache_patch_offset !=
                right_fixup.cache_patch_offset ||
            left_fixup.resolved != right_fixup.resolved)
        {
            return false;
        }
    }
    return true;
}

// Builds both representations over the same live bytes and requires identical
// plans and identical emitted images.
bool AgreesAtEntry(std::uint32_t base,
                   std::uint32_t size,
                   const std::uint8_t* live,
                   std::uint32_t entry,
                   runtime::AotTranslationPlan* view_plan)
{
    const runtime::RelocatedRuntimeImage owning =
        MakeImage(base, size, live, false);
    const runtime::RelocatedRuntimeImage view =
        MakeImage(base, size, live, true);

    runtime::AotTranslationPlan owning_plan;
    runtime::AotTranslationPlan built_view_plan;
    const bool owning_built = runtime::BuildAotTranslationPlanFromEntry(
        owning, entry, &owning_plan);
    const bool view_built = runtime::BuildAotTranslationPlanFromEntry(
        view, entry, &built_view_plan);
    if (owning_built != view_built ||
        !SamePlan(owning_plan, built_view_plan))
    {
        return false;
    }
    if (view_plan != nullptr)
    {
        *view_plan = built_view_plan;
    }
    if (!owning_built)
    {
        return true;
    }

    runtime::AotCodeCacheImage owning_image;
    runtime::AotCodeCacheImage view_image;
    const bool owning_emitted =
        runtime::BuildAotCodeCacheImage(owning_plan, &owning_image);
    const bool view_emitted =
        runtime::BuildAotCodeCacheImage(built_view_plan, &view_image);
    return owning_emitted == view_emitted &&
        SameImage(owning_image, view_image);
}

#endif  // defined(_WIN32)
}  // namespace

bool RunArenaViewProbe()
{
#if !defined(_WIN32)
    return true;
#else
    auto* guest = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, kRegionBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (guest == nullptr ||
        reinterpret_cast<std::uintptr_t>(guest) >
            std::numeric_limits<std::uint32_t>::max())
    {
        if (guest != nullptr)
        {
            VirtualFree(guest, 0, MEM_RELEASE);
        }
        std::cout << "arena_view_all=false\n";
        return false;
    }
    const std::uint32_t base =
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(guest));
    WriteGuestProgram(guest);

    // 1. Same live bytes, both representations: plan and emitted image must be
    //    identical, and the CFG must be the intended non-trivial one.
    runtime::AotTranslationPlan first_plan;
    const bool identical_plan =
        AgreesAtEntry(base, kRegionBytes, guest, base, &first_plan);
    const bool exercised = first_plan.valid && first_plan.block_count >= 3U &&
        first_plan.instruction_count >= 6U &&
        first_plan.direct_call_count >= 1U &&
        first_plan.conditional_branch_count >= 1U &&
        first_plan.indirect_exit_count >= 1U &&
        first_plan.outside_image_target_count >= 1U;

    // 2. The view must be live rather than a stale alias: changing a guest byte
    //    must change the plan, and both representations must change together.
    guest[1] = 0x99U;
    runtime::AotTranslationPlan second_plan;
    const bool live_agrees =
        AgreesAtEntry(base, kRegionBytes, guest, base, &second_plan);
    const bool live_visible = identical_plan && live_agrees &&
        !SamePlan(first_plan, second_plan);
    guest[1] = 0x78U;

    // 3. Bounds: an entry within one maximum instruction length of the end has
    //    no readable window, so both must report the same refusal instead of
    //    reading past the object.
    runtime::AotTranslationPlan edge_plan;
    const bool edge_agrees = AgreesAtEntry(
        base, kRegionBytes, guest, base + kRegionBytes - 4U, &edge_plan);
    const bool edge_refused = edge_agrees && !edge_plan.valid &&
        edge_plan.outside_image_target_count == 1U;

    // 4. A target outside the object must stay outside: entering above the
    //    object is refused identically by both.
    runtime::AotTranslationPlan outside_plan;
    const bool outside_agrees = AgreesAtEntry(
        base, kRegionBytes, guest, base + kRegionBytes + 0x100U,
        &outside_plan);
    const bool outside_refused = outside_agrees && !outside_plan.valid;

    // 5. The second reader on the shared accessors must window an external view
    //    the same way it windows an owning object.
    runtime::RelocatedImageByteWindow owning_window;
    runtime::RelocatedImageByteWindow view_window;
    const bool owning_windowed = runtime::BuildRelocatedImageByteWindow(
        MakeImage(base, kRegionBytes, guest, false), base + 0x40U, 8U, 8U,
        &owning_window);
    const bool view_windowed = runtime::BuildRelocatedImageByteWindow(
        MakeImage(base, kRegionBytes, guest, true), base + 0x40U, 8U, 8U,
        &view_window);
    const bool window_agrees = owning_windowed == view_windowed &&
        owning_window.valid == view_window.valid &&
        owning_window.window_base == view_window.window_base &&
        owning_window.focus_offset == view_window.focus_offset &&
        owning_window.bytes == view_window.bytes &&
        !owning_window.bytes.empty();

    VirtualFree(guest, 0, MEM_RELEASE);

    const bool all = identical_plan && exercised && live_visible &&
        edge_refused && outside_refused && window_agrees;
    std::cout << "arena_view_identical_plan="
              << (identical_plan ? "true" : "false")
              << "\narena_view_cfg_exercised=" << (exercised ? "true" : "false")
              << "\narena_view_live_visible="
              << (live_visible ? "true" : "false")
              << "\narena_view_edge_refused="
              << (edge_refused ? "true" : "false")
              << "\narena_view_outside_refused="
              << (outside_refused ? "true" : "false")
              << "\narena_view_byte_window=" << (window_agrees ? "true" : "false")
              << "\narena_view_all=" << (all ? "true" : "false") << "\n";
    return all;
#endif
}

}  // namespace repiu::tools
