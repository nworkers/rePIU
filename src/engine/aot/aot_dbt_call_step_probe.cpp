#include "aot_dbt_call_step_probe.h"

#include "guest_memory_access.h"
#include "thread_context.h"

#include <cstdlib>
#include <cstring>
#include "repiu/platform/guest_cpu_context.h"

namespace repiu::engine
{
namespace
{

constexpr std::uint32_t kTrapFlag = 0x00000100U;
constexpr std::uint32_t kDebugEnableMask = 0x000000FFU;
constexpr std::uint32_t kDr01ControlMask = 0x00FF0000U;

bool IsTargetSequence(const ThreadContext& context,
                      std::uint32_t call_sequence)
{
    for (std::uint32_t index = 0;
         index < context.aot_dbt_call_step_probe_target_count;
         ++index)
    {
        if (context.aot_dbt_call_step_probe_targets[index] == call_sequence)
        {
            return true;
        }
    }
    return false;
}

void AppendEntry(ThreadContext* context,
                 AotCallStepProbeEntry entry)
{
    const std::uint32_t sequence =
        context->aot_dbt_call_step_probe_trace_count + 1U;
    entry.sequence = sequence;
    context->aot_dbt_call_step_probe_trace[
        (sequence - 1U) % kAotCallStepProbeTraceCapacity] = entry;
    context->aot_dbt_call_step_probe_trace_count = sequence;
}

void CaptureStack(ThreadContext* context,
                  std::uint32_t esp,
                  AotCallStepProbeEntry* entry)
{
    if (context == nullptr || entry == nullptr)
    {
        return;
    }
    for (std::uint32_t index = 0; index < 4U; ++index)
    {
        const std::uint32_t address =
            esp + index * sizeof(std::uint32_t);
        const auto* value = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(address));
        if (address >= esp &&
            IsGuestRangeReadable(context, value, sizeof(*value)))
        {
            std::memcpy(&entry->stack_dwords[index],
                        value, sizeof(*value));
            entry->stack_valid_mask |= 1U << index;
        }
    }
}

AotCallStepProbeEntry CaptureEntry(
    ThreadContext& context,
    const repiu::platform::GuestCpuContext& win32_context,
    AotCallStepProbeEventKind kind,
    std::uint32_t expected_eip,
    std::uint32_t expected_esp)
{
    AotCallStepProbeEntry entry;
    entry.kind = kind;
    entry.call_sequence =
        context.aot_dbt_call_step_probe_active_call_sequence;
    entry.guest_source = context.aot_dbt_call_step_probe_guest_source;
    entry.guest_target = context.aot_dbt_call_step_probe_guest_target;
    entry.guest_return = context.aot_dbt_call_step_probe_guest_return;
    entry.eip = static_cast<std::uint32_t>(win32_context.Eip);
    entry.esp = static_cast<std::uint32_t>(win32_context.Esp);
    entry.eflags = static_cast<std::uint32_t>(win32_context.EFlags);
    entry.eax = static_cast<std::uint32_t>(win32_context.Eax);
    entry.ebx = static_cast<std::uint32_t>(win32_context.Ebx);
    entry.ecx = static_cast<std::uint32_t>(win32_context.Ecx);
    entry.edx = static_cast<std::uint32_t>(win32_context.Edx);
    entry.esi = static_cast<std::uint32_t>(win32_context.Esi);
    entry.edi = static_cast<std::uint32_t>(win32_context.Edi);
    entry.ebp = static_cast<std::uint32_t>(win32_context.Ebp);
    entry.expected_eip = expected_eip;
    entry.expected_esp = expected_esp;
    entry.dr6 = static_cast<std::uint32_t>(win32_context.Dr6);
    entry.eip_matches = entry.eip == expected_eip;
    entry.esp_matches = entry.esp == expected_esp;
    CaptureStack(&context, entry.esp, &entry);
    return entry;
}

void RestoreDebugState(repiu::platform::GuestCpuContext* win32_context, ThreadContext* context)
{
    win32_context->Dr0 = context->aot_dbt_call_step_probe_saved_dr0;
    win32_context->Dr1 = context->aot_dbt_call_step_probe_saved_dr1;
    win32_context->Dr2 = context->aot_dbt_call_step_probe_saved_dr2;
    win32_context->Dr3 = context->aot_dbt_call_step_probe_saved_dr3;
    win32_context->Dr6 = context->aot_dbt_call_step_probe_saved_dr6;
    win32_context->Dr7 = context->aot_dbt_call_step_probe_saved_dr7;
    if (context->aot_dbt_call_step_probe_original_tf != 0U)
    {
        win32_context->EFlags |= kTrapFlag;
    }
    else
    {
        win32_context->EFlags &= ~kTrapFlag;
    }
}

void FinishProbe(ThreadContext* context)
{
    context->aot_dbt_call_step_probe_phase =
        AotCallStepProbePhase::kIdle;
    context->aot_dbt_call_step_probe_active_call_sequence = 0U;
}

void RecordConflict(repiu::platform::GuestCpuContext* win32_context, ThreadContext* context)
{
    AppendEntry(
        context,
        CaptureEntry(
            *context, *win32_context,
            AotCallStepProbeEventKind::kConflict,
            context->aot_dbt_call_step_probe_guest_return,
            context->aot_dbt_call_step_probe_entry_esp));
    ++context->aot_dbt_call_step_probe_conflict_count;
    FinishProbe(context);
}

}  // namespace

