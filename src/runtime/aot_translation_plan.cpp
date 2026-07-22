#include "repiu/runtime/aot_translation_plan.h"

#include <Zydis.h>

#include <chrono>
#include <cstring>
#include <unordered_map>
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

// Task 264 Phase 3a: a segment-override memory access that the emitter may
// translate natively (guard + base-folded displacement). Returns the overridden
// segment index (0=ES,2=SS,3=DS,4=FS,5=GS). The emitter re-verifies the exact
// form and falls back to a boundary for anything it cannot encode, so this stays
// permissive; it only screens out privileged/CS-override cases up front.
bool IsTranslatableSegmentOverrideMem(const ZydisDecodedInstruction& instruction,
                                      const ZydisDecodedOperand* operands,
                                      std::uint8_t* segment_register)
{
    if (operands == nullptr || segment_register == nullptr ||
        (instruction.attributes & ZYDIS_ATTRIB_HAS_SEGMENT) == 0U ||
        (instruction.attributes & ZYDIS_ATTRIB_IS_PRIVILEGED) != 0U ||
        (instruction.attributes & ZYDIS_ATTRIB_HAS_MODRM) == 0U)
    {
        return false;
    }
    for (std::uint8_t index = 0;
         index < instruction.operand_count_visible; ++index)
    {
        if (operands[index].type != ZYDIS_OPERAND_TYPE_MEMORY)
        {
            continue;
        }
        switch (operands[index].mem.segment)
        {
            case ZYDIS_REGISTER_ES: *segment_register = 0U; return true;
            case ZYDIS_REGISTER_SS: *segment_register = 2U; return true;
            case ZYDIS_REGISTER_DS: *segment_register = 3U; return true;
            case ZYDIS_REGISTER_FS: *segment_register = 4U; return true;
            case ZYDIS_REGISTER_GS: *segment_register = 5U; return true;
            default: return false; // CS override or none
        }
    }
    return false;
}

bool IsHleBoundary(const ZydisDecodedInstruction& instruction,
                   const ZydisDecodedOperand* operands)
{
    // A push of a segment register only reads the selector and writes it to the
    // (flat) stack -- it needs no HLE. The single-step boundary already executes
    // it natively, pushing the host selector, so translating it (kCopy) is
    // behavior-identical and removes the exception round-trip (Task 264 Phase 1).
    // The Task 264 probe confirmed the host segment register is what the
    // single-step path pushes; matching it exactly avoids any behavior change.
    if (instruction.mnemonic == ZYDIS_MNEMONIC_PUSH && operands != nullptr)
    {
        for (std::uint8_t index = 0;
             index < instruction.operand_count_visible; ++index)
        {
            if (operands[index].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                ZydisRegisterGetClass(operands[index].reg.value) ==
                    ZYDIS_REGCLASS_SEGMENT)
            {
                return false;
            }
        }
    }
    if ((instruction.attributes &
         (ZYDIS_ATTRIB_IS_PRIVILEGED | ZYDIS_ATTRIB_HAS_SEGMENT)) != 0U)
    {
        return true;
    }
    // NOTE (Task 264 Phase 2, reverted): translating `mov r32,Sreg` natively
    // (kCopy) regressed -- the guest stalled at the fatal-breakpoint idiom
    // 0x030F3438 with no rendering. Unlike `push seg` (proven native/host-value
    // by probe), the single-step path for a segment-register store returns the
    // *shadow* selector, so a native store of the host selector (0x2b) diverges.
    // Segment-register stores therefore stay HLE boundaries.
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

// Watcom emits switch statements as `cmp reg, imm` + `ja default` followed
// by `jmp dword ptr cs:[reg*4 + table]`. The guard records the fallthrough
// address and bound so the table branch can be translated natively.
struct JumpTableGuard
{
    std::uint8_t index_register = 0xFFU;
    std::uint32_t entry_count = 0;
};

// The emitted slot is `jmp [reg*4+disp32]` + INT3 + N dword entries and the
// address map stores its emitted length in one byte, so N is capped at 61.
constexpr std::uint32_t kMaximumJumpTableEntries = 61U;

bool ReadGuardRegisterId(ZydisRegister reg, std::uint8_t* register_id)
{
    if (register_id == nullptr ||
        ZydisRegisterGetClass(reg) != ZYDIS_REGCLASS_GPR32)
    {
        return false;
    }
    const std::int8_t id = ZydisRegisterGetId(reg);
    if (id < 0 || id > 7 || id == 4)
    {
        return false;
    }
    *register_id = static_cast<std::uint8_t>(id);
    return true;
}

bool ReadJumpTableGuard(const ZydisDecoder& decoder,
                        const AotInstructionRecord& compare,
                        JumpTableGuard* guard)
{
    if (guard == nullptr || compare.kind != AotInstructionKind::kCopy ||
        static_cast<ZydisMnemonic>(compare.mnemonic) != ZYDIS_MNEMONIC_CMP)
    {
        return false;
    }
    ZydisDecodedInstruction instruction{};
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
            &decoder, compare.bytes.data(), compare.bytes.size(),
            &instruction, operands)) ||
        instruction.operand_count_visible < 2U ||
        operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER ||
        operands[1].type != ZYDIS_OPERAND_TYPE_IMMEDIATE)
    {
        return false;
    }
    const std::int64_t bound = operands[1].imm.is_signed
        ? operands[1].imm.value.s
        : static_cast<std::int64_t>(operands[1].imm.value.u);
    if (bound < 0 ||
        bound + 1 > static_cast<std::int64_t>(kMaximumJumpTableEntries))
    {
        return false;
    }
    if (!ReadGuardRegisterId(operands[0].reg.value, &guard->index_register))
    {
        return false;
    }
    guard->entry_count = static_cast<std::uint32_t>(bound) + 1U;
    return true;
}

