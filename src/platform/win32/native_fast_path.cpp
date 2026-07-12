#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "native_fast_path.h"

#include <cstddef>
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

std::uint32_t ModRmSize(const std::uint8_t* bytes)
{
    const std::uint8_t modrm = bytes[0];
    const std::uint8_t mod = modrm >> 6U;
    const std::uint8_t rm = modrm & 7U;
    std::uint32_t size = 1;
    if (mod != 3U && rm == 4U)
    {
        const std::uint8_t sib = bytes[size++];
        if (mod == 0U && (sib & 7U) == 5U)
        {
            size += 4;
        }
    }
    if (mod == 0U && rm == 5U)
    {
        size += 4;
    }
    else if (mod == 1U)
    {
        ++size;
    }
    else if (mod == 2U)
    {
        size += 4;
    }
    return size;
}

enum class FlowKind
{
    kNext,
    kConditional,
    kJump,
    kCall,
    kReturn,
    kReject,
};

struct DecodedInstruction
{
    FlowKind flow = FlowKind::kReject;
    std::uint32_t size = 0;
    std::uint32_t target = 0;
};

DecodedInstruction DecodeSafeInstruction(std::uint32_t address)
{
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(address));
    const std::uint8_t opcode = bytes[0];
    DecodedInstruction result{FlowKind::kNext, 1, 0};
    if ((opcode >= 0x40U && opcode <= 0x5FU) || opcode == 0x90U ||
        opcode == 0x98U || opcode == 0x99U || opcode == 0x9EU ||
        opcode == 0x9FU || opcode == 0xC9U)
    {
        return result;
    }
    if (opcode >= 0xB8U && opcode <= 0xBFU)
    {
        result.size = 5;
        return result;
    }
    if (opcode == 0x68U ||
        opcode == 0x05U || opcode == 0x0DU || opcode == 0x15U ||
        opcode == 0x1DU || opcode == 0x25U || opcode == 0x2DU ||
        opcode == 0x35U || opcode == 0x3DU ||
        (opcode >= 0xA0U && opcode <= 0xA3U))
    {
        result.size = 5;
        return result;
    }
    if (opcode == 0x6AU)
    {
        result.size = 2;
        return result;
    }
    if (opcode == 0xC3U)
    {
        return {FlowKind::kReturn, 1, 0};
    }
    if (opcode == 0xC2U)
    {
        return {FlowKind::kReturn, 3, 0};
    }
    if (opcode >= 0x70U && opcode <= 0x7FU)
    {
        const auto displacement = static_cast<std::int8_t>(bytes[1]);
        return {FlowKind::kConditional,
                2,
                address + 2U + static_cast<std::int32_t>(displacement)};
    }
    if (opcode >= 0xE0U && opcode <= 0xE3U)
    {
        const auto displacement = static_cast<std::int8_t>(bytes[1]);
        return {FlowKind::kConditional,
                2,
                address + 2U + static_cast<std::int32_t>(displacement)};
    }
    if (opcode == 0xEBU)
    {
        const auto displacement = static_cast<std::int8_t>(bytes[1]);
        return {FlowKind::kJump,
                2,
                address + 2U + static_cast<std::int32_t>(displacement)};
    }
    if (opcode == 0xE8U || opcode == 0xE9U)
    {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, bytes + 1, sizeof(displacement));
        return {opcode == 0xE8U ? FlowKind::kCall : FlowKind::kJump,
                5,
                address + 5U + displacement};
    }
    if (opcode == 0x0FU)
    {
        const std::uint8_t second = bytes[1];
        if (second >= 0x80U && second <= 0x8FU)
        {
            std::int32_t displacement = 0;
            std::memcpy(&displacement, bytes + 2, sizeof(displacement));
            return {FlowKind::kConditional,
                    6,
                    address + 6U + displacement};
        }
        if (second == 0xAFU || second == 0xB6U || second == 0xB7U ||
            second == 0xBEU || second == 0xBFU)
        {
            return {FlowKind::kNext, 2U + ModRmSize(bytes + 2), 0};
        }
        return {};
    }
    const bool modrm =
        opcode == 0x00U || opcode == 0x01U || opcode == 0x02U ||
        opcode == 0x03U || opcode == 0x08U || opcode == 0x09U ||
        opcode == 0x0AU || opcode == 0x0BU || opcode == 0x10U ||
        opcode == 0x11U || opcode == 0x12U || opcode == 0x13U ||
        opcode == 0x18U || opcode == 0x19U || opcode == 0x1AU ||
        opcode == 0x1BU || opcode == 0x20U || opcode == 0x21U ||
        opcode == 0x22U || opcode == 0x23U || opcode == 0x28U ||
        opcode == 0x29U || opcode == 0x2AU || opcode == 0x2BU ||
        opcode == 0x30U || opcode == 0x31U || opcode == 0x32U ||
        opcode == 0x33U || opcode == 0x38U || opcode == 0x39U ||
        opcode == 0x3AU || opcode == 0x3BU || opcode == 0x63U ||
        opcode == 0x84U || opcode == 0x85U || opcode == 0x86U ||
        opcode == 0x87U || opcode == 0x88U || opcode == 0x89U ||
        opcode == 0x8AU || opcode == 0x8BU ||
        opcode == 0x8DU || opcode == 0x8FU || opcode == 0xC0U ||
        opcode == 0xC1U || opcode == 0xD0U || opcode == 0xD1U ||
        opcode == 0xD2U || opcode == 0xD3U ||
        (opcode >= 0xD8U && opcode <= 0xDFU) ||
        opcode == 0xF6U || opcode == 0xF7U || opcode == 0xFEU ||
        opcode == 0xFFU || opcode == 0x69U || opcode == 0x6BU ||
        opcode == 0x80U || opcode == 0x81U || opcode == 0x83U ||
        opcode == 0xC6U || opcode == 0xC7U;
    if (!modrm)
    {
        return {};
    }
    const std::uint8_t group = (bytes[1] >> 3U) & 7U;
    if (opcode == 0xFFU && group != 0U && group != 1U && group != 6U)
    {
        return {};
    }
    result.size = 1U + ModRmSize(bytes + 1);
    if (opcode == 0x80U || opcode == 0x83U || opcode == 0xC0U ||
        opcode == 0xC6U || opcode == 0x6BU ||
        (opcode == 0xF6U && group == 0U))
    {
        ++result.size;
    }
    else if (opcode == 0x81U || opcode == 0xC7U || opcode == 0x69U ||
             (opcode == 0xF7U && group == 0U))
    {
        result.size += 4;
    }
    return result;
}

