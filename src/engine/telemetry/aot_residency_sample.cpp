#include "aot_residency_sample.h"

#include "repiu/runtime/env_toggle.h"
#include "thread_context.h"
#include "execution_internal.h"
#include "guest_memory_access.h"

#include <Zydis.h>

#include <cstdlib>
#include "repiu/platform/atomic_ops.h"

namespace repiu::engine
{

bool ResolveAotResidencySampleEnabled(const char* setting)
{
    return repiu::runtime::ResolveOptInToggle(setting);
}

bool AotResidencySampleEnabled()
{
    static const bool enabled = ResolveAotResidencySampleEnabled(
        std::getenv("REPIU_AOT_RESIDENCY_SAMPLE"));
    return enabled;
}

void AccumulateAotResidency(ThreadContext* context,
                            std::uint32_t guest_entry_eip)
{
    // The gate comes first so a disabled sampler costs one predictable branch:
    // no timing scope, no decoder, no guest reads.
    if (!AotResidencySampleEnabled())
    {
        return;
    }
    // Task 326 function-axis attribution. Instrumented at the definition so
    // calls from aot_dbt_dispatch.cpp and aot_dbt_hle_dispatch.cpp are included
    // too; that surplus over the handler axis is itself informative.
    const ExecutionTimeScope residency_time_scope(
        context != nullptr ? context->execution_time_profile.get() : nullptr,
        ExecutionTimeBucket::kAotResidency);
    // Task 478: the decoder state is fixed at (LEGACY_32, STACK_WIDTH_32) and is
    // not mutated while decoding, so one initialization serves every sample. It
    // used to run once per call, 3,076,235 times in the measured profile.
    static ZydisDecoder decoder;
    static const bool decoder_ready =
        ZYAN_SUCCESS(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LEGACY_32,
                                      ZYDIS_STACK_WIDTH_32));
    if (!decoder_ready)
    {
        return;
    }
    constexpr std::uint32_t kResidencyCap = 64U;
    std::uint32_t eip = guest_entry_eip;
    std::uint32_t count = 0;
    while (count < kResidencyCap)
    {
        const auto* code = reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(eip));
        // A full x86 instruction is at most 15 bytes; require that window to be
        // readable so the decode never faults (page-edge samples stop here).
        if (!IsGuestRangeReadable(context, code, 15U))
        {
            break;
        }
        ZydisDecodedInstruction instruction{};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
        if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, code, 15,
                                                 &instruction, operands)))
        {
            break;
        }
        ++count;
        const bool is_transfer =
            instruction.meta.branch_type != ZYDIS_BRANCH_TYPE_NONE ||
            instruction.mnemonic == ZYDIS_MNEMONIC_RET ||
            instruction.mnemonic == ZYDIS_MNEMONIC_IRET ||
            instruction.mnemonic == ZYDIS_MNEMONIC_IRETD;
        if (is_transfer)
        {
            break;
        }
        eip += instruction.length;
    }
    if (count == 0)
    {
        return;
    }
    context->aot_residency_instruction_total.fetch_add(
        count, std::memory_order_relaxed);
    context->aot_residency_sample_count.fetch_add(1U, std::memory_order_relaxed);
    std::uint32_t prev_max =
        context->aot_residency_max.load(std::memory_order_relaxed);
    while (count > prev_max &&
           !context->aot_residency_max.compare_exchange_weak(
               prev_max, count, std::memory_order_relaxed))
    {
    }
    if (context->shared_live_telemetry != nullptr)
    {
        SharedLiveTelemetry* telemetry = context->shared_live_telemetry;
        repiu::platform::AtomicExchangeAdd(&telemetry->aot_residency_total,
                               static_cast<long>(count));
        repiu::platform::AtomicIncrement(&telemetry->aot_residency_samples);
        if (static_cast<std::uint32_t>(telemetry->aot_residency_max) < count)
        {
            repiu::platform::AtomicExchange(&telemetry->aot_residency_max,
                                static_cast<long>(count));
        }
    }
}

}  // namespace repiu::engine