bool MatchJumpTableBranch(const ZydisDecodedInstruction& instruction,
                          const ZydisDecodedOperand* operands,
                          std::uint8_t* register_id,
                          std::uint32_t* table_address)
{
    if (operands == nullptr || register_id == nullptr ||
        table_address == nullptr ||
        instruction.mnemonic != ZYDIS_MNEMONIC_JMP ||
        instruction.meta.branch_type == ZYDIS_BRANCH_TYPE_FAR ||
        instruction.operand_count_visible == 0U ||
        operands[0].type != ZYDIS_OPERAND_TYPE_MEMORY)
    {
        return false;
    }
    const auto& memory = operands[0].mem;
    if (memory.base != ZYDIS_REGISTER_NONE || memory.scale != 4U ||
        !memory.disp.has_displacement || memory.disp.value <= 0 ||
        memory.disp.value > static_cast<std::int64_t>(UINT32_MAX))
    {
        return false;
    }
    if (memory.segment != ZYDIS_REGISTER_CS &&
        memory.segment != ZYDIS_REGISTER_DS)
    {
        return false;
    }
    if (!ReadGuardRegisterId(memory.index, register_id))
    {
        return false;
    }
    *table_address = static_cast<std::uint32_t>(memory.disp.value);
    return true;
}

bool ReadJumpTableTargets(const RelocatedRuntimeImage& image,
                          std::uint32_t table_address,
                          std::uint32_t entry_count,
                          std::vector<std::uint32_t>* targets)
{
    if (targets == nullptr || entry_count == 0U ||
        entry_count > kMaximumJumpTableEntries ||
        static_cast<std::uint64_t>(table_address) + entry_count * 4ULL >
            UINT32_MAX)
    {
        return false;
    }
    targets->clear();
    targets->reserve(entry_count);
    for (std::uint32_t index = 0; index < entry_count; ++index)
    {
        const std::uint8_t* entry_bytes =
            FindBytes(image, table_address + index * 4U, 4U);
        if (entry_bytes == nullptr)
        {
            return false;
        }
        std::uint32_t target = 0;
        std::memcpy(&target, entry_bytes, sizeof(target));
        if (FindBytes(image, target, 1U) == nullptr)
        {
            return false;
        }
        targets->push_back(target);
    }
    return true;
}

