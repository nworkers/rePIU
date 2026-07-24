#include "aot_dbt_dispatch.h"

#include "aot_runtime_dispatch.h"
#include "execution_internal.h"
#include "guest_memory_access.h"

#include <Zydis.h>

namespace repiu::platform::win32
{
namespace
{

bool PostHleTranslationEnabled()
{
    static const bool enabled = [] {
        char value[16] = {};
        const DWORD length = GetEnvironmentVariableA(
            "REPIU_AOT_DBT_POST_HLE_TRANSLATE", value, sizeof(value));
        return length != 0U && length < sizeof(value) &&
            ResolveAotDbtPostHleTranslationEnabled(
                std::string_view(value, length));
    }();
    return enabled;
}

bool DoesGuestInstructionWriteSegmentRegister(ThreadContext* context,
                                              std::uint32_t guest_eip)
{
    const auto* code = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(guest_eip));
    if (context == nullptr ||
        !IsGuestRangeReadable(context, code, 15U))
    {
        return true;
    }
    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(
            &decoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32)))
    {
        return true;
    }
    ZydisDecodedInstruction instruction{};
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
            &decoder, code, 15U, &instruction, operands)))
    {
        return true;
    }
    for (std::uint8_t index = 0;
         index < instruction.operand_count; ++index)
    {
        if (operands[index].type == ZYDIS_OPERAND_TYPE_REGISTER &&
            ZydisRegisterGetClass(operands[index].reg.value) ==
                ZYDIS_REGCLASS_SEGMENT &&
            (operands[index].actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) != 0)
        {
            return true;
        }
    }
    return false;
}

bool IsImmediateHleReentrySpanSafe(ThreadContext* context,
                                   std::uint32_t guest_entry)
{
    if (context == nullptr)
    {
        return false;
    }

    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(
            &decoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32)))
    {
        return false;
    }

    constexpr std::uint32_t kInstructionCap = 64U;
    std::uint32_t eip = guest_entry;
    for (std::uint32_t count = 0; count < kInstructionCap; ++count)
    {
        if (IsAotHleBoundaryAddress(context, eip))
        {
            return false;
        }
        const auto* code = reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(eip));
        if (!IsGuestRangeReadable(context, code, 15U))
        {
            return false;
        }
        ZydisDecodedInstruction instruction{};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
        if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
                &decoder, code, 15U, &instruction, operands)))
        {
            return false;
        }
        if (instruction.meta.branch_type != ZYDIS_BRANCH_TYPE_NONE ||
            instruction.mnemonic == ZYDIS_MNEMONIC_RET ||
            instruction.mnemonic == ZYDIS_MNEMONIC_IRET ||
            instruction.mnemonic == ZYDIS_MNEMONIC_IRETD)
        {
            return true;
        }
        eip += instruction.length;
    }
    return false;
}

}  // namespace

bool ResolveAotDbtPostHleTranslationEnabled(std::string_view setting)
{
    return setting == "1" || setting == "on" || setting == "true";
}

bool TryResumeAotAfterHandledHle(CONTEXT* win32_context,
                                 ThreadContext* context,
                                 std::uint32_t handled_guest_eip)
{
    if (win32_context == nullptr || context == nullptr ||
        context->aot_placement == nullptr ||
        !context->aot_reentry_pending ||
        !runtime::ExecutionBackendUsesImmediateHleReentry(
            context->execution_backend) ||
        DoesGuestInstructionWriteSegmentRegister(
            context, handled_guest_eip))
    {
        return false;
    }

    context->aot_dbt_hle_reentry_attempt_count.fetch_add(
        1, std::memory_order_relaxed);
    const std::uint32_t current =
        static_cast<std::uint32_t>(win32_context->Eip);
    if (!IsGuestInstructionPointer(context, current) ||
        IsWin32AotGuestPageQuarantined(
            *context->aot_placement, current))
    {
        return false;
    }

    std::uint32_t cache_address = 0;
    const bool cache_hit = FindAotCacheAddress(
        *context->aot_placement, current, &cache_address);
    if (cache_hit)
    {
        if (!IsImmediateHleReentrySpanSafe(context, current))
        {
            return false;
        }
    }
    else
    {
        if (!PostHleTranslationEnabled())
        {
            return false;
        }
        context->aot_dbt_hle_translation_attempt_count.fetch_add(
            1U, std::memory_order_relaxed);
        if (!ResolveAotTransferTarget(
                context, current, &cache_address))
        {
            return false;
        }
        context->aot_dbt_hle_translation_success_count.fetch_add(
            1U, std::memory_order_relaxed);
    }

    win32_context->Eip = cache_address;
    win32_context->EFlags &= ~0x00000100U;
    context->aot_reentry_pending = false;
    context->aot_legacy_fallback = false;
    context->enable_single_step_trace = false;
    context->aot_dbt_hle_reentry_success_count.fetch_add(
        1, std::memory_order_relaxed);
    AccumulateAotResidency(context, current);
    BumpAotReentryCount(context);
    return true;
}

}  // namespace repiu::platform::win32