bool VerifyFunction(std::uint32_t entry,
                    std::uint32_t runtime_base,
                    std::uint32_t runtime_size,
                    std::unordered_map<std::uint32_t, std::int8_t>* cache,
                    std::uint32_t depth,
                    std::atomic<std::uint32_t>* rejected_instruction,
                    std::atomic<std::uint32_t>* rejected_opcode,
                    std::atomic<std::uint32_t>* rejected_bytes_low,
                    std::atomic<std::uint32_t>* rejected_bytes_high)
{
    constexpr std::uint32_t kMaxDepth = 8;
    constexpr std::size_t kMaxInstructions = 4096;
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
        if (!IsRuntimeRange(address, 16U, runtime_base, runtime_size))
        {
            (*cache)[entry] = -1;
            return false;
        }
        const DecodedInstruction instruction = DecodeSafeInstruction(address);
        if (instruction.flow == FlowKind::kReject || instruction.size == 0)
        {
            if (rejected_instruction != nullptr && rejected_opcode != nullptr)
            {
                rejected_instruction->store(address, std::memory_order_relaxed);
                rejected_opcode->store(
                    *reinterpret_cast<const std::uint8_t*>(
                        static_cast<std::uintptr_t>(address)),
                    std::memory_order_relaxed);
                std::uint32_t bytes_low = 0;
                std::uint32_t bytes_high = 0;
                std::memcpy(&bytes_low,
                            reinterpret_cast<const void*>(
                                static_cast<std::uintptr_t>(address)),
                            sizeof(bytes_low));
                std::memcpy(&bytes_high,
                            reinterpret_cast<const void*>(
                                static_cast<std::uintptr_t>(address + 4U)),
                            sizeof(bytes_high));
                rejected_bytes_low->store(bytes_low, std::memory_order_relaxed);
                rejected_bytes_high->store(bytes_high, std::memory_order_relaxed);
            }
            (*cache)[entry] = -1;
            return false;
        }
        const std::uint32_t next = address + instruction.size;
        if (instruction.flow == FlowKind::kReturn)
        {
            has_return = true;
            continue;
        }
        if ((instruction.flow == FlowKind::kConditional ||
             instruction.flow == FlowKind::kJump ||
             instruction.flow == FlowKind::kCall) &&
            !IsRuntimeRange(instruction.target, 1U, runtime_base, runtime_size))
        {
            (*cache)[entry] = -1;
            return false;
        }
        if (instruction.flow == FlowKind::kCall)
        {
            if (!VerifyFunction(instruction.target,
                                runtime_base,
                                runtime_size,
                                cache,
                                depth + 1U,
                                rejected_instruction,
                                rejected_opcode,
                                rejected_bytes_low,
                                rejected_bytes_high))
            {
                (*cache)[entry] = -1;
                return false;
            }
            pending.push_back(next);
        }
        else if (instruction.flow == FlowKind::kConditional)
        {
            pending.push_back(instruction.target);
            pending.push_back(next);
        }
        else if (instruction.flow == FlowKind::kJump)
        {
            pending.push_back(instruction.target);
        }
        else
        {
            pending.push_back(next);
        }
    }
    const bool safe = has_return && pending.empty();
    (*cache)[entry] = safe ? 1 : -1;
    return safe;
}

}  // namespace