// A guarded table branch may be visited before its cmp/ja guard when the
// walk reaches the fallthrough first (e.g. the plan entry is the branch).
// The sweep re-decodes such records once the guard is known.
bool TryReclassifyJumpTable(
    const RelocatedRuntimeImage& image,
    const ZydisDecoder& decoder,
    const std::unordered_map<std::uint32_t, JumpTableGuard>& guards,
    AotInstructionRecord* record)
{
    if (record == nullptr ||
        (record->kind != AotInstructionKind::kHleBoundary &&
         record->kind != AotInstructionKind::kIndirectExit) ||
        record->bytes.empty())
    {
        return false;
    }
    const auto guard = guards.find(record->guest_address);
    if (guard == guards.end())
    {
        return false;
    }
    ZydisDecodedInstruction instruction{};
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
            &decoder, record->bytes.data(), record->bytes.size(),
            &instruction, operands)))
    {
        return false;
    }
    std::uint8_t branch_register = 0xFFU;
    std::uint32_t table_address = 0;
    if (!MatchJumpTableBranch(instruction, operands,
                              &branch_register, &table_address) ||
        branch_register != guard->second.index_register ||
        !ReadJumpTableTargets(image, table_address,
                              guard->second.entry_count,
                              &record->table_targets))
    {
        return false;
    }
    record->table_index_register = branch_register;
    return true;
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
    std::unordered_map<std::uint32_t, JumpTableGuard> jump_table_guards;
    bool sweep_jump_table_guards = true;
    while (sweep_jump_table_guards)
    {
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
                const auto guard = jump_table_guards.find(address);
                if (guard != jump_table_guards.end())
                {
                    std::uint8_t branch_register = 0xFFU;
                    std::uint32_t table_address = 0;
                    if (MatchJumpTableBranch(instruction, operands,
                                             &branch_register, &table_address) &&
                        branch_register == guard->second.index_register &&
                        ReadJumpTableTargets(image, table_address,
                                             guard->second.entry_count,
                                             &record.table_targets))
                    {
                        record.kind = AotInstructionKind::kJumpTable;
                        record.table_index_register = branch_register;
                        ++plan->jump_table_count;
                        plan->jump_table_target_count += static_cast<std::uint32_t>(
                            record.table_targets.size());
                        plan->estimated_emitted_bytes +=
                            8U + 4U * record.table_targets.size();
                        for (const std::uint32_t target : record.table_targets)
                        {
                            pending.push_back(target);
                        }
                        block.instructions.push_back(std::move(record));
                        break;
                    }
                }
                std::uint8_t segment_override_register = 0xFFU;
                if (IsTranslatableSegmentOverrideMem(
                        instruction, operands, &segment_override_register))
                {
                    // Translated natively when the emitter can re-encode it;
                    // otherwise the emitter falls back to a boundary. Ends the
                    // block like a boundary so the emitted sequence carries its
                    // own fallthrough jump (Task 264 Phase 3a).
                    record.kind = AotInstructionKind::kSegmentOverrideMem;
                    record.segment_override_register = segment_override_register;
                    record.fallthrough_target = next;
                    block.instructions.push_back(std::move(record));
                    ++plan->hle_boundary_count;
                    plan->estimated_emitted_bytes += 48U;
                    pending.push_back(next);
                    break;
                }
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
                        if (instruction.mnemonic == ZYDIS_MNEMONIC_JNBE &&
                            !block.instructions.empty())
                        {
                            JumpTableGuard table_guard;
                            if (ReadJumpTableGuard(decoder,
                                                   block.instructions.back(),
                                                   &table_guard))
                            {
                                jump_table_guards.emplace(next, table_guard);
                            }
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
        sweep_jump_table_guards = false;
        if (plan->instruction_count < kMaximumInstructions)
        {
            for (AotBasicBlock& swept_block : plan->blocks)
            {
                for (AotInstructionRecord& swept_record :
                     swept_block.instructions)
                {
                    if (!TryReclassifyJumpTable(image, decoder,
                                                jump_table_guards,
                                                &swept_record))
                    {
                        continue;
                    }
                    if (swept_record.kind == AotInstructionKind::kHleBoundary)
                    {
                        --plan->hle_boundary_count;
                    }
                    else
                    {
                        --plan->indirect_exit_count;
                    }
                    swept_record.kind = AotInstructionKind::kJumpTable;
                    ++plan->jump_table_count;
                    plan->jump_table_target_count +=
                        static_cast<std::uint32_t>(
                            swept_record.table_targets.size());
                    plan->estimated_emitted_bytes +=
                        8U + 4U * swept_record.table_targets.size();
                    for (const std::uint32_t target :
                         swept_record.table_targets)
                    {
                        pending.push_back(target);
                    }
                    sweep_jump_table_guards = true;
                }
            }
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
