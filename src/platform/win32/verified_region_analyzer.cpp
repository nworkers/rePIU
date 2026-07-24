#include "verified_region_analyzer.h"

#include <Zydis.h>

#include <array>
#include <cstring>
#include <unordered_set>
#include <vector>

namespace repiu::platform::win32::detail
{
namespace
{

bool IsRuntimeRange(std::uint32_t address,
                    std::uint32_t byte_count,
                    std::uint32_t runtime_base,
                    std::uint32_t runtime_size)
{
    const std::uint32_t end = address + byte_count;
    const std::uint32_t runtime_end = runtime_base + runtime_size;
    return end >= address && runtime_end >= runtime_base &&
           address >= runtime_base && end <= runtime_end;
}

void RecordFailure(std::uint32_t address, VerifiedRegionFailure* failure)
{
    if (failure == nullptr)
    {
        return;
    }
    failure->instruction = address;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(address));
    failure->opcode = bytes[0];
    std::memcpy(&failure->bytes_low, bytes, sizeof(failure->bytes_low));
    std::memcpy(&failure->bytes_high,
                bytes + sizeof(failure->bytes_low),
                sizeof(failure->bytes_high));
}

bool IsSensitive(const ZydisDecodedInstruction& instruction)
{
    if ((instruction.attributes &
         (ZYDIS_ATTRIB_IS_PRIVILEGED | ZYDIS_ATTRIB_HAS_SEGMENT)) != 0)
    {
        return true;
    }
    switch (instruction.meta.category)
    {
    case ZYDIS_CATEGORY_INTERRUPT:
    case ZYDIS_CATEGORY_IO:
    case ZYDIS_CATEGORY_IOSTRINGOP:
    case ZYDIS_CATEGORY_RDWRFSGS:
    case ZYDIS_CATEGORY_SEGOP:
    case ZYDIS_CATEGORY_STRINGOP:
    case ZYDIS_CATEGORY_SYSCALL:
    case ZYDIS_CATEGORY_SYSRET:
    case ZYDIS_CATEGORY_SYSTEM:
    case ZYDIS_CATEGORY_UINTR:
        return true;
    default:
        return false;
    }
}

bool HasExplicitMemoryWrite(const ZydisDecodedInstruction& instruction,
                            const ZydisDecodedOperand* operands)
{
    if (operands == nullptr)
    {
        return false;
    }
    for (std::uint8_t index = 0;
         index < instruction.operand_count_visible; ++index)
    {
        if (operands[index].type == ZYDIS_OPERAND_TYPE_MEMORY &&
            (operands[index].actions &
             ZYDIS_OPERAND_ACTION_MASK_WRITE) != 0U)
        {
            return true;
        }
    }
    return false;
}

bool MemoryWriteUsesModifiedAddressRegister(
    const ZydisDecodedInstruction& instruction,
    const ZydisDecodedOperand* operands,
    const std::array<bool, ZYDIS_REGISTER_MAX_VALUE + 1>& modified)
{
    for (std::uint8_t index = 0;
         index < instruction.operand_count_visible; ++index)
    {
        if (operands[index].type != ZYDIS_OPERAND_TYPE_MEMORY ||
            (operands[index].actions &
             ZYDIS_OPERAND_ACTION_MASK_WRITE) == 0U)
        {
            continue;
        }
        const ZydisRegister base = ZydisRegisterGetLargestEnclosing(
            ZYDIS_MACHINE_MODE_LEGACY_32, operands[index].mem.base);
        const ZydisRegister index_register =
            ZydisRegisterGetLargestEnclosing(
                ZYDIS_MACHINE_MODE_LEGACY_32,
                operands[index].mem.index);
        if ((base != ZYDIS_REGISTER_NONE && modified[base]) ||
            (index_register != ZYDIS_REGISTER_NONE &&
             modified[index_register]))
        {
            return true;
        }
    }
    return false;
}

bool IsMemoryWriteTargetAllowed(
    const ZydisDecodedInstruction& instruction,
    const ZydisDecodedOperand* operands,
    const NativeLinearSpanOptions& options)
{
    if (options.register_query == nullptr ||
        options.write_target_query == nullptr)
    {
        return false;
    }
    for (std::uint8_t operand_index = 0;
         operand_index < instruction.operand_count_visible;
         ++operand_index)
    {
        const ZydisDecodedOperand& operand = operands[operand_index];
        if (operand.type != ZYDIS_OPERAND_TYPE_MEMORY ||
            (operand.actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) == 0U)
        {
            continue;
        }
        std::uint32_t base = 0;
        std::uint32_t index = 0;
        if ((operand.mem.base != ZYDIS_REGISTER_NONE &&
             !options.register_query(
                 options.write_guard_context,
                 static_cast<std::uint32_t>(
                     ZydisRegisterGetLargestEnclosing(
                         ZYDIS_MACHINE_MODE_LEGACY_32,
                         operand.mem.base)),
                 &base)) ||
            (operand.mem.index != ZYDIS_REGISTER_NONE &&
             !options.register_query(
                 options.write_guard_context,
                 static_cast<std::uint32_t>(
                     ZydisRegisterGetLargestEnclosing(
                         ZYDIS_MACHINE_MODE_LEGACY_32,
                         operand.mem.index)),
                 &index)))
        {
            return false;
        }
        const std::uint32_t displacement =
            operand.mem.disp.has_displacement
            ? static_cast<std::uint32_t>(operand.mem.disp.value)
            : 0U;
        const std::uint32_t address =
            base + index * operand.mem.scale + displacement;
        const std::uint32_t byte_count =
            (static_cast<std::uint32_t>(operand.size) + 7U) / 8U;
        if (byte_count == 0U ||
            !options.write_target_query(
                options.write_guard_context, address, byte_count))
        {
            return false;
        }
    }
    return true;
}

void RecordModifiedRegisters(
    const ZydisDecodedInstruction& instruction,
    const ZydisDecodedOperand* operands,
    std::array<bool, ZYDIS_REGISTER_MAX_VALUE + 1>* modified)
{
    for (std::uint8_t index = 0;
         index < instruction.operand_count; ++index)
    {
        if (operands[index].type != ZYDIS_OPERAND_TYPE_REGISTER ||
            (operands[index].actions &
             ZYDIS_OPERAND_ACTION_MASK_WRITE) == 0U)
        {
            continue;
        }
        const ZydisRegister enclosing = ZydisRegisterGetLargestEnclosing(
            ZYDIS_MACHINE_MODE_LEGACY_32, operands[index].reg.value);
        if (enclosing != ZYDIS_REGISTER_NONE)
        {
            (*modified)[enclosing] = true;
        }
    }
}

bool ReadDirectTarget(const ZydisDecodedInstruction& instruction,
                      const ZydisDecodedOperand* operands,
                      std::uint32_t address,
                      std::uint32_t* target)
{
    if (operands == nullptr || target == nullptr ||
        instruction.operand_count_visible == 0 ||
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

bool VerifyFunction(std::uint32_t entry,
                    std::uint32_t runtime_base,
                    std::uint32_t runtime_size,
                    std::unordered_map<std::uint32_t, std::int8_t>* cache,
                    VerifiedRegionFailure* failure,
                    std::uint32_t depth)
{
    constexpr std::uint32_t kMaxDepth = 16;
    constexpr std::size_t kMaxInstructions = 16384;
    if (cache == nullptr || depth > kMaxDepth ||
        !IsRuntimeRange(entry, 1U, runtime_base, runtime_size))
    {
        return false;
    }
    const auto cached = cache->find(entry);
    if (cached != cache->end())
    {
        return cached->second == 1;
    }
    (*cache)[entry] = 2;

    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(
            &decoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32)))
    {
        (*cache)[entry] = -1;
        return false;
    }
    std::vector<std::uint32_t> pending{entry};
    std::unordered_set<std::uint32_t> visited;
    bool has_return = false;
    while (!pending.empty() && visited.size() < kMaxInstructions)
    {
        const std::uint32_t address = pending.back();
        pending.pop_back();
        if (!visited.insert(address).second)
        {
            continue;
        }
        if (!IsRuntimeRange(address,
                            ZYDIS_MAX_INSTRUCTION_LENGTH,
                            runtime_base,
                            runtime_size))
        {
            RecordFailure(address, failure);
            (*cache)[entry] = -1;
            return false;
        }
        ZydisDecodedInstruction instruction{};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
        const auto* bytes = reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(address));
        if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
                &decoder,
                bytes,
                ZYDIS_MAX_INSTRUCTION_LENGTH,
                &instruction,
                operands)) ||
            instruction.length == 0 || IsSensitive(instruction))
        {
            RecordFailure(address, failure);
            (*cache)[entry] = -1;
            return false;
        }
        const std::uint32_t next = address + instruction.length;
        const auto category = instruction.meta.category;
        if (category == ZYDIS_CATEGORY_RET)
        {
            has_return = true;
            continue;
        }
        if (category == ZYDIS_CATEGORY_CALL ||
            category == ZYDIS_CATEGORY_COND_BR ||
            category == ZYDIS_CATEGORY_UNCOND_BR)
        {
            std::uint32_t target = 0;
            if (!ReadDirectTarget(instruction, operands, address, &target) ||
                !IsRuntimeRange(target, 1U, runtime_base, runtime_size))
            {
                RecordFailure(address, failure);
                (*cache)[entry] = -1;
                return false;
            }
            if (category == ZYDIS_CATEGORY_CALL)
            {
                if (!VerifyFunction(target,
                                    runtime_base,
                                    runtime_size,
                                    cache,
                                    failure,
                                    depth + 1U))
                {
                    (*cache)[entry] = -1;
                    return false;
                }
                pending.push_back(next);
            }
            else if (category == ZYDIS_CATEGORY_COND_BR)
            {
                pending.push_back(target);
                pending.push_back(next);
            }
            else
            {
                pending.push_back(target);
            }
            continue;
        }
        pending.push_back(next);
    }
    const bool safe = has_return && pending.empty();
    (*cache)[entry] = safe ? 1 : -1;
    return safe;
}

}  // namespace

