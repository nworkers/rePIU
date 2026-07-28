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

// Task 346: "cannot tell" and "writes one" now have different consequences, so
// they stop sharing a return value.
enum class SegmentWriteProbe
{
    kUnknown,
    kNo,
    kYes,
};

// Task 346. Restores the pre-Task-346 blanket refusal for A/B in one binary.
bool SegmentWriteBlocksResumeEnabled()
{
    static const bool enabled = []() {
        const char* value =
            std::getenv("REPIU_AOT_SEGMENT_WRITE_BLOCKS_RESUME");
        return value != nullptr && std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

SegmentWriteProbe ProbeGuestInstructionSegmentWrite(ThreadContext* context,
                                                   std::uint32_t guest_eip)
{
    const auto* code = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(guest_eip));
    if (context == nullptr ||
        !IsGuestRangeReadable(context, code, 15U))
    {
        return SegmentWriteProbe::kUnknown;
    }
    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(
            &decoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32)))
    {
        return SegmentWriteProbe::kUnknown;
    }
    ZydisDecodedInstruction instruction{};
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
            &decoder, code, 15U, &instruction, operands)))
    {
        return SegmentWriteProbe::kUnknown;
    }
    for (std::uint8_t index = 0;
         index < instruction.operand_count; ++index)
    {
        if (operands[index].type == ZYDIS_OPERAND_TYPE_REGISTER &&
            ZydisRegisterGetClass(operands[index].reg.value) ==
                ZYDIS_REGCLASS_SEGMENT &&
            (operands[index].actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) != 0)
        {
            return SegmentWriteProbe::kYes;
        }
    }
    return SegmentWriteProbe::kNo;
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
        context->aot_placement == nullptr)
    {
        return false;
    }
    // Task 340: the first guard rejected 98.7% of SUPERBLOCK attempts, and its
    // conditions are split here so the reason is named rather than inferred.
    if (!runtime::ExecutionBackendUsesImmediateHleReentry(
            context->execution_backend))
    {
        ++context->hle_reentry_reject_backend;
        return false;
    }
    if (!context->aot_reentry_pending)
    {
        ++context->hle_reentry_reject_not_pending;
        return false;
    }

    // Task 323 sub-stage attribution. Each region below opens a scope against
    // the single-step sample published in ThreadContext::active_hotspot_scope,
    // so sum(sub-stage) <= kAotResume holds by construction. All scopes are
    // inert when the hotspot profile is disabled.
    {
        SingleStepHotspotStageScope stage_scope(
            context->active_hotspot_scope,
            SingleStepProfileStage::kSegmentWriteProbe);
        // Task 346: a segment write no longer abandons the return. The cache
        // folds segment bases into displacements, but re-folding is already
        // performed by the segment-load HLE itself, and every folded site
        // carries a selector guard that traps to a fixed INT3 when the current
        // selector differs from the folded one -- so a stale fold cannot
        // execute silently. Only an unreadable or undecodable instruction still
        // fails closed. See docs/design/20260728-346-resume-after-segment-write.md.
        const SegmentWriteProbe segment_write =
            ProbeGuestInstructionSegmentWrite(context, handled_guest_eip);
        if (segment_write == SegmentWriteProbe::kUnknown ||
            (segment_write == SegmentWriteProbe::kYes &&
             SegmentWriteBlocksResumeEnabled()))
        {
            ++context->hle_reentry_reject_segment_write;
            return false;
        }
        if (segment_write == SegmentWriteProbe::kYes)
        {
            ReResolveAotSegmentOverrides(context);
            ++context->hle_reentry_segment_write_resumed;
        }
    }

    context->aot_dbt_hle_reentry_attempt_count.fetch_add(
        1, std::memory_order_relaxed);
    const std::uint32_t current =
        static_cast<std::uint32_t>(win32_context->Eip);
    {
        SingleStepHotspotStageScope stage_scope(
            context->active_hotspot_scope,
            SingleStepProfileStage::kQuarantineCheck);
        // Task 340: Task 339 measured this pair rejecting 88.7% of baseline
        // attempts without separating them. "Outside the arena" means the HLE
        // left EIP somewhere the cache could never cover; "quarantined" means
        // the page is deliberately excluded. Different causes, different fixes.
        if (!IsGuestInstructionPointer(context, current))
        {
            ++context->hle_reentry_reject_outside_arena;
            return false;
        }
        if (IsWin32AotGuestPageQuarantined(*context->aot_placement, current))
        {
            ++context->hle_reentry_reject_quarantined;
            return false;
        }
    }

    std::uint32_t cache_address = 0;
    bool cache_hit = false;
    {
        SingleStepHotspotStageScope stage_scope(
            context->active_hotspot_scope,
            SingleStepProfileStage::kCacheLookup);
        cache_hit = FindAotCacheAddress(
            *context->aot_placement, current, &cache_address);
    }
    if (cache_hit)
    {
        SingleStepHotspotStageScope stage_scope(
            context->active_hotspot_scope,
            SingleStepProfileStage::kSpanSafety);
        if (!IsImmediateHleReentrySpanSafe(context, current))
        {
            ++context->hle_reentry_reject_span_unsafe;
            return false;
        }
    }
    else
    {
        // Task 340: counted before the opt-in is consulted, so a cache miss is
        // visible whether or not post-HLE translation is enabled. Task 339
        // found this branch unreachable in practice; this proves it per run.
        ++context->hle_reentry_reject_cache_miss;
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
    ++context->hle_reentry_success;
    context->aot_dbt_hle_reentry_success_count.fetch_add(
        1, std::memory_order_relaxed);
    AccumulateAotResidency(context, current);
    BumpAotReentryCount(context);
    return true;
}

}  // namespace repiu::platform::win32