void ConfigureAotDbtCallStepProbe(
    ThreadContext* context, const char* sequence_list)
{
    if (context == nullptr || sequence_list == nullptr ||
        *sequence_list == '\0' ||
        !context->aot_dbt_call_return_trace_configured)
    {
        return;
    }
    const char* cursor = sequence_list;
    while (*cursor != '\0' &&
           context->aot_dbt_call_step_probe_target_count <
               kAotCallStepProbeTargetCapacity)
    {
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(cursor, &end, 10);
        if (end == cursor || parsed == 0UL ||
            parsed > 0xFFFFFFFFUL)
        {
            context->aot_dbt_call_step_probe_target_count = 0U;
            return;
        }
        const std::uint32_t sequence =
            static_cast<std::uint32_t>(parsed);
        if (!IsTargetSequence(*context, sequence))
        {
            context->aot_dbt_call_step_probe_targets[
                context->aot_dbt_call_step_probe_target_count++] =
                sequence;
        }
        if (*end == '\0')
        {
            cursor = end;
            break;
        }
        if (*end != ',')
        {
            context->aot_dbt_call_step_probe_target_count = 0U;
            return;
        }
        cursor = end + 1;
    }
    if (*cursor != '\0')
    {
        context->aot_dbt_call_step_probe_target_count = 0U;
        return;
    }
    context->aot_dbt_call_step_probe_configured =
        context->aot_dbt_call_step_probe_target_count != 0U;
}

bool MaybeArmAotDbtCallStepProbe(
    ThreadContext* context,
    AotTransferOrigin origin,
    const runtime::AotDbtIndirectDispatchSite& site,
    std::uint32_t call_sequence,
    std::uint32_t guest_target,
    std::uint32_t cache_target,
    std::uint32_t guest_return,
    std::uint32_t entry_esp,
    std::uint32_t* saved_eflags)
{
    if (context == nullptr || saved_eflags == nullptr ||
        !context->aot_dbt_call_step_probe_configured ||
        context->aot_placement == nullptr ||
        origin != AotTransferOrigin::kHost || !site.is_call ||
        call_sequence == 0U ||
        !IsTargetSequence(*context, call_sequence))
    {
        return false;
    }
    if (context->aot_dbt_call_step_probe_phase !=
        AotCallStepProbePhase::kIdle)
    {
        ++context->aot_dbt_call_step_probe_skipped_count;
        return false;
    }
    context->aot_dbt_call_step_probe_active_call_sequence = call_sequence;
    context->aot_dbt_call_step_probe_guest_source = site.guest_source;
    context->aot_dbt_call_step_probe_guest_target = guest_target;
    context->aot_dbt_call_step_probe_guest_return = guest_return;
    context->aot_dbt_call_step_probe_entry_esp = entry_esp;
    context->aot_dbt_call_step_probe_pre_eip =
        context->aot_placement->base_address + site.success_cache_offset;
    context->aot_dbt_call_step_probe_post_eip = cache_target;
    context->aot_dbt_call_step_probe_return_cache_eip = 0U;
    context->aot_dbt_call_step_probe_original_tf =
        *saved_eflags & kTrapFlag;
    context->aot_dbt_call_step_probe_phase =
        AotCallStepProbePhase::kAwaitPreC3;
    *saved_eflags |= kTrapFlag;
    ++context->aot_dbt_call_step_probe_arm_count;
    return true;
}