bool VerifyNativeFunctionWithZydis(
    std::uint32_t entry,
    std::uint32_t runtime_base,
    std::uint32_t runtime_size,
    std::unordered_map<std::uint32_t, std::int8_t>* cache,
    VerifiedRegionFailure* failure)
{
    return VerifyFunction(entry,
                          runtime_base,
                          runtime_size,
                          cache,
                          failure,
                          0);
}

bool ScanNativeRegionWithZydis(
    std::uint32_t entry,
    std::uint32_t runtime_base,
    std::uint32_t runtime_size,
    std::uint32_t max_sensitive,
    std::vector<std::uint32_t>* sensitive)
{
    constexpr std::size_t kMaxInstructions = 16384;
    if (sensitive == nullptr ||
        !IsRuntimeRange(entry, 1U, runtime_base, runtime_size))
    {
        return false;
    }
    sensitive->clear();
    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(
            &decoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32)))
    {
        return false;
    }
    std::vector<std::uint32_t> pending{entry};
    std::unordered_set<std::uint32_t> visited;
    bool has_return = false;
    while (!pending.empty() && visited.size() < kMaxInstructions)
    {
        const std::uint32_t address = pending.back();
        pending.pop_back();
        if (!visited.insert(address).second)
        {
            continue;
        }
        if (!IsRuntimeRange(address,
                            ZYDIS_MAX_INSTRUCTION_LENGTH,
                            runtime_base,
                            runtime_size))
        {
            return false;
        }
        ZydisDecodedInstruction instruction{};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
        const auto* bytes = reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(address));
        if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
                &decoder, bytes, ZYDIS_MAX_INSTRUCTION_LENGTH, &instruction,
                operands)) ||
            instruction.length == 0)
        {
            return false;
        }
        if (IsSensitive(instruction))
        {
            // HLE-sensitive instruction: record its address (deduped by the
            // visited set) and continue past it natively-in-region. A rep-prefixed
            // string op counts once; the HLE handler performs the whole operation.
            if (sensitive->size() >= max_sensitive)
            {
                return false;
            }
            sensitive->push_back(address);
            pending.push_back(address + instruction.length);
            continue;
        }
        const std::uint32_t next = address + instruction.length;
        const auto category = instruction.meta.category;
        if (category == ZYDIS_CATEGORY_RET)
        {
            has_return = true;
            continue;
        }
        if (category == ZYDIS_CATEGORY_CALL ||
            category == ZYDIS_CATEGORY_COND_BR ||
            category == ZYDIS_CATEGORY_UNCOND_BR)
        {
            std::uint32_t target = 0;
            if (!ReadDirectTarget(instruction, operands, address, &target) ||
                !IsRuntimeRange(target, 1U, runtime_base, runtime_size))
            {
                // An indirect or far transfer means we cannot enumerate every
                // reachable sensitive instruction, so the region is unprovable.
                return false;
            }
            if (category == ZYDIS_CATEGORY_CALL)
            {
                // Flatten the callee into this region and resume after the call.
                pending.push_back(target);
                pending.push_back(next);
            }
            else if (category == ZYDIS_CATEGORY_COND_BR)
            {
                pending.push_back(target);
                pending.push_back(next);
            }
            else
            {
                pending.push_back(target);
            }
            continue;
        }
        pending.push_back(next);
    }
    return has_return && pending.empty();
}

