#include "repiu/runtime/aot_translation_plan.h"

#include <Zydis.h>

#include <chrono>
#include <unordered_set>
#include <utility>
#include <vector>

namespace repiu::runtime
{
namespace
{

const std::uint8_t* FindBytes(const RelocatedRuntimeImage& image,
                              std::uint32_t address,
                              std::uint32_t bytes)
{
    for (const RelocatedRuntimeObject& object : image.objects)
    {
        if (address < object.relocated_base_address)
        {
            continue;
        }
        const std::uint64_t offset =
            static_cast<std::uint64_t>(address) - object.relocated_base_address;
        if (offset + bytes <= object.memory.size())
        {
            return object.memory.data() + offset;
        }
    }
    return nullptr;
}

bool ReadDirectTarget(const ZydisDecodedInstruction& instruction,
                      const ZydisDecodedOperand* operands,
                      std::uint32_t address,
                      std::uint32_t* target)
{
    if (operands == nullptr || target == nullptr ||
        instruction.operand_count_visible == 0U ||
        operands[0].type != ZYDIS_OPERAND_TYPE_IMMEDIATE ||
        !operands[0].imm.is_relative ||
        instruction.meta.branch_type == ZYDIS_BRANCH_TYPE_FAR)
    {
        return false;
    }
    ZyanU64 absolute = 0;
    if (!ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(
            &instruction, &operands[0], address, &absolute)) ||
        absolute > UINT32_MAX)
    {
        return false;
    }
    *target = static_cast<std::uint32_t>(absolute);
    return true;
}

bool IsHleBoundary(const ZydisDecodedInstruction& instruction,
                   const ZydisDecodedOperand* operands)
{
    if ((instruction.attributes &
         (ZYDIS_ATTRIB_IS_PRIVILEGED | ZYDIS_ATTRIB_HAS_SEGMENT)) != 0U)
    {
        return true;
    }
    if (operands != nullptr)
    {
        for (std::uint8_t index = 0;
             index < instruction.operand_count_visible; ++index)
        {
            if (operands[index].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                ZydisRegisterGetClass(operands[index].reg.value) ==
                    ZYDIS_REGCLASS_SEGMENT)
            {
                return true;
            }
        }
    }
    switch (instruction.meta.category)
    {
        case ZYDIS_CATEGORY_INTERRUPT:
        case ZYDIS_CATEGORY_IO:
        case ZYDIS_CATEGORY_IOSTRINGOP:
        case ZYDIS_CATEGORY_RDWRFSGS:
        case ZYDIS_CATEGORY_SEGOP:
        case ZYDIS_CATEGORY_SYSCALL:
        case ZYDIS_CATEGORY_SYSRET:
        case ZYDIS_CATEGORY_SYSTEM:
        case ZYDIS_CATEGORY_UINTR:
            return true;
        default:
            return false;
    }
}

bool IsExcludedGuestAddress(
    const std::vector<AotExcludedGuestRange>& ranges,
    std::uint32_t address)
{
    for (const AotExcludedGuestRange& range : ranges)
    {
        const std::uint64_t end =
            static_cast<std::uint64_t>(range.guest_address) +
            range.byte_count;
        if (range.byte_count != 0U &&
            address >= range.guest_address && address < end)
        {
            return true;
        }
    }
    return false;
}

void AppendExcludedBoundary(std::uint32_t address,
                            AotTranslationPlan* plan,
                            AotBasicBlock* block)
{
    AotInstructionRecord record;
    record.guest_address = address;
    record.kind = AotInstructionKind::kHleBoundary;
    record.length = 1U;
    block->instructions.push_back(std::move(record));
    ++plan->instruction_count;
    ++plan->hle_boundary_count;
    ++plan->source_code_bytes;
    ++plan->estimated_emitted_bytes;
}

}  // namespace