bool HandleAotDbtCallStepProbe(const repiu::platform::FaultEvent& fault,
                               ThreadContext* context)
{
    repiu::platform::GuestCpuContext* win32_context = fault.registers;
    if (win32_context == nullptr || context == nullptr ||
        context->aot_dbt_call_step_probe_phase ==
            AotCallStepProbePhase::kIdle ||
        fault.kind != repiu::platform::FaultKind::kSingleStep)
    {
        return false;
    }

    const AotCallStepProbePhase phase =
        context->aot_dbt_call_step_probe_phase;
    if (phase == AotCallStepProbePhase::kAwaitPreC3)
    {
        const std::uint32_t expected_esp =
            context->aot_dbt_call_step_probe_entry_esp - 8U;
        AotCallStepProbeEntry entry = CaptureEntry(
            *context, *win32_context,
            AotCallStepProbeEventKind::kPreC3,
            context->aot_dbt_call_step_probe_pre_eip,
            expected_esp);
        AppendEntry(context, entry);
        if (!entry.eip_matches || !entry.esp_matches)
        {
            AppendEntry(
                context,
                CaptureEntry(
                    *context, *win32_context,
                    AotCallStepProbeEventKind::kUnexpected,
                    context->aot_dbt_call_step_probe_pre_eip,
                    expected_esp));
            if (context->aot_dbt_call_step_probe_original_tf != 0U)
            {
                win32_context->EFlags |= kTrapFlag;
            }
            else
            {
                win32_context->EFlags &= ~kTrapFlag;
            }
            ++context->aot_dbt_call_step_probe_conflict_count;
            FinishProbe(context);
            return false;
        }
        context->aot_dbt_call_step_probe_phase =
            AotCallStepProbePhase::kAwaitPostC3;
        win32_context->Dr6 = 0U;
        win32_context->EFlags |= kTrapFlag;
        return true;
    }

    if (phase == AotCallStepProbePhase::kAwaitPostC3)
    {
        const std::uint32_t expected_esp =
            context->aot_dbt_call_step_probe_entry_esp - 4U;
        AotCallStepProbeEntry entry = CaptureEntry(
            *context, *win32_context,
            AotCallStepProbeEventKind::kPostC3,
            context->aot_dbt_call_step_probe_post_eip,
            expected_esp);
        AppendEntry(context, entry);
        if (context->aot_dbt_call_step_probe_original_tf != 0U)
        {
            win32_context->EFlags |= kTrapFlag;
        }
        else
        {
            win32_context->EFlags &= ~kTrapFlag;
        }
        if (!entry.eip_matches || !entry.esp_matches ||
            context->native_fast_path.active ||
            context->native_fast_path.region_active ||
            context->native_fast_path.linear_span_active ||
            (static_cast<std::uint32_t>(win32_context->Dr7) &
             kDebugEnableMask) != 0U)
        {
            RecordConflict(win32_context, context);
            return true;
        }

        std::uint32_t return_cache = 0U;
        const bool has_return_cache =
            context->aot_placement != nullptr &&
            FindAotCacheAddress(
                *context->aot_placement,
                context->aot_dbt_call_step_probe_guest_return,
                &return_cache);
        context->aot_dbt_call_step_probe_return_cache_eip =
            has_return_cache ? return_cache : 0U;
        context->aot_dbt_call_step_probe_saved_dr0 =
            static_cast<std::uint32_t>(win32_context->Dr0);
        context->aot_dbt_call_step_probe_saved_dr1 =
            static_cast<std::uint32_t>(win32_context->Dr1);
        context->aot_dbt_call_step_probe_saved_dr2 =
            static_cast<std::uint32_t>(win32_context->Dr2);
        context->aot_dbt_call_step_probe_saved_dr3 =
            static_cast<std::uint32_t>(win32_context->Dr3);
        context->aot_dbt_call_step_probe_saved_dr6 =
            static_cast<std::uint32_t>(win32_context->Dr6);
        context->aot_dbt_call_step_probe_saved_dr7 =
            static_cast<std::uint32_t>(win32_context->Dr7);
        win32_context->Dr0 = has_return_cache
            ? return_cache
            : context->aot_dbt_call_step_probe_guest_return;
        win32_context->Dr1 =
            context->aot_dbt_call_step_probe_guest_return;
        win32_context->Dr6 = 0U;
        win32_context->Dr7 =
            (static_cast<std::uint32_t>(win32_context->Dr7) &
             ~(kDebugEnableMask | kDr01ControlMask)) |
            (has_return_cache ? 0x5U : 0x1U);
        context->aot_dbt_call_step_probe_phase =
            AotCallStepProbePhase::kAwaitReturnTarget;
        return true;
    }

    if (phase == AotCallStepProbePhase::kAwaitReturnTarget)
    {
        const std::uint32_t dr6 =
            static_cast<std::uint32_t>(win32_context->Dr6);
        const std::uint32_t eip =
            static_cast<std::uint32_t>(win32_context->Eip);
        const bool cache_hit =
            (dr6 & 0x1U) != 0U &&
            eip == (context->aot_dbt_call_step_probe_return_cache_eip != 0U
                        ? context->aot_dbt_call_step_probe_return_cache_eip
                        : context->aot_dbt_call_step_probe_guest_return);
        const bool guest_hit =
            (dr6 & 0x2U) != 0U &&
            eip == context->aot_dbt_call_step_probe_guest_return;
        if (!cache_hit && !guest_hit)
        {
            return false;
        }
        AppendEntry(
            context,
            CaptureEntry(
                *context, *win32_context,
                AotCallStepProbeEventKind::kReturnTarget,
                cache_hit
                    ? (context->aot_dbt_call_step_probe_return_cache_eip != 0U
                           ? context->aot_dbt_call_step_probe_return_cache_eip
                           : context->aot_dbt_call_step_probe_guest_return)
                    : context->aot_dbt_call_step_probe_guest_return,
                context->aot_dbt_call_step_probe_entry_esp));
        RestoreDebugState(win32_context, context);
        ++context->aot_dbt_call_step_probe_complete_count;
        FinishProbe(context);
        return true;
    }
    return false;
}

bool AotDbtCallStepReturnWatchActive(const ThreadContext* context)
{
    return context != nullptr &&
           context->aot_dbt_call_step_probe_phase ==
               AotCallStepProbePhase::kAwaitReturnTarget;
}

}  // namespace repiu::engine