bool ScanNativeLinearSpanWithZydis(
    std::uint32_t entry,
    std::uint32_t runtime_base,
    std::uint32_t runtime_size,
    NativeLinearSpan* span,
    const NativeLinearSpanOptions* options)
{
    constexpr std::uint32_t kMinimumInstructionCount = 2;
    constexpr std::uint32_t kMaximumInstructionCount = 64;
    if (span == nullptr ||
        !IsRuntimeRange(entry, 1U, runtime_base, runtime_size))
    {
        return false;
    }
    *span = NativeLinearSpan{};
    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(
            &decoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32)))
    {
        return false;
    }

    std::uint32_t address = entry;
    std::uint32_t verified_write_guard_page = 0xFFFFFFFFU;
    std::array<bool, ZYDIS_REGISTER_MAX_VALUE + 1> modified_registers{};
    for (std::uint32_t count = 0;
         count < kMaximumInstructionCount; ++count)
    {
        const std::uint32_t address_page = address & 0xFFFFF000U;
        if (options != nullptr && options->allow_memory_writes &&
            address_page != verified_write_guard_page)
        {
            if (options->write_guard_query == nullptr ||
                !options->write_guard_query(
                    options->write_guard_context, address_page))
            {
                span->boundary_address = address;
                span->instruction_count = count;
                span->boundary_write_guard_uncovered = true;
                return count >= kMinimumInstructionCount;
            }
            verified_write_guard_page = address_page;
        }
        if (!IsRuntimeRange(address,
                            ZYDIS_MAX_INSTRUCTION_LENGTH,
                            runtime_base,
                            runtime_size))
        {
            return false;
        }
        ZydisDecodedInstruction instruction{};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
        const auto* bytes = reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(address));
        if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
                &decoder, bytes, ZYDIS_MAX_INSTRUCTION_LENGTH, &instruction,
                operands)) ||
            instruction.length == 0)
        {
            return false;
        }
        const std::uint32_t instruction_end_page =
            (address + instruction.length - 1U) & 0xFFFFF000U;
        if (options != nullptr && options->allow_memory_writes &&
            instruction_end_page != verified_write_guard_page)
        {
            if (options->write_guard_query == nullptr ||
                !options->write_guard_query(
                    options->write_guard_context, instruction_end_page))
            {
                span->boundary_address = address;
                span->instruction_count = count;
                span->boundary_write_guard_uncovered = true;
                return count >= kMinimumInstructionCount;
            }
            verified_write_guard_page = instruction_end_page;
        }

        const bool sensitive = IsSensitive(instruction);
        const bool memory_write =
            HasExplicitMemoryWrite(instruction, operands);
        const bool pass_memory_write = memory_write && count != 0U &&
            options != nullptr && options->allow_memory_writes &&
            !MemoryWriteUsesModifiedAddressRegister(
                instruction, operands, modified_registers) &&
            IsMemoryWriteTargetAllowed(
                instruction, operands, *options);
        const bool control_transfer =
            instruction.meta.branch_type != ZYDIS_BRANCH_TYPE_NONE;
        std::uint32_t direct_jump_target = 0;
        const bool direct_unconditional_jump =
            instruction.mnemonic == ZYDIS_MNEMONIC_JMP &&
            instruction.meta.category == ZYDIS_CATEGORY_UNCOND_BR &&
            ReadDirectTarget(
                instruction, operands, address, &direct_jump_target);
        const bool chain_direct_jump =
            options != nullptr &&
            options->chain_forward_direct_jumps &&
            direct_unconditional_jump && direct_jump_target > address &&
            IsRuntimeRange(
                direct_jump_target, 1U, runtime_base, runtime_size) &&
            options->direct_jump_target_query != nullptr &&
            options->direct_jump_target_query(
                options->write_guard_context, direct_jump_target);
        if (options != nullptr &&
            options->chain_forward_direct_jumps &&
            direct_unconditional_jump && direct_jump_target <= address)
        {
            span->boundary_backward_jump = true;
        }
        if (sensitive || (memory_write && !pass_memory_write) ||
            (control_transfer && !chain_direct_jump))
        {
            span->boundary_address = address;
            span->instruction_count = count;
            span->boundary_sensitive = sensitive;
            span->boundary_memory_write = memory_write;
            return count >= kMinimumInstructionCount;
        }
        if (pass_memory_write)
        {
            ++span->crossed_memory_write_count;
        }
        if (chain_direct_jump)
        {
            ++span->chained_direct_jump_count;
            address = direct_jump_target;
            continue;
        }
        RecordModifiedRegisters(
            instruction, operands, &modified_registers);
        address += instruction.length;
    }
    return false;
}

}  // namespace repiu::platform::win32::detail