bool BuildAotTranslationPlanFromEntry(const RelocatedRuntimeImage& image,
                                      std::uint32_t entry_address,
                                      const std::vector<
                                          AotExcludedGuestRange>&
                                          excluded_ranges,
                                      AotTranslationPlan* plan)
{
    constexpr std::uint32_t kMaximumInstructions = 2'000'000U;
    constexpr std::uint32_t kMaximumBlockInstructions = 65'536U;
    if (plan == nullptr || !image.valid)
    {
        return false;
    }
    *plan = AotTranslationPlan{};
    plan->entry_address = entry_address;
    const auto started = std::chrono::steady_clock::now();
    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(
            &decoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32)))
    {
        plan->message = "failed to initialize Zydis legacy-32 decoder";
        return false;
    }
    std::vector<std::uint32_t> pending{plan->entry_address};
    std::unordered_set<std::uint32_t> visited_blocks;
    std::unordered_set<std::uint32_t> visited_instructions;
    while (!pending.empty() && plan->instruction_count < kMaximumInstructions)
    {
        const std::uint32_t block_entry = pending.back();
        pending.pop_back();
        if (!visited_blocks.insert(block_entry).second)
        {
            continue;
        }
        if (IsExcludedGuestAddress(excluded_ranges, block_entry))
        {
            ++plan->block_count;
            plan->blocks.push_back(AotBasicBlock{});
            AotBasicBlock& block = plan->blocks.back();
            block.guest_address = block_entry;
            AppendExcludedBoundary(block_entry, plan, &block);
            continue;
        }
        if (FindBytes(image, block_entry, 1U) == nullptr)
        {
            ++plan->outside_image_target_count;
            continue;
        }
        ++plan->block_count;
        plan->blocks.push_back(AotBasicBlock{});
        AotBasicBlock& block = plan->blocks.back();
        block.guest_address = block_entry;
        std::uint32_t address = block_entry;
        for (std::uint32_t block_instruction = 0;
             block_instruction < kMaximumBlockInstructions;
             ++block_instruction)
        {
            if (IsExcludedGuestAddress(excluded_ranges, address))
            {
                AppendExcludedBoundary(address, plan, &block);
                break;
            }
            if (!visited_instructions.insert(address).second)
            {
                break;
            }
            const std::uint8_t* bytes = FindBytes(
                image, address, ZYDIS_MAX_INSTRUCTION_LENGTH);
            if (bytes == nullptr)
            {
                ++plan->outside_image_target_count;
                break;
            }
            ZydisDecodedInstruction instruction{};
            ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
            if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
                    &decoder, bytes, ZYDIS_MAX_INSTRUCTION_LENGTH,
                    &instruction, operands)) || instruction.length == 0U)
            {
                ++plan->decode_failure_count;
                break;
            }
            ++plan->instruction_count;
            plan->source_code_bytes += instruction.length;
            plan->estimated_emitted_bytes += instruction.length;
            const std::uint32_t next = address + instruction.length;
            AotInstructionRecord record;
            record.guest_address = address;
            record.length = instruction.length;
            record.mnemonic = static_cast<std::uint16_t>(instruction.mnemonic);
            record.bytes.assign(bytes, bytes + instruction.length);
            if (IsHleBoundary(instruction, operands))
            {
                record.kind = AotInstructionKind::kHleBoundary;
                block.instructions.push_back(std::move(record));
                ++plan->hle_boundary_count;
                plan->estimated_emitted_bytes += 14U;
                pending.push_back(next);
                break;
            }
            const auto category = instruction.meta.category;
            if (category == ZYDIS_CATEGORY_RET)
            {
                record.kind = AotInstructionKind::kReturn;
                block.instructions.push_back(std::move(record));
                ++plan->return_count;
                break;
            }
            if (category == ZYDIS_CATEGORY_CALL ||
                category == ZYDIS_CATEGORY_COND_BR ||
                category == ZYDIS_CATEGORY_UNCOND_BR)
            {
                std::uint32_t target = 0;
                if (!ReadDirectTarget(
                        instruction, operands, address, &target))
                {
                    record.kind = AotInstructionKind::kIndirectExit;
                    block.instructions.push_back(std::move(record));
                    ++plan->indirect_exit_count;
                    break;
                }
                record.direct_target = target;
                record.fallthrough_target = next;
                if (category == ZYDIS_CATEGORY_CALL)
                {
                    record.kind = AotInstructionKind::kDirectCall;
                    ++plan->direct_call_count;
                    if (instruction.length < 5U)
                    {
                        plan->estimated_emitted_bytes +=
                            5U - instruction.length;
                    }
                    pending.push_back(target);
                    pending.push_back(next);
                }
                else if (category == ZYDIS_CATEGORY_COND_BR)
                {
                    record.kind = AotInstructionKind::kConditionalBranch;
                    ++plan->conditional_branch_count;
                    if (instruction.length < 6U)
                    {
                        plan->estimated_emitted_bytes += 6U - instruction.length;
                    }
                    pending.push_back(target);
                    pending.push_back(next);
                }
                else
                {
                    record.kind = AotInstructionKind::kDirectJump;
                    ++plan->direct_jump_count;
                    if (instruction.length < 5U)
                    {
                        plan->estimated_emitted_bytes += 5U - instruction.length;
                    }
                    pending.push_back(target);
                }
                block.instructions.push_back(std::move(record));
                break;
            }
            record.kind = AotInstructionKind::kCopy;
            block.instructions.push_back(std::move(record));
            ++plan->copy_instruction_count;
            address = next;
        }
    }
    if (!pending.empty())
    {
        ++plan->analysis_limit_count;
    }
    plan->elapsed_microseconds = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count());
    plan->valid = plan->instruction_count != 0U;
    plan->message = plan->valid
        ? "AOT translation plan is ready"
        : "AOT translation plan contains no reachable instructions";
    return plan->valid;
}

bool BuildAotTranslationPlanFromEntry(const RelocatedRuntimeImage& image,
                                      std::uint32_t entry_address,
                                      AotTranslationPlan* plan)
{
    return BuildAotTranslationPlanFromEntry(
        image, entry_address, {}, plan);
}

bool BuildAotTranslationPlan(const RelocatedRuntimeImage& image,
                             AotTranslationPlan* plan)
{
    return BuildAotTranslationPlanFromEntry(
        image, image.relocated_entry_linear_address, plan);
}

}  // namespace repiu::runtime
