#include "jump_table_guard_probe.h"

#include "repiu/runtime/aot_code_cache.h"
#include "repiu/runtime/aot_translation_plan.h"
#include "repiu/runtime/runtime_memory.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

namespace repiu::tools
{
namespace
{

constexpr std::uint32_t kImageBase = 0x00100000U;
constexpr std::uint32_t kTableOffset = 0x100U;
constexpr std::uint32_t kJumpOffset = 11U;

runtime::RelocatedRuntimeImage MakeImage(
    const std::vector<std::uint8_t>& prefix)
{
    runtime::RelocatedRuntimeImage image;
    image.valid = true;
    image.relocated_image_base = kImageBase;
    image.relocated_entry_linear_address = kImageBase;
    runtime::RelocatedRuntimeObject object;
    object.relocated_base_address = kImageBase;
    object.virtual_size = 0x200U;
    object.memory.resize(object.virtual_size, 0x90U);
    std::memcpy(object.memory.data(), prefix.data(), prefix.size());
    object.memory[0x40U] = 0xC3U;
    for (std::uint32_t index = 0; index < 10U; ++index)
    {
        const std::uint32_t target = kImageBase + 0x40U;
        std::memcpy(object.memory.data() + kTableOffset + index * 4U,
                    &target, sizeof(target));
    }
    image.objects.push_back(std::move(object));
    return image;
}

std::vector<std::uint8_t> MakeByteGuardPrefix(
    const std::vector<std::uint8_t>& normalization,
    std::uint8_t compare_modrm = 0xF9U)
{
    std::vector<std::uint8_t> bytes = {
        0x80U, compare_modrm, 0x09U,
        0x77U, 0x39U};
    bytes.insert(bytes.end(), normalization.begin(), normalization.end());
    const std::uint8_t branch[] = {
        0x2EU, 0xFFU, 0x24U, 0x8DU,
        0U, 0U, 0U, 0U};
    bytes.insert(bytes.end(), branch, branch + sizeof(branch));
    const std::uint32_t table = kImageBase + kTableOffset;
    std::memcpy(bytes.data() + bytes.size() - sizeof(table),
                &table, sizeof(table));
    return bytes;
}

bool HasJumpTableAt(const runtime::AotTranslationPlan& plan,
                    std::uint32_t address)
{
    for (const runtime::AotBasicBlock& block : plan.blocks)
    {
        for (const runtime::AotInstructionRecord& instruction :
             block.instructions)
        {
            if (instruction.guest_address == address)
            {
                return instruction.kind ==
                        runtime::AotInstructionKind::kJumpTable &&
                    instruction.table_targets.size() == 10U;
            }
        }
    }
    return false;
}

bool PlansAsJumpTable(const std::vector<std::uint8_t>& prefix,
                      std::uint32_t jump_offset,
                      bool build_cache)
{
    const runtime::RelocatedRuntimeImage image = MakeImage(prefix);
    runtime::AotTranslationPlan plan;
    if (!runtime::BuildAotTranslationPlanFromEntry(
            image, image.relocated_entry_linear_address, &plan))
    {
        return false;
    }
    const bool matched = plan.jump_table_count == 1U &&
        plan.jump_table_target_count == 10U &&
        HasJumpTableAt(plan, kImageBase + jump_offset);
    if (!matched || !build_cache)
    {
        return matched;
    }
    runtime::AotCodeCacheImage cache;
    return runtime::BuildAotCodeCacheImage(plan, &cache) && cache.valid &&
        cache.jump_table_sites.size() == 1U;
}

}  // namespace

bool RunJumpTableGuardProbe()
{
    const std::vector<std::uint8_t> byte_guard = MakeByteGuardPrefix({
        0x81U, 0xE1U, 0xFFU, 0x00U, 0x00U, 0x00U});
    const bool byte_guard_supported = PlansAsJumpTable(
        byte_guard, kJumpOffset, true);

    const bool wrong_mask_rejected = !PlansAsJumpTable(
        MakeByteGuardPrefix({
            0x81U, 0xE1U, 0xFFU, 0xFFU, 0x00U, 0x00U}),
        kJumpOffset, false);
    const bool wrong_register_rejected = !PlansAsJumpTable(
        MakeByteGuardPrefix({
            0x81U, 0xE2U, 0xFFU, 0x00U, 0x00U, 0x00U}),
        kJumpOffset, false);
    const bool high_byte_rejected = !PlansAsJumpTable(
        MakeByteGuardPrefix({
            0x81U, 0xE1U, 0xFFU, 0x00U, 0x00U, 0x00U}, 0xFDU),
        kJumpOffset, false);
    const std::vector<std::uint8_t> no_normalization = MakeByteGuardPrefix({});
    const bool missing_normalization_rejected = !PlansAsJumpTable(
        no_normalization, 5U, false);

    std::vector<std::uint8_t> dword_guard = {
        0x83U, 0xF9U, 0x09U,
        0x77U, 0x39U,
        0x2EU, 0xFFU, 0x24U, 0x8DU,
        0U, 0U, 0U, 0U};
    const std::uint32_t table = kImageBase + kTableOffset;
    std::memcpy(dword_guard.data() + dword_guard.size() - sizeof(table),
                &table, sizeof(table));
    const bool dword_guard_preserved = PlansAsJumpTable(
        dword_guard, 5U, true);

    const bool all = byte_guard_supported && wrong_mask_rejected &&
        wrong_register_rejected && high_byte_rejected &&
        missing_normalization_rejected && dword_guard_preserved;
    std::cout << "jump_table_byte_guard_supported="
              << (byte_guard_supported ? "true" : "false")
              << "\njump_table_wrong_mask_rejected="
              << (wrong_mask_rejected ? "true" : "false")
              << "\njump_table_wrong_register_rejected="
              << (wrong_register_rejected ? "true" : "false")
              << "\njump_table_high_byte_rejected="
              << (high_byte_rejected ? "true" : "false")
              << "\njump_table_missing_normalization_rejected="
              << (missing_normalization_rejected ? "true" : "false")
              << "\njump_table_dword_guard_preserved="
              << (dword_guard_preserved ? "true" : "false")
              << "\njump_table_guard_all=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
