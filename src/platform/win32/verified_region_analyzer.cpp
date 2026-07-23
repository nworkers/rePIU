#include "verified_region_analyzer.h"

#include <Zydis.h>

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
    NativeLinearSpan* span)
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
    for (std::uint32_t count = 0;
         count < kMaximumInstructionCount; ++count)
    {
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

        const bool sensitive = IsSensitive(instruction);
        const bool memory_write =
            HasExplicitMemoryWrite(instruction, operands);
        const bool control_transfer =
            instruction.meta.branch_type != ZYDIS_BRANCH_TYPE_NONE;
        if (sensitive || memory_write || control_transfer)
        {
            span->boundary_address = address;
            span->instruction_count = count;
            span->boundary_sensitive = sensitive;
            span->boundary_memory_write = memory_write;
            return count >= kMinimumInstructionCount;
        }
        address += instruction.length;
    }
    return false;
}

}  // namespace repiu::platform::win32::detail
