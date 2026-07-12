#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "native_fast_path.h"
#include "verified_region_analyzer.h"

#include <cstring>

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

}  // namespace

bool TryEnterNativeFastPath(CONTEXT* context,
                            NativeFastPathState* state,
                            std::uint32_t runtime_base,
                            std::uint32_t runtime_size)
{
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
    std::int32_t displacement = 0;
    std::memcpy(&displacement, previous + 1, sizeof(displacement));
    if (previous[0] != 0xE8U ||
        previous_eip + 5U + displacement != context->Eip)
    {
        return false;
    }

    const auto cached = state->verification_cache.find(context->Eip);
    const bool was_cached = cached != state->verification_cache.end();
    VerifiedRegionFailure failure{};
    if (!VerifyNativeFunctionWithZydis(context->Eip,
                                       runtime_base,
                                       runtime_size,
                                       &state->verification_cache,
                                       &failure))
    {
        state->last_rejected_candidate.store(
            context->Eip, std::memory_order_relaxed);
        state->last_rejected_instruction.store(
            failure.instruction, std::memory_order_relaxed);
        state->last_rejected_opcode.store(
            failure.opcode, std::memory_order_relaxed);
        state->last_rejected_bytes_low.store(
            failure.bytes_low, std::memory_order_relaxed);
        state->last_rejected_bytes_high.store(
            failure.bytes_high, std::memory_order_relaxed);
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
        state->verification_cache[state->last_entry] = -1;
        state->rejected_count.fetch_add(1, std::memory_order_relaxed);
    }
}

}  // namespace repiu::platform::win32::detail