bool TryEnterNativeFastPath(CONTEXT* context,
                            NativeFastPathState* state,
                            std::uint32_t runtime_base,
                            std::uint32_t runtime_size)
{
    // The in-tree decoder prototype remains fail-closed until instruction
    // boundaries are supplied by a complete, validated x86 decoder.
    constexpr bool kEnableExperimentalVerifier = false;
    if (!kEnableExperimentalVerifier)
    {
        return false;
    }
    if (context == nullptr || state == nullptr || state->active ||
        !IsRuntimeRange(context->Esp,
                        sizeof(std::uint32_t),
                        runtime_base,
                        runtime_size))
    {
        return false;
    }
    const std::uint32_t previous_eip = state->previous_eip;
    state->previous_eip = context->Eip;
    if (!IsRuntimeRange(previous_eip, 5U, runtime_base, runtime_size))
    {
        return false;
    }
    const auto* previous = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(previous_eip));
    std::int32_t call_displacement = 0;
    std::memcpy(&call_displacement,
                previous + 1,
                sizeof(call_displacement));
    if (previous[0] != 0xE8U ||
        previous_eip + 5U + call_displacement != context->Eip)
    {
        return false;
    }
    const auto cached = state->verification_cache.find(context->Eip);
    const bool was_cached = cached != state->verification_cache.end();
    if (!VerifyFunction(context->Eip,
                        runtime_base,
                        runtime_size,
                        &state->verification_cache,
                        0,
                        &state->last_rejected_instruction,
                        &state->last_rejected_opcode,
                        &state->last_rejected_bytes_low,
                        &state->last_rejected_bytes_high))
    {
        state->last_rejected_candidate.store(
            context->Eip, std::memory_order_relaxed);
        if (!was_cached)
        {
            state->rejected_count.fetch_add(1, std::memory_order_relaxed);
        }
        return false;
    }
    if (!was_cached)
    {
        state->verified_count.fetch_add(1, std::memory_order_relaxed);
    }
    const std::uint32_t return_address =
        *reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(context->Esp));
    if (!IsRuntimeRange(return_address, 1U, runtime_base, runtime_size))
    {
        return false;
    }

    state->active = true;
    state->return_address = return_address;
    state->saved_dr0 = static_cast<std::uint32_t>(context->Dr0);
    state->saved_dr6 = static_cast<std::uint32_t>(context->Dr6);
    state->saved_dr7 = static_cast<std::uint32_t>(context->Dr7);
    state->last_entry = context->Eip;
    state->entry_count.fetch_add(1, std::memory_order_relaxed);
    context->Dr0 = return_address;
    context->Dr6 = 0;
    context->Dr7 = (context->Dr7 & ~0x000F0003U) | 0x1U;
    context->EFlags &= ~0x00000100U;
    return true;
}

void LeaveNativeFastPath(CONTEXT* context,
                         NativeFastPathState* state,
                         bool returned)
{
    if (context == nullptr || state == nullptr || !state->active)
    {
        return;
    }
    context->Dr0 = state->saved_dr0;
    context->Dr6 = state->saved_dr6;
    context->Dr7 = state->saved_dr7;
    context->EFlags |= 0x00000100U;
    state->active = false;
    if (returned)
    {
        state->return_count.fetch_add(1, std::memory_order_relaxed);
        state->last_return = context->Eip;
    }
    else
    {
        state->cancel_count.fetch_add(1, std::memory_order_relaxed);
    }
}

}  // namespace repiu::platform::win32::detail
