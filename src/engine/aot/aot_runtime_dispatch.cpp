#include "aot_runtime_dispatch.h"
#include "repiu/runtime/env_toggle.h"
#include "aot_generation_failure_policy.h"
#include "aot_dbt_glide_gate_dispatch.h"
#include "aot_dbt_direct_edge_dispatch.h"

#include "native_linear_span.h"
#include "aot_residency_sample.h"
#include "guest_address_watch.h"
#include "aot_dbt_call_return_trace.h"
#include "repiu/engine/aot_boundary_provenance.h"
#include "repiu/engine/aot_ff_boundary_attribution.h"
#include "repiu/engine/aot_ff_boundary_target_attribution.h"
#include "repiu/engine/aot_ff_target_timing.h"
#include "repiu/engine/aot_return_stage_profile.h"
#include "repiu/runtime/aot_direct_return_table.h"
#include "execution_internal.h"
#include "guest_memory_access.h"
#include "instruction_emulation.h"

#include <Zydis.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "repiu/platform/guest_cpu_context.h"
#include "repiu/platform/atomic_ops.h"
#include "repiu/platform/host_time.h"
#include "repiu/platform/worker_signal.h"

namespace repiu::engine
{

void BumpAotBoundaryCount(ThreadContext* context)
{
    context->aot_boundary_count.fetch_add(1U, std::memory_order_relaxed);
    if (context->shared_live_telemetry != nullptr)
    {
        repiu::platform::AtomicIncrement(
            &context->shared_live_telemetry->aot_boundary_count);
    }
}

void BumpAotBoundaryReason(ThreadContext* context, AotBoundaryReason reason)
{
    std::atomic<std::uint32_t>* local = nullptr;
    volatile std::int32_t* shared = nullptr;
    SharedLiveTelemetry* telemetry = context->shared_live_telemetry;
    switch (reason)
    {
        case AotBoundaryReason::kReturn:
            local = &context->aot_boundary_return_count;
            shared = telemetry != nullptr ? &telemetry->aot_boundary_return_count
                                          : nullptr;
            break;
        case AotBoundaryReason::kIndirectBranch:
            local = &context->aot_boundary_indirect_count;
            shared = telemetry != nullptr
                         ? &telemetry->aot_boundary_indirect_count
                         : nullptr;
            break;
        case AotBoundaryReason::kDirectBranch:
            local = &context->aot_boundary_direct_count;
            shared = telemetry != nullptr ? &telemetry->aot_boundary_direct_count
                                          : nullptr;
            break;
        case AotBoundaryReason::kConditionalBranch:
            local = &context->aot_boundary_conditional_count;
            shared = telemetry != nullptr
                         ? &telemetry->aot_boundary_conditional_count
                         : nullptr;
            break;
        case AotBoundaryReason::kOther:
            local = &context->aot_boundary_other_count;
            shared = telemetry != nullptr ? &telemetry->aot_boundary_other_count
                                          : nullptr;
            break;
    }
    if (local != nullptr)
    {
        local->fetch_add(1U, std::memory_order_relaxed);
    }
    if (shared != nullptr)
    {
        repiu::platform::AtomicIncrement(shared);
    }
}

void BumpAotReentryCount(ThreadContext* context)
{
    context->aot_reentry_count.fetch_add(1U, std::memory_order_relaxed);
    if (context->shared_live_telemetry != nullptr)
    {
        repiu::platform::AtomicIncrement(
            &context->shared_live_telemetry->aot_reentry_count);
    }
}

void RecordAotOtherBoundarySample(ThreadContext* context,
                                  std::uint32_t guest_eip,
                                  const std::uint8_t* bytes,
                                  std::size_t length)
{
    if (length == 0)
    {
        return;
    }
    const std::uint8_t opcode = bytes[0];
    const std::uint32_t new_count = ++context->aot_other_opcode_histogram[opcode];
    // Task 367: the same sample counted a second way. `bytes[0]` is an escape
    // byte or a prefix for most of this population, so it cannot say which
    // instruction produced the exception.
    RecordAotBoundaryOpcodeSample(
        &context->aot_boundary_opcode_census, bytes, length);
    RecordAotFfBoundarySample(
        &context->aot_ff_boundary_attribution, guest_eip, bytes, length);
    std::uint32_t packed = 0;
    for (std::size_t i = 0; i < 4U && i < length; ++i)
    {
        packed |= static_cast<std::uint32_t>(bytes[i]) << (8U * i);
    }
    context->aot_last_other_boundary_eip.store(guest_eip,
                                               std::memory_order_relaxed);
    context->aot_last_other_boundary_bytes.store(packed,
                                                 std::memory_order_relaxed);
    if (context->shared_live_telemetry != nullptr)
    {
        SharedLiveTelemetry* telemetry = context->shared_live_telemetry;
        repiu::platform::AtomicExchange(&telemetry->aot_last_other_eip,
                            static_cast<long>(guest_eip));
        repiu::platform::AtomicExchange(&telemetry->aot_last_other_bytes,
                            static_cast<long>(packed));
        // Running max: this opcode is the histogram peak so far. Only the guest
        // thread writes these, so a plain read of the mirror is sufficient.
        if (static_cast<std::uint32_t>(telemetry->aot_other_top_opcode_count) <
            new_count)
        {
            repiu::platform::AtomicExchange(&telemetry->aot_other_top_opcode,
                                static_cast<long>(opcode));
            repiu::platform::AtomicExchange(&telemetry->aot_other_top_opcode_count,
                                static_cast<long>(new_count));
        }
    }
}

// Live-mirrored the same way as BumpAotBoundaryCount/BumpAotReentryCount so
// a stuck aot_boundary_guest_eip can be cross-checked against repeated
// page retire/re-resolve activity while dispatch is silent (Task 217).
void BumpAotPageRetireAttemptCount(ThreadContext* context)
{
    context->aot_page_retire_attempt_count.fetch_add(
        1U, std::memory_order_relaxed);
    if (context->shared_live_telemetry != nullptr)
    {
        repiu::platform::AtomicIncrement(
            &context->shared_live_telemetry->aot_page_retire_attempt_count);
    }
}

void BumpAotPageRetireSuccessCount(ThreadContext* context)
{
    context->aot_page_retire_success_count.fetch_add(
        1U, std::memory_order_relaxed);
    if (context->shared_live_telemetry != nullptr)
    {
        repiu::platform::AtomicIncrement(
            &context->shared_live_telemetry->aot_page_retire_success_count);
    }
}

void BumpAotRetiredEntryTrapCount(ThreadContext* context)
{
    context->aot_retired_entry_trap_count.fetch_add(
        1U, std::memory_order_relaxed);
    if (context->shared_live_telemetry != nullptr)
    {
        repiu::platform::AtomicIncrement(
            &context->shared_live_telemetry->aot_retired_entry_trap_count);
    }
}

void BumpAotQuarantineCount(ThreadContext* context)
{
    context->aot_quarantine_count.fetch_add(1U, std::memory_order_relaxed);
    if (context->shared_live_telemetry != nullptr)
    {
        repiu::platform::AtomicIncrement(
            &context->shared_live_telemetry->aot_quarantine_count);
    }
}


// Shared evidence packet for a pathological zero return address (design
// 246): guest stack around ESP, live code bytes around code_center, the
// tracked call frames, and the recent return trace. Fires at most four
// times per process across both call sites.
void DumpZeroReturnEvidence(const repiu::platform::GuestCpuContext* win32_context,
                            ThreadContext* context,
                            const char* reason,
                            std::uint32_t code_center)
{
    static long zero_return_dump_count = 0;
    const long dump_index = repiu::platform::AtomicIncrement(&zero_return_dump_count);
    if (dump_index > 4)
    {
        return;
    }
    fprintf(stderr,
            "[repiu-live-debug] zero return target #%ld reason=%s"
            " eip=0x%08X esp=0x%08X code_center=0x%08X call_depth=%u\n",
            dump_index, reason,
            static_cast<std::uint32_t>(win32_context->Eip),
            static_cast<std::uint32_t>(win32_context->Esp),
            code_center, context->aot_call_depth);
    const std::uint32_t stack_base = win32_context->Esp - 0x20U;
    for (std::uint32_t row = 0; row < 8U; ++row)
    {
        const std::uint32_t row_address = stack_base + row * 0x20U;
        const auto* row_pointer = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(row_address));
        if (!IsGuestRangeReadable(context, row_pointer, 0x20U))
        {
            continue;
        }
        fprintf(stderr,
                "[repiu-live-debug]   stack 0x%08X: %08X %08X %08X %08X"
                " %08X %08X %08X %08X\n",
                row_address,
                row_pointer[0], row_pointer[1], row_pointer[2],
                row_pointer[3], row_pointer[4], row_pointer[5],
                row_pointer[6], row_pointer[7]);
    }
    const std::uint32_t code_base = code_center - 0x40U;
    for (std::uint32_t row = 0; row < 6U; ++row)
    {
        const std::uint32_t row_address = code_base + row * 0x10U;
        const auto* row_pointer = reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(row_address));
        if (!IsGuestRangeReadable(context, row_pointer, 0x10U))
        {
            continue;
        }
        char text[3U * 16U + 1U] = {};
        for (std::uint32_t column = 0; column < 16U; ++column)
        {
            std::snprintf(text + column * 3U, 4U, "%02X ",
                          row_pointer[column]);
        }
        fprintf(stderr, "[repiu-live-debug]   code 0x%08X: %s\n",
                row_address, text);
    }
    const std::uint32_t frame_count = std::min<std::uint32_t>(
        context->aot_call_depth, 8U);
    for (std::uint32_t index = 0; index < frame_count; ++index)
    {
        const ThreadContext::AotCallFrame& frame =
            context->aot_call_frames[context->aot_call_depth - 1U - index];
        fprintf(stderr,
                "[repiu-live-debug]   frame[-%u] source=0x%08X"
                " target=0x%08X fallthrough=0x%08X\n",
                index, frame.source, frame.target, frame.fallthrough);
    }
    const std::uint32_t trace_count = std::min<std::uint32_t>(
        context->aot_return_trace_count, 8U);
    for (std::uint32_t index = 0; index < trace_count; ++index)
    {
        const std::uint32_t slot =
            (context->aot_return_trace_count - 1U - index) %
            kAotReturnTraceCapacity;
        const AotReturnTraceEntry& entry =
            context->aot_return_trace[slot];
        fprintf(stderr,
                "[repiu-live-debug]   ret[-%u] source=0x%08X actual=0x%08X"
                " expected=0x%08X esp=0x%08X match=%d\n",
                index, entry.source, entry.actual_target,
                entry.expected_target, entry.esp, entry.matches ? 1 : 0);
    }
}

void BuildAotSegmentTable(ThreadContext* context,
                               AotSegmentTable* table)
{
    if (context == nullptr || table == nullptr)
    {
        return;
    }
    *table = AotSegmentTable{};
    const std::uint16_t selectors[6] = {
        context->guest_es, 0U, context->guest_ss,
        context->guest_ds, context->guest_fs, context->guest_gs};
    const std::uint32_t addresses[6] = {
        static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&context->guest_es)),
        0U,
        static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&context->guest_ss)),
        static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&context->guest_ds)),
        static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&context->guest_fs)),
        static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&context->guest_gs))};
    for (std::uint8_t seg = 0; seg < 6U; ++seg)
    {
        if (seg == 1U)
        {
            continue; // CS has no shadow
        }
        BuildAotSegmentResolution(
            context->selector_table, addresses[seg], selectors[seg],
            &table->segments[seg]);
    }
}

void RecordAotBreakpointProvenance(
    ThreadContext* context,
    AotCacheBreakpointProvenance provenance)
{
    if (context == nullptr)
    {
        return;
    }
    std::uint32_t index = static_cast<std::uint32_t>(provenance);
    if (index >= kAotCacheBreakpointProvenanceCount)
    {
        index = static_cast<std::uint32_t>(
            AotCacheBreakpointProvenance::kUnknown);
    }
    context->aot_breakpoint_provenance_counts[index].fetch_add(
        1U, std::memory_order_relaxed);
    if (context->shared_live_telemetry != nullptr)
    {
        repiu::platform::AtomicIncrement(
            &context->shared_live_telemetry
                 ->aot_breakpoint_provenance_counts[index]);
    }
}

bool SameAotSegmentResolution(
    const AotSegmentResolution& left,
    const AotSegmentResolution& right)
{
    return left.shadow_address == right.shadow_address &&
        left.selector == right.selector && left.base == right.base &&
        left.limit == right.limit && left.flags == right.flags &&
        left.policy == right.policy;
}

void ReResolveAotSegmentOverrides(ThreadContext* context)
{
    if (context == nullptr || context->aot_placement == nullptr ||
        (context->aot_placement->segment_override_sites.empty() &&
         context->aot_placement->guarded_segment_pop_sites.empty()))
    {
        return;
    }
    AotSegmentTable table{};
    BuildAotSegmentTable(context, &table);
    // Only pay the cache re-protect/flush when the complete selector descriptor
    // changes. Selector-only gating leaves stale folded bases after DPMI updates.
    bool changed = !context->aot_segment_resolutions_initialized;
    for (std::uint8_t seg = 0; seg < 6U; ++seg)
    {
        if (!SameAotSegmentResolution(
                table.segments[seg], context->aot_resolved_segments[seg]))
        {
            changed = true;
            break;
        }
    }
    if (!changed)
    {
        return;
    }
    AotSegmentPatchStats stats{};
    if (ReResolveWin32AotSegmentOverrides(
            context->aot_placement, &table, &stats) != 0U)
    {
        for (std::uint8_t seg = 0; seg < 6U; ++seg)
        {
            context->aot_resolved_segments[seg] = table.segments[seg];
        }
        context->aot_segment_resolutions_initialized = true;
        context->aot_selector_guard_native_site_count.fetch_add(
            stats.native_site_count, std::memory_order_relaxed);
        context->aot_selector_guard_hle_site_count.fetch_add(
            stats.hle_site_count, std::memory_order_relaxed);
        context->aot_selector_guard_unresolved_site_count.fetch_add(
            stats.unresolved_site_count, std::memory_order_relaxed);
    }
}

int AotTranslationWorkerProc(void* parameter)
{
    ThreadContext* context = static_cast<ThreadContext*>(parameter);
    if (context == nullptr || context->aot_translation_request_event == nullptr ||
        context->aot_translation_complete_event == nullptr)
    {
        return 1;
    }
    for (;;)
    {
        if (!repiu::platform::WaitForWorkerSignal(
                context->aot_translation_request_event))
        {
            return 2;
        }
        // Task 327: T1, taken before anything else so the wake latency it ends
        // contains only scheduling. Shutdown accumulates nothing.
        const std::uint64_t wake_cycles = ReadAotWorkerTimingCycles();
        if (context->aot_translation_shutdown.load(std::memory_order_acquire))
        {
            return 0;
        }
        AotWorkerTimingProfile* worker_timing =
            context->aot_worker_timing.get();
        const auto operation = static_cast<AotWorkerOperation>(
            context->aot_worker_operation.load(std::memory_order_acquire));
        if (operation != AotWorkerOperation::kTranslate)
        {
            RecordAotWorkerOtherOperation(worker_timing);
        }
        else
        {
            RecordAotWorkerWake(worker_timing, wake_cycles);
        }
        if (operation == AotWorkerOperation::kPatchInlineCache)
        {
            context->aot_inline_cache_patch_result =
                AotInlineCachePatchResult{};
            PatchAotIndirectInlineCache(
                context->aot_placement,
                context->aot_patch_cache_miss_address.load(
                    std::memory_order_acquire),
                context->aot_patch_guest_target.load(
                    std::memory_order_acquire),
                context->aot_patch_cache_target.load(
                    std::memory_order_acquire),
                &context->aot_inline_cache_patch_result);
        }
        else if (operation == AotWorkerOperation::kRetireGuestPage)
        {
            context->aot_guest_page_retire_result =
                AotGuestPageRetireResult{};
            RetireAotGuestPage(
                context->aot_placement,
                context->aot_retire_guest_page.load(
                    std::memory_order_acquire),
                context->aot_retire_quarantine.load(
                    std::memory_order_acquire),
                &context->aot_guest_page_retire_result);
        }
        else
        {
            const std::uint32_t target = context->aot_translation_target.load(
                std::memory_order_acquire);
            context->aot_translation_result = AotDynamicAppendResult{};
            // Task 264 Phase 3a: resolve each shadow segment register so the
            // translator can fold the base into segment-override accesses.
            AotSegmentTable segment_table{};
            const std::uint64_t segment_table_start =
                ReadAotWorkerTimingCycles();
            BuildAotSegmentTable(context, &segment_table);
            const std::uint64_t append_start = ReadAotWorkerTimingCycles();
            RecordAotWorkerSegmentTable(
                worker_timing,
                AotWorkerTimingDelta(
                    worker_timing, segment_table_start, append_start));
            AppendDynamicAotTranslation(
                context->runtime_base, context->runtime_size, target,
                context->aot_excluded_guest_ranges,
                &context->aot_page_write_watch, context->aot_placement,
                &segment_table, &context->aot_translation_result,
                worker_timing);
            RecordAotWorkerAppend(
                worker_timing,
                AotWorkerTimingDelta(
                    worker_timing, append_start,
                    ReadAotWorkerTimingCycles()));
            if (context->aot_translation_result.unsafe_failure)
            {
                context->aot_terminal_failure.store(
                    true, std::memory_order_release);
            }
        }
        // Task 327: T2 immediately before the completion signal, so the
        // complete latency it anchors contains only scheduling.
        RecordAotWorkerCompleteSignal(
            worker_timing, ReadAotWorkerTimingCycles());
        repiu::platform::SignalWorker(
            context->aot_translation_complete_event);
    }
}

bool RequestAotDynamicTranslation(ThreadContext* context,
                                  std::uint32_t target,
                                  std::uint32_t* cache_entry,
                                  std::uint32_t* added_bytes)
{
    if (context == nullptr || cache_entry == nullptr || added_bytes == nullptr ||
        !context->aot_translation_thread.valid ||
        context->aot_translation_request_event == nullptr ||
        context->aot_translation_complete_event == nullptr)
    {
        return false;
    }
    repiu::platform::ResetWorkerSignal(
        context->aot_translation_complete_event);
    context->aot_worker_operation.store(
        static_cast<std::uint32_t>(AotWorkerOperation::kTranslate),
        std::memory_order_release);
    context->aot_translation_target.store(target, std::memory_order_release);
    // Task 327: T0 must be taken immediately before the signal, or the wake
    // latency it anchors would absorb setup work instead.
    AotWorkerTimingProfile* worker_timing =
        context->aot_worker_timing.get();
    const std::uint64_t request_cycles =
        worker_timing != nullptr ? ReadAotWorkerTimingCycles() : 0U;
    RecordAotWorkerRequestSignal(worker_timing, request_cycles);
    if (!repiu::platform::SignalWorker(
            context->aot_translation_request_event) ||
        !repiu::platform::WaitForWorkerSignal(
            context->aot_translation_complete_event))
    {
        context->aot_terminal_failure.store(true, std::memory_order_release);
        return false;
    }
    if (worker_timing != nullptr)
    {
        RecordAotWorkerGuestResume(
            worker_timing, request_cycles, ReadAotWorkerTimingCycles());
    }
    if (context->aot_translation_result.unsafe_failure)
    {
        context->aot_terminal_failure.store(true, std::memory_order_release);
        return false;
    }
    if (!context->aot_translation_result.appended)
    {
        return false;
    }
    *cache_entry = context->aot_translation_result.cache_entry;
    *added_bytes = context->aot_translation_result.added_bytes;
    return true;
}

void ReleaseUnneededAotGuestPageWatches(ThreadContext* context,
                                             std::uint32_t address,
                                             std::uint32_t size)
{
    if (context == nullptr || context->aot_placement == nullptr) return;

    constexpr std::uint32_t kPageMask = 0xFFFFF000U;
    const std::uint32_t first_page = address & kPageMask;
    const std::uint64_t end = static_cast<std::uint64_t>(address) + size;
    const std::uint32_t last_page = static_cast<std::uint32_t>((end - 1U) & kPageMask);

    for (std::uint32_t page = first_page; page <= last_page; page += 0x1000U)
    {
        bool relevant = AotGuestRangeHasActiveTranslation(
            *context->aot_placement, page, 0x1000U);
        if (!relevant)
        {
            relevant = IsAotGuestPageRetired(*context->aot_placement, page) ||
                       IsAotGuestPageQuarantined(*context->aot_placement, page);
        }
        if (!relevant)
        {
            RemoveAotPageWriteWatch(&context->aot_page_write_watch, page);
        }
    }
}

bool HandleAotGuestCodeWriteCompletion(
    const repiu::platform::FaultEvent& fault, ThreadContext* context)
{
    repiu::platform::GuestCpuContext* win32_context = fault.registers;
    // Task 326 handler-axis attribution. Function scope, so every early return
    // is covered.
    const ExecutionTimeScope write_completion_time_scope(
        context != nullptr ? context->execution_time_profile.get() : nullptr,
        ExecutionTimeBucket::kAotWriteCompletion);
    if (win32_context == nullptr || context == nullptr ||
        !HasPendingAotGuestWrite(context->aot_page_write_watch) ||
        fault.kind != repiu::platform::FaultKind::kSingleStep)
    {
        return false;
    }
    AotGuestWriteCompletion completion;
    if (!CompleteAotGuestWrite(
            &context->aot_page_write_watch, &completion) ||
        !NoteSuccessfulAotGuestWrite(
            context, completion.destination, completion.byte_count))
    {
        context->aot_terminal_failure.store(true, std::memory_order_release);
        return false;
    }
    ReleaseUnneededAotGuestPageWatches(context, completion.destination, completion.byte_count);
    if (completion.keep_single_step ||
        (completion.from_guest && context->aot_reentry_pending))
    {
        win32_context->EFlags |= 0x00000100U;
    }
    else
    {
        win32_context->EFlags &= ~0x00000100U;
    }
    return true;
}

bool HandleAotGuestCodeWriteFault(const repiu::platform::FaultEvent& fault,
                                  ThreadContext* context)
{
    repiu::platform::GuestCpuContext* win32_context = fault.registers;
    const ExecutionTimeScope write_fault_time_scope(
        context != nullptr ? context->execution_time_profile.get() : nullptr,
        ExecutionTimeBucket::kAotWriteFault);
    if (win32_context == nullptr || context == nullptr ||
        context->aot_placement == nullptr ||
        fault.kind != repiu::platform::FaultKind::kAccessViolation ||
        !fault.access.valid || !fault.access.write_access)
    {
        return false;
    }
    const std::uintptr_t destination_value = fault.access.fault_address;
    if (destination_value > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    const std::uint32_t destination =
        static_cast<std::uint32_t>(destination_value);
    if (!IsAotGuestPageWriteWatched(
            context->aot_page_write_watch, destination))
    {
        return false;
    }
    const std::uint32_t execution_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    const bool from_guest = IsGuestInstructionPointer(
        context, execution_address);
    if (!from_guest && !IsAotCacheAddress(context, execution_address))
    {
        return false;
    }
    const bool keep_single_step =
        (win32_context->EFlags & 0x00000100U) != 0U ||
        context->enable_single_step_trace ||
        context->aot_reentry_pending || context->aot_legacy_fallback;
    if (!BeginAotGuestWrite(
            &context->aot_page_write_watch, execution_address, destination,
            from_guest, keep_single_step,
            AotGuestAddressForExecutionAddress(context, execution_address)))
    {
        context->aot_terminal_failure.store(true, std::memory_order_release);
        return false;
    }
    context->exception_dispatch_last_eip.store(
        execution_address, std::memory_order_relaxed);
    win32_context->EFlags |= 0x00000100U;
    return true;
}

bool AotInlineCachePatchOnGuestThreadEnabled()
{
    // On by default. A pumpit2 A/B with vsync off measured 69.3 against 107.2
    // frames per second, and swaps per guest cycle and primitives per cycle
    // agreed at +54.8% and +51.1%. The runs did the same work per frame -- 356.9
    // against 346.0 patches, 560.4 against 547.3 primitives -- so only the unit
    // price moved, and the worker's other-operation count fell from 1,728,404 to
    // 55. An explicit `0|off|false` restores the worker round trip as a control.
    static const bool enabled = repiu::runtime::ResolvePromotedToggle(
        std::getenv("REPIU_AOT_INLINE_CACHE_PATCH_INLINE"));
    return enabled;
}

// Task 445: the patch on the guest thread, with no worker round trip.
//
// A pumpit2 position census put **34.1% of the guest thread's samples** inside
// this function's wait, against 1,721,010 patches and only 390 translations --
// 385 patches per frame, each a kernel event round trip for fourteen bytes.
//
// Task 190 gave two reasons for the worker, and neither requires it. Its "the
// guest waits, so it never executes a half-patched slot" holds automatically
// when the guest is the one patching: it is not executing the cache then. Its
// W^X rule matters only if two threads can touch the cache at once, and none
// can -- every worker request signals and then blocks the guest on
// `WaitForSingleObject(INFINITE)`, so the worker only ever runs while the guest
// is parked. The handshake *is* the mutual exclusion, and it survives moving the
// patch here: still exactly one thread mutating at a time.
bool PatchAotInlineCacheOnGuestThread(ThreadContext* context,
                                      std::uint32_t cache_miss_address,
                                      std::uint32_t guest_target,
                                      std::uint32_t cache_target)
{
    context->aot_inline_cache_patch_result = AotInlineCachePatchResult{};
    PatchAotIndirectInlineCache(
        context->aot_placement, cache_miss_address, guest_target, cache_target,
        &context->aot_inline_cache_patch_result);
    ++context->aot_inline_cache_direct_patch_count;
    return context->aot_inline_cache_patch_result.patched;
}

bool RequestAotInlineCachePatch(ThreadContext* context,
                                std::uint32_t cache_miss_address,
                                std::uint32_t guest_target,
                                std::uint32_t cache_target)
{
    if (context == nullptr || !context->aot_translation_thread.valid ||
        context->aot_translation_request_event == nullptr ||
        context->aot_translation_complete_event == nullptr)
    {
        return false;
    }
    if (AotInlineCachePatchOnGuestThreadEnabled())
    {
        return PatchAotInlineCacheOnGuestThread(
            context, cache_miss_address, guest_target, cache_target);
    }
    ++context->aot_inline_cache_worker_patch_count;
    repiu::platform::ResetWorkerSignal(
        context->aot_translation_complete_event);
    context->aot_patch_cache_miss_address.store(
        cache_miss_address, std::memory_order_release);
    context->aot_patch_guest_target.store(guest_target,
                                           std::memory_order_release);
    context->aot_patch_cache_target.store(cache_target,
                                           std::memory_order_release);
    context->aot_worker_operation.store(
        static_cast<std::uint32_t>(AotWorkerOperation::kPatchInlineCache),
        std::memory_order_release);
    if (!repiu::platform::SignalWorker(
            context->aot_translation_request_event) ||
        !repiu::platform::WaitForWorkerSignal(
            context->aot_translation_complete_event))
    {
        context->aot_terminal_failure.store(true, std::memory_order_release);
        return false;
    }
    return context->aot_inline_cache_patch_result.patched;
}

// Task 415. A failed re-translation used to quarantine the entry's whole page
// permanently, which drops every other routine on that page to single-stepping:
// pumpit3's one failure per run (`0x0301DFFE`, page `0x0301D000`) left 74% of
// re-entries rejected and 473,674 single steps. What failed is one entry, and it
// failed because its instruction straddles a page boundary into a retired
// neighbour. Remembering the address is enough to stop the retry storm that
// quarantine existed to prevent. Guest-thread only, so no locking, and kept in
// this translation unit so no widely included header changes.
// See docs/design/20260804-415-generation-failure-address-scope.md.
namespace
{

constexpr std::size_t kAotGenerationFailureAddressCapacity = 256U;

std::vector<std::uint32_t>& AotGenerationFailureAddresses()
{
    static std::vector<std::uint32_t> addresses;
    return addresses;
}

bool AotQuarantineOnGenerationFailureEnabled()
{
    static const bool enabled = [] {
        const char* value =
            std::getenv("REPIU_AOT_QUARANTINE_ON_GENERATION_FAILURE");
        return value != nullptr && std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

bool HasAotGenerationFailureAddress(std::uint32_t address)
{
    const std::vector<std::uint32_t>& addresses =
        AotGenerationFailureAddresses();
    return std::find(addresses.begin(), addresses.end(), address) !=
        addresses.end();
}

}  // namespace

std::uint32_t AotGenerationFailureAddressCount()
{
    return static_cast<std::uint32_t>(AotGenerationFailureAddresses().size());
}

std::uint32_t& AotGenerationFailureSkipCount()
{
    static std::uint32_t count = 0;
    return count;
}

std::uint32_t& AotGenerationFailureQuarantineCount()
{
    static std::uint32_t count = 0;
    return count;
}

std::uint32_t& AotSpanningEntryActivationCount()
{
    static std::uint32_t count = 0;
    return count;
}

bool RequestAotGuestPageRetirement(ThreadContext* context,
                                   std::uint32_t guest_page,
                                   bool quarantine)
{
    if (context == nullptr || !context->aot_translation_thread.valid ||
        context->aot_translation_request_event == nullptr ||
        context->aot_translation_complete_event == nullptr)
    {
        return false;
    }
    repiu::platform::ResetWorkerSignal(
        context->aot_translation_complete_event);
    context->aot_retire_guest_page.store(
        guest_page, std::memory_order_release);
    context->aot_retire_quarantine.store(
        quarantine, std::memory_order_release);
    context->aot_worker_operation.store(
        static_cast<std::uint32_t>(AotWorkerOperation::kRetireGuestPage),
        std::memory_order_release);
    if (!repiu::platform::SignalWorker(
            context->aot_translation_request_event) ||
        !repiu::platform::WaitForWorkerSignal(
            context->aot_translation_complete_event))
    {
        context->aot_terminal_failure.store(true, std::memory_order_release);
        return false;
    }
    if (context->aot_guest_page_retire_result.guard_reset_count != 0U)
    {
        context->aot_inline_cache_guard_reset_count.fetch_add(
            context->aot_guest_page_retire_result.guard_reset_count,
            std::memory_order_relaxed);
    }
    return context->aot_guest_page_retire_result.retired;
}

std::uint32_t AotGuestAddressForExecutionAddress(
    const ThreadContext* context,
    std::uint32_t execution_address)
{
    if (context == nullptr)
    {
        return 0U;
    }
    if (IsGuestInstructionPointer(context, execution_address))
    {
        return execution_address;
    }
    std::uint32_t guest_address = 0U;
    if (context->aot_placement != nullptr &&
        FindAotGuestAddress(*context->aot_placement,
                            execution_address, &guest_address))
    {
        return guest_address;
    }
    return 0U;
}


bool IsAotInlineCacheMiss(const ThreadContext* context,
                          std::uint32_t cache_address)
{
    if (context == nullptr || context->aot_placement == nullptr ||
        cache_address < context->aot_placement->base_address)
    {
        return false;
    }
    AotCodeCachePlacement* placement = context->aot_placement;
    const std::uint32_t offset = cache_address - placement->base_address;
    // Task 479. This test runs immediately before every patch attempt and on
    // every return dispatch that turns out not to be one, and a "no" answer
    // streamed all 8,019 sites -- the worst case of the same scan the patch path
    // carried. Unlike the patch path, a "not found" from the index is trusted
    // here rather than confirmed by the scan: "no" is the common answer, so
    // confirming it would reinstate exactly the cost being removed. Anything the
    // index cannot answer still falls through to the scan below.
    EnsureAotInlineCacheSiteIndex(placement);
    const AotInlineCacheSiteLookup indexed =
        LookupAotInlineCacheSiteIndex(*placement, offset);
    if (indexed.usable)
    {
        if (!indexed.found)
        {
            ++placement->inline_cache_site_index.lookup_count;
            return false;
        }
        if (indexed.site_index <
            placement->indirect_inline_cache_sites.size())
        {
            const runtime::AotIndirectInlineCacheSite& site =
                placement->indirect_inline_cache_sites[indexed.site_index];
            if (offset == site.miss_cache_offset ||
                offset == site.miss_cache_offset + 1U)
            {
                ++placement->inline_cache_site_index.lookup_count;
                return true;
            }
        }
    }
    ++placement->inline_cache_site_index.fallback_scan_count;
    for (const auto& site : placement->indirect_inline_cache_sites)
    {
        if (offset == site.miss_cache_offset ||
            offset == site.miss_cache_offset + 1U)
        {
            return true;
        }
    }
    return false;
}

bool IsAotHleBoundaryAddress(const ThreadContext* context,
                             std::uint32_t guest_address)
{
    // Task 326. The context is const because this is a query, but the profile
    // is observation state rather than execution state.
    const ExecutionTimeScope hle_boundary_scan_time_scope(
        context != nullptr ? context->execution_time_profile.get() : nullptr,
        ExecutionTimeBucket::kAotHleBoundaryScan);
    if (context == nullptr)
    {
        return false;
    }
    for (const runtime::AotExcludedGuestRange& range :
         context->aot_excluded_guest_ranges)
    {
        const std::uint64_t end =
            static_cast<std::uint64_t>(range.guest_address) +
            range.byte_count;
        if (range.byte_count != 0U && guest_address >= range.guest_address &&
            guest_address < end)
        {
            return true;
        }
    }
    return false;
}

// Task 404: one failed re-translation quarantines a guest page for the rest of
// the run, and on pumpit3 the page it takes carries the 200-iteration I/O delay
// loop. `AppendDynamicAotTranslation` names the cause in `message` and six
// different causes are possible, so the string is what decides where the fix
// belongs. Guest thread only, called from the VEH path.
static void RecordAotGenerationFailure(ThreadContext* context,
                                       std::uint32_t target,
                                       bool quarantined)
{
    if (context->generation_failure_trace_count >=
        ThreadContext::kGenerationFailureTraceCapacity)
    {
        ++context->generation_failure_trace_overflow;
        return;
    }
    auto& entry =
        context->generation_failure_trace[
            context->generation_failure_trace_count++];
    entry.target = target;
    entry.page = AotGuestPage(target);
    entry.quarantined = quarantined;
    entry.terminal =
        context->aot_terminal_failure.load(std::memory_order_acquire);
    const std::string& message = context->aot_translation_result.message;
    const std::size_t copied = std::min<std::size_t>(
        message.size(), ThreadContext::kGenerationFailureMessageCapacity - 1U);
    std::memcpy(entry.message, message.c_str(), copied);
    entry.message[copied] = '\0';
}

// Task 581: the resolver body, separated so the watch in the wrapper below has
// one place to observe both the request and its outcome. The alternative was a
// record call beside each of the five success returns scattered through it.
namespace
{

bool ResolveAotTransferTargetBody(ThreadContext* context,
                                  std::uint32_t target,
                                  std::uint32_t* cache_target,
                                  bool force_generation,
                                  AotRetiredTrapResolution* retired_resolution)
{
    const ExecutionTimeScope transfer_resolve_time_scope(
        context != nullptr ? context->execution_time_profile.get() : nullptr,
        ExecutionTimeBucket::kAotTransferResolve);
    if (retired_resolution != nullptr)
    {
        *retired_resolution = AotRetiredTrapResolution::kFallback;
    }
    if (context == nullptr || cache_target == nullptr ||
        context->aot_placement == nullptr)
    {
        return false;
    }
    if (ResolveGlideGateDirectTarget(
            context, target, cache_target))
    {
        return true;
    }
    if (IsAotHleBoundaryAddress(context, target))
    {
        return false;
    }
    if (IsAotGuestPageQuarantined(
            *context->aot_placement, target))
    {
        if (retired_resolution != nullptr)
        {
            *retired_resolution = AotRetiredTrapResolution::kQuarantined;
        }
        return false;
    }
    if (IsAotCacheAddress(context, target) ||
        FindAotCacheAddress(*context->aot_placement, target, cache_target))
    {
        if (retired_resolution != nullptr)
        {
            *retired_resolution = AotRetiredTrapResolution::kActiveHit;
        }
        return true;
    }
    const bool retired_target = force_generation ||
        IsAotGuestPageRetired(*context->aot_placement, target) ||
        HasAotRetiredGuestAddress(*context->aot_placement, target);
    // Task 415: an address whose generation already failed is never attempted
    // again. This is the property quarantine was providing -- no retry storm --
    // without taking the rest of the page down with it.
    if (retired_target && HasAotGenerationFailureAddress(target))
    {
        ++AotGenerationFailureSkipCount();
        if (retired_resolution != nullptr)
        {
            *retired_resolution = AotRetiredTrapResolution::kGenerationFailure;
        }
        return false;
    }
    std::uint32_t dynamic_cache_entry = 0;
    std::uint32_t dynamic_added_bytes = 0;
    // Tasks 425 and 426: this dispatcher is reached only with a placed AOT
    // cache, and the trampoline's four non-AOT entry points hard-code a null
    // placement and `kLegacy` at the call site, so the backend here is always
    // `dynamic`. The guard that used to wrap this increment, and the
    // `(!dynamic_translation && !retired_target)` disjunct that used to lead
    // the request below, both existed for the static-only `aot` backend and
    // were always taken. Neither the count nor the call frequency changes.
    context->aot_dynamic_attempt_count.fetch_add(
        1, std::memory_order_relaxed);
    bool dynamic_translation_failed = false;
    {
        const ExecutionTimeScope dynamic_translate_time_scope(
            context->execution_time_profile.get(),
            ExecutionTimeBucket::kAotDynamicTranslate);
        dynamic_translation_failed =
            !RequestAotDynamicTranslation(
                context, target, &dynamic_cache_entry, &dynamic_added_bytes);
    }
    if (dynamic_translation_failed)
    {
        if (retired_target)
        {
            if (retired_resolution != nullptr)
            {
                *retired_resolution =
                    AotRetiredTrapResolution::kGenerationFailure;
            }
            context->aot_generation_failure_count.fetch_add(
                1, std::memory_order_relaxed);
            bool quarantined = false;
            // Task 415: remember the address instead of quarantining its page.
            // The page keeps serving every other entry from the cache, and the
            // skip above makes sure this address is never retried. The old
            // behaviour returns when the switch is set, or when failures spread
            // wider than this policy was designed for.
            std::vector<std::uint32_t>& failures =
                AotGenerationFailureAddresses();
            const bool fall_back_to_quarantine =
                AotQuarantineOnGenerationFailureEnabled() ||
                failures.size() >= kAotGenerationFailureAddressCapacity;
            if (!fall_back_to_quarantine)
            {
                if (!HasAotGenerationFailureAddress(target))
                {
                    failures.push_back(target);
                }
            }
            else if (!context->aot_terminal_failure.load(
                         std::memory_order_acquire) &&
                     RequestAotGuestPageRetirement(context, target, true))
            {
                ++AotGenerationFailureQuarantineCount();
                BumpAotQuarantineCount(context);
                quarantined = true;
            }
            else
            {
                context->aot_terminal_failure.store(
                    true, std::memory_order_release);
            }
            // Task 404: the reason lives in the append result and was being
            // discarded. Recorded after the outcome so one entry says both what
            // failed and what the policy did about it.
            RecordAotGenerationFailure(context, target, quarantined);
        }
        return false;
    }
    context->aot_dynamic_success_count.fetch_add(
        1, std::memory_order_relaxed);
    context->aot_dynamic_added_bytes.fetch_add(
        dynamic_added_bytes, std::memory_order_relaxed);
    if (retired_target)
    {
        if (retired_resolution != nullptr)
        {
            *retired_resolution =
                AotRetiredTrapResolution::kGenerationPublished;
        }
        context->aot_generation_publish_count.fetch_add(
            1, std::memory_order_relaxed);
        context->aot_generation_relinked_entry_count.fetch_add(
            context->aot_translation_result.relinked_entry_count,
            std::memory_order_relaxed);
        context->aot_last_published_generation.store(
            context->aot_translation_result.generation,
            std::memory_order_relaxed);
    }
    *cache_target = dynamic_cache_entry;
    return true;
}

}  // namespace

bool ResolveAotTransferTarget(ThreadContext* context,
                              std::uint32_t target,
                              std::uint32_t* cache_target,
                              bool force_generation,
                              AotRetiredTrapResolution* retired_resolution)
{
    RecordGuestAddressWatch(
        GuestAddressWatchEvent::kDispatchRequest, target, target);
    const bool resolved = ResolveAotTransferTargetBody(
        context, target, cache_target, force_generation, retired_resolution);
    if (resolved)
    {
        RecordGuestAddressWatch(
            GuestAddressWatchEvent::kCacheEntry,
            target,
            cache_target != nullptr ? *cache_target : 0U);
    }
    return resolved;
}

bool EvaluateAotCondition(std::uint8_t condition, std::uint32_t eflags)
{
    const bool carry = (eflags & 0x00000001U) != 0U;
    const bool parity = (eflags & 0x00000004U) != 0U;
    const bool zero = (eflags & 0x00000040U) != 0U;
    const bool sign = (eflags & 0x00000080U) != 0U;
    const bool overflow = (eflags & 0x00000800U) != 0U;
    switch (condition & 0x0FU)
    {
        case 0x0U: return overflow;
        case 0x1U: return !overflow;
        case 0x2U: return carry;
        case 0x3U: return !carry;
        case 0x4U: return zero;
        case 0x5U: return !zero;
        case 0x6U: return carry || zero;
        case 0x7U: return !carry && !zero;
        case 0x8U: return sign;
        case 0x9U: return !sign;
        case 0xAU: return parity;
        case 0xBU: return !parity;
        case 0xCU: return sign != overflow;
        case 0xDU: return sign == overflow;
        case 0xEU: return zero || sign != overflow;
        case 0xFU: return !zero && sign == overflow;
    }
    return false;
}

bool HandleAotConditionalTransfer(const repiu::platform::FaultEvent& fault,
                                  ThreadContext* context)
{
    repiu::platform::GuestCpuContext* win32_context = fault.registers;
    const ExecutionTimeScope conditional_time_scope(
        context != nullptr ? context->execution_time_profile.get() : nullptr,
        ExecutionTimeBucket::kAotConditional);
    if (win32_context == nullptr || context == nullptr ||
        context->aot_placement == nullptr || !context->aot_reentry_pending ||
        fault.kind != repiu::platform::FaultKind::kBreakpoint)
    {
        return false;
    }
    const std::uint32_t source = static_cast<std::uint32_t>(win32_context->Eip);
    const auto* instruction = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(source));
    std::uint8_t condition = 0;
    std::uint32_t instruction_size = 0;
    std::int32_t displacement = 0;
    if (instruction[0] >= 0x70U && instruction[0] <= 0x7FU)
    {
        condition = instruction[0] & 0x0FU;
        instruction_size = 2U;
        displacement = static_cast<std::int8_t>(instruction[1]);
    }
    else if (instruction[0] == 0x0FU && instruction[1] >= 0x80U &&
             instruction[1] <= 0x8FU)
    {
        condition = instruction[1] & 0x0FU;
        instruction_size = 6U;
        std::memcpy(&displacement, instruction + 2U, sizeof(displacement));
    }
    else
    {
        return false;
    }
    const bool taken = EvaluateAotCondition(
        condition, static_cast<std::uint32_t>(win32_context->EFlags));
    const std::uint32_t target = taken
        ? source + instruction_size + displacement
        : source + instruction_size;
    std::uint32_t cache_target = target;
    if (!ResolveAotTransferTarget(context, target, &cache_target))
    {
        context->aot_last_indirect_source.store(source,
                                                 std::memory_order_relaxed);
        context->aot_last_indirect_target.store(target,
                                                 std::memory_order_relaxed);
        return false;
    }
    win32_context->Eip = cache_target;
    win32_context->EFlags &= ~0x00000100U;
    context->aot_reentry_pending = false;
    context->aot_legacy_fallback = false;
    context->enable_single_step_trace = false;
    context->aot_indirect_dispatch_count.fetch_add(1, std::memory_order_relaxed);
    context->aot_transfer_trace[
        context->aot_transfer_trace_count % kAotTransferTraceCapacity] = {
            source, target, false};
    ++context->aot_transfer_trace_count;
    context->aot_last_indirect_source.store(source, std::memory_order_relaxed);
    context->aot_last_indirect_target.store(target, std::memory_order_relaxed);
    AccumulateAotResidency(context, target);
    BumpAotReentryCount(context);
    return true;
}

bool HandleAotIndirectTransfer(const repiu::platform::FaultEvent& fault,
                               ThreadContext* context,
                               AotDbtDispatchFallbackReason* fallback_reason,
                               AotTransferOrigin origin)
{
    repiu::platform::GuestCpuContext* win32_context = fault.registers;
    const ExecutionTimeScope indirect_time_scope(
        context != nullptr ? context->execution_time_profile.get() : nullptr,
        ExecutionTimeBucket::kAotIndirect);
    if (fallback_reason != nullptr)
    {
        *fallback_reason = AotDbtDispatchFallbackReason::kUnknown;
    }
    if (win32_context == nullptr || context == nullptr ||
        context->aot_placement == nullptr ||
        !context->aot_reentry_pending ||
        fault.kind != repiu::platform::FaultKind::kBreakpoint)
    {
        if (fallback_reason != nullptr)
        {
            *fallback_reason = AotDbtDispatchFallbackReason::kInvalidState;
        }
        return false;
    }
    const std::uint32_t source = static_cast<std::uint32_t>(win32_context->Eip);
    const auto* instruction = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(source));
    bool is_call = false;
    std::uint32_t target = 0;
    std::uint32_t instruction_size = 0;
    if (instruction[0] == 0xE8U || instruction[0] == 0xE9U)
    {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, instruction + 1, sizeof(displacement));
        instruction_size = 5U;
        target = source + instruction_size + displacement;
        is_call = instruction[0] == 0xE8U;
    }
    else if (instruction[0] == 0xEBU)
    {
        instruction_size = 2U;
        target = source + instruction_size +
            static_cast<std::int8_t>(instruction[1]);
    }
    else if (instruction[0] != 0xFFU)
    {
        if (fallback_reason != nullptr)
        {
            *fallback_reason =
                AotDbtDispatchFallbackReason::kInvalidInstruction;
        }
        return false;
    }
    else
    {
        const std::uint8_t operation = (instruction[1] >> 3) & 0x07U;
        is_call = operation == 2U;
        if (!is_call && operation != 4U)
        {
            if (fallback_reason != nullptr)
            {
                *fallback_reason =
                    AotDbtDispatchFallbackReason::kInvalidInstruction;
            }
            return false;
        }
        const std::uint8_t mod = instruction[1] >> 6;
        const std::uint8_t rm = instruction[1] & 0x07U;
        instruction_size = 2U;
        if (mod == 3U)
        {
            target = ReadGeneralRegister32(win32_context, rm);
        }
        else
        {
            std::uint32_t pointer_address = 0;
            if (!DecodeModRmMemoryAddress(win32_context, instruction,
                                          &pointer_address,
                                          &instruction_size) ||
                !ReadGuestUInt32(
                    context,
                    reinterpret_cast<const void*>(
                        static_cast<std::uintptr_t>(pointer_address)),
                    &target))
            {
                if (fallback_reason != nullptr)
                {
                    *fallback_reason =
                        AotDbtDispatchFallbackReason::kUnreadableSource;
                }
                return false;
            }
        }
    }
    std::uint32_t cache_target = target;
    AotDbtDispatchFallbackReason target_failure =
        AotDbtDispatchFallbackReason::kTranslationFailure;
    if (target == 0U)
    {
        target_failure = AotDbtDispatchFallbackReason::kZeroTarget;
    }
    else if (IsAotHleBoundaryAddress(context, target))
    {
        target_failure = AotDbtDispatchFallbackReason::kHleTarget;
    }
    else if (IsAotGuestPageQuarantined(
                 *context->aot_placement, target))
    {
        target_failure = AotDbtDispatchFallbackReason::kQuarantinedTarget;
    }
    else if (!IsGuestInstructionPointer(context, target) &&
             !IsAotCacheAddress(context, target))
    {
        target_failure = AotDbtDispatchFallbackReason::kNonGuestTarget;
    }
    if (!ResolveAotTransferTarget(context, target, &cache_target))
    {
        if (fallback_reason != nullptr)
        {
            *fallback_reason = target_failure;
        }
        context->aot_last_indirect_source.store(source,
                                                 std::memory_order_relaxed);
        context->aot_last_indirect_target.store(target,
                                                 std::memory_order_relaxed);
        return false;
    }
    if (IsAotInlineCacheMiss(context, context->aot_reentry_cache_address))
    {
        context->aot_inline_cache_patch_attempt_count.fetch_add(
            1, std::memory_order_relaxed);
        if (RequestAotInlineCachePatch(
                context, context->aot_reentry_cache_address,
                target, cache_target))
        {
            context->aot_inline_cache_patch_success_count.fetch_add(
                1, std::memory_order_relaxed);
        }
    }
    if (is_call)
    {
        const std::uint32_t return_address = source + instruction_size;
        const std::uint32_t entry_esp =
            static_cast<std::uint32_t>(win32_context->Esp);
        const std::uint32_t stack_address = win32_context->Esp - 4U;
        if (!WriteGuestUInt32(
                context,
                reinterpret_cast<void*>(
                    static_cast<std::uintptr_t>(stack_address)),
                return_address))
        {
            if (fallback_reason != nullptr)
            {
                *fallback_reason =
                    AotDbtDispatchFallbackReason::kUnreadableSource;
            }
            return false;
        }
        const std::uint32_t trace_sequence =
            RecordAotDbtCallReturnCall(
                context, origin, source, target, return_address, entry_esp);
        win32_context->Esp = stack_address;
        if (context->aot_call_depth < ThreadContext::kAotCallFrameCapacity)
        {
            ThreadContext::AotCallFrame& frame =
                context->aot_call_frames[context->aot_call_depth++];
            frame.source = source;
            frame.target = target;
            frame.fallthrough = return_address;
            frame.trace_sequence = trace_sequence;
            frame.entry_esp = entry_esp;
            frame.origin = origin;
            context->aot_last_call_source = source;
            context->aot_last_call_target = target;
        }
    }
    win32_context->Eip = cache_target;
    win32_context->EFlags &= ~0x00000100U;
    context->aot_reentry_pending = false;
    context->aot_legacy_fallback = false;
    context->enable_single_step_trace = false;
    context->aot_indirect_dispatch_count.fetch_add(
        1, std::memory_order_relaxed);
    const std::uint32_t transfer_slot =
        context->aot_transfer_trace_count % kAotTransferTraceCapacity;
    context->aot_transfer_trace[transfer_slot] = {source, target, is_call};
    ++context->aot_transfer_trace_count;
    context->aot_last_indirect_source.store(source,
                                             std::memory_order_relaxed);
    context->aot_last_indirect_target.store(target,
                                             std::memory_order_relaxed);
    AccumulateAotResidency(context, target);
    BumpAotReentryCount(context);
    return true;
}

bool HandleAotReturnTransfer(const repiu::platform::FaultEvent& fault,
                             ThreadContext* context,
                             AotDbtDispatchFallbackReason* fallback_reason,
                             AotTransferOrigin origin,
                             std::uint32_t return_patch_site_index)
{
    repiu::platform::GuestCpuContext* win32_context = fault.registers;
    const ExecutionTimeScope return_transfer_time_scope(
        context != nullptr ? context->execution_time_profile.get() : nullptr,
        ExecutionTimeBucket::kAotReturn);
    // Task 482: five mutually exclusive stages plus the residual of this same
    // window. The DBT adapter opens its own outer window around this call, so
    // the scope below attributes only when the VEH path arrives here directly.
    AotReturnStageProfile* stage_profile =
        context != nullptr ? &context->aot_return_stage_profile : nullptr;
    const AotReturnOuterScope outer_stage(stage_profile);
    AotReturnStageScope entry_stage(stage_profile,
                                    AotReturnStage::kEntryValidation);
    if (fallback_reason != nullptr)
    {
        *fallback_reason = AotDbtDispatchFallbackReason::kUnknown;
    }
    if (win32_context == nullptr || context == nullptr ||
        context->aot_placement == nullptr ||
        !context->aot_reentry_pending ||
        fault.kind != repiu::platform::FaultKind::kBreakpoint)
    {
        if (fallback_reason != nullptr)
        {
            *fallback_reason = AotDbtDispatchFallbackReason::kInvalidState;
        }
        return false;
    }
    const auto* instruction = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(win32_context->Eip));
    if (instruction[0] != 0xC3U && instruction[0] != 0xC2U)
    {
        if (fallback_reason != nullptr)
        {
            *fallback_reason =
                AotDbtDispatchFallbackReason::kInvalidInstruction;
        }
        return false;
    }
    entry_stage.Close();
    AotReturnStageScope read_stage(stage_profile,
                                   AotReturnStage::kTargetRead);
    std::uint32_t target = 0;
    if (!ReadGuestUInt32(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(win32_context->Esp)),
            &target))
    {
        if (fallback_reason != nullptr)
        {
            *fallback_reason =
                AotDbtDispatchFallbackReason::kUnreadableSource;
        }
        return false;
    }
    context->aot_last_return_target.store(target,
                                           std::memory_order_relaxed);
    context->aot_last_return_source.store(
        static_cast<std::uint32_t>(win32_context->Eip),
        std::memory_order_relaxed);
    // A zero return address is always pathological. Dump a self-contained
    // evidence packet for the first few occurrences so the deterministic
    // 0x0304ED35 zero-slot failure (Task 245) identifies its own corruption
    // shape, tracked call chain, and live code bytes (design 246).
    if (target == 0U)
    {
        DumpZeroReturnEvidence(
            win32_context, context, "return-dispatch",
            static_cast<std::uint32_t>(win32_context->Eip));
    }
    const void* return_stack = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(win32_context->Esp));
    if (IsGuestRangeReadable(context, return_stack,
                             sizeof(context->aot_last_return_stack)))
    {
        std::memcpy(context->aot_last_return_stack, return_stack,
                    sizeof(context->aot_last_return_stack));
    }
    context->aot_last_return_matches_call = false;
    context->aot_last_expected_return = 0;
    const ThreadContext::AotCallFrame* expected_frame = nullptr;
    if (context->aot_call_depth != 0U)
    {
        expected_frame =
            &context->aot_call_frames[context->aot_call_depth - 1U];
        const ThreadContext::AotCallFrame& frame = *expected_frame;
        context->aot_last_expected_return = frame.fallthrough;
        context->aot_last_expected_call_source = frame.source;
        context->aot_last_expected_call_target = frame.target;
        context->aot_last_return_matches_call =
            target == frame.fallthrough;
        if (context->aot_last_return_matches_call)
        {
            --context->aot_call_depth;
        }
    }
    RecordAotDbtCallReturnReturn(
        context, origin, static_cast<std::uint32_t>(win32_context->Eip),
        target, static_cast<std::uint32_t>(win32_context->Esp),
        expected_frame != nullptr ? expected_frame->trace_sequence : 0U,
        expected_frame != nullptr ? expected_frame->source : 0U,
        expected_frame != nullptr ? expected_frame->target : 0U,
        expected_frame != nullptr ? expected_frame->fallthrough : 0U,
        expected_frame != nullptr ? expected_frame->entry_esp : 0U);
    const std::uint32_t trace_slot =
        context->aot_return_trace_count % kAotReturnTraceCapacity;
    context->aot_return_trace[trace_slot] = {
        static_cast<std::uint32_t>(win32_context->Eip), target,
        context->aot_last_expected_return, win32_context->Esp,
        context->aot_last_return_matches_call};
    ++context->aot_return_trace_count;
    if (context->shared_live_telemetry != nullptr)
    {
        repiu::platform::AtomicExchange(
            &context->shared_live_telemetry->aot_last_return_source,
            static_cast<long>(win32_context->Eip));
        repiu::platform::AtomicExchange(
            &context->shared_live_telemetry->aot_last_return_target,
            static_cast<long>(target));
        repiu::platform::AtomicExchange(
            &context->shared_live_telemetry->aot_last_expected_return,
            static_cast<long>(context->aot_last_expected_return));
        repiu::platform::AtomicExchange(
            &context->shared_live_telemetry->aot_last_return_matches_call,
            context->aot_last_return_matches_call ? 1L : 0L);
    }
    read_stage.Close();
    AotReturnStageScope resolution_stage(
        stage_profile, AotReturnStage::kTargetResolution);
    std::uint32_t cache_target = target;
    AotDbtDispatchFallbackReason target_failure =
        AotDbtDispatchFallbackReason::kTranslationFailure;
    if (target == 0U)
    {
        target_failure = AotDbtDispatchFallbackReason::kZeroTarget;
    }
    else if (IsAotHleBoundaryAddress(context, target))
    {
        target_failure = AotDbtDispatchFallbackReason::kHleTarget;
    }
    else if (IsAotGuestPageQuarantined(
                 *context->aot_placement, target))
    {
        target_failure = AotDbtDispatchFallbackReason::kQuarantinedTarget;
    }
    else if (!IsGuestInstructionPointer(context, target) &&
             !IsAotCacheAddress(context, target))
    {
        target_failure = AotDbtDispatchFallbackReason::kNonGuestTarget;
    }
    AotRetiredTrapResolution target_resolution =
        AotRetiredTrapResolution::kFallback;
    if (!ResolveAotTransferTarget(context, target, &cache_target, false,
                                  &target_resolution))
    {
        if (fallback_reason != nullptr)
        {
            *fallback_reason = target_failure;
        }
        return false;
    }
    resolution_stage.Close();
    AotReturnStageScope patch_stage(stage_profile,
                                    AotReturnStage::kPatchPolicy);
    // Task 499: memoize only a resolution the host just validated as an active
    // hit. Glide-gate direct targets and freshly translated code report other
    // resolutions and stay out of the table, so a hit can only ever repeat a
    // mapping this handler already accepted.
    if (target_resolution == AotRetiredTrapResolution::kActiveHit &&
        context->aot_placement->direct_return_table_enabled)
    {
        runtime::InsertAotDirectReturnEntry(
            &context->aot_placement->direct_return_table, target,
            cache_target);
    }
    if (IsAotInlineCacheMiss(context, context->aot_reentry_cache_address))
    {
        const AotReturnPatchAction patch_action =
            return_patch_site_index == 0xFFFFFFFFU
            ? AotReturnPatchAction::kPatch
            : ObserveAotReturnPatchMiss(
                  context->aot_placement, return_patch_site_index, target);
        if (patch_action == AotReturnPatchAction::kPatch)
        {
            context->aot_inline_cache_patch_attempt_count.fetch_add(
                1, std::memory_order_relaxed);
            if (RequestAotInlineCachePatch(
                    context, context->aot_reentry_cache_address,
                    target, cache_target))
            {
                context->aot_inline_cache_patch_success_count.fetch_add(
                    1, std::memory_order_relaxed);
            }
        }
    }
    patch_stage.Close();
    const AotReturnStageScope continuation_stage(
        stage_profile, AotReturnStage::kContinuation);
    std::uint32_t pop_bytes = 4U;
    if (instruction[0] == 0xC2U)
    {
        pop_bytes += static_cast<std::uint32_t>(instruction[1]) |
                     (static_cast<std::uint32_t>(instruction[2]) << 8U);
    }
    win32_context->Esp += pop_bytes;
    win32_context->Eip = cache_target;
    win32_context->EFlags &= ~0x00000100U;
    context->aot_reentry_pending = false;
    context->aot_legacy_fallback = false;
    context->enable_single_step_trace = false;
    context->aot_return_dispatch_count.fetch_add(
        1, std::memory_order_relaxed);
    if (context->shared_live_telemetry != nullptr)
    {
        repiu::platform::AtomicIncrement(
            &context->shared_live_telemetry->aot_return_dispatch_count);
    }
    AccumulateAotResidency(context, target);
    BumpAotReentryCount(context);
    return true;
}

// Task 264 prerequisite probe (instrumentation only, no guest-state change).
// If the boundary guest instruction is a 32-bit push of a segment register,
// record the host segment register value alongside the shadow selector so the
// two can be compared: host==shadow means the guest executes with its own
// selectors loaded (native push is correct); a mismatch means host-flat (native
// push would push the host selector, so the translation must read the shadow).
static void ProbePushSegBoundary(ThreadContext* context,
                                 const repiu::platform::GuestCpuContext* win32_context,
                                 const std::uint8_t* bytes,
                                 std::size_t length)
{
    if (context->shared_live_telemetry == nullptr || length == 0)
    {
        return;
    }
    std::uint32_t host_selector = 0;
    std::uint32_t shadow_selector = 0;
    std::uint8_t opcode = bytes[0];
    switch (opcode)
    {
        case 0x06U: // push es
            host_selector = win32_context->SegEs;
            shadow_selector = context->guest_es;
            break;
        case 0x16U: // push ss
            host_selector = win32_context->SegSs;
            shadow_selector = context->guest_ss;
            break;
        case 0x1EU: // push ds
            host_selector = win32_context->SegDs;
            shadow_selector = context->guest_ds;
            break;
        case 0x0FU: // two-byte push fs/gs
            if (length < 2)
            {
                return;
            }
            if (bytes[1] == 0xA0U) // push fs
            {
                host_selector = win32_context->SegFs;
                shadow_selector = context->guest_fs;
                opcode = 0xA0U;
            }
            else if (bytes[1] == 0xA8U) // push gs
            {
                host_selector = win32_context->SegGs;
                shadow_selector = context->guest_gs;
                opcode = 0xA8U;
            }
            else
            {
                return;
            }
            break;
        default:
            return;
    }
    SharedLiveTelemetry* telemetry = context->shared_live_telemetry;
    repiu::platform::AtomicIncrement(&telemetry->aot_pushseg_count);
    repiu::platform::AtomicExchange(&telemetry->aot_pushseg_last_opcode,
                        static_cast<long>(opcode));
    repiu::platform::AtomicExchange(&telemetry->aot_pushseg_last_host_sel,
                        static_cast<long>(host_selector));
    repiu::platform::AtomicExchange(&telemetry->aot_pushseg_last_shadow_sel,
                        static_cast<long>(shadow_selector));
    if ((host_selector & 0xFFFFU) == (shadow_selector & 0xFFFFU))
    {
        repiu::platform::AtomicIncrement(&telemetry->aot_pushseg_match_count);
    }
    else
    {
        repiu::platform::AtomicIncrement(&telemetry->aot_pushseg_mismatch_count);
    }
}

// Task 264 Phase 3 characterization probe (instrumentation only). If the
// boundary guest instruction carries a segment-override prefix, resolve the
// overridden segment's shadow selector to a descriptor base so we can judge
// whether GS/FS/DS/ES memory overrides are flat (base 0, translatable by
// stripping the prefix) or need a runtime base add.
static void ProbeSegmentOverrideBoundary(ThreadContext* context,
                                         const std::uint8_t* bytes,
                                         std::size_t length)
{
    if (context == nullptr || length == 0)
    {
        return;
    }
    std::uint16_t selector = 0;
    std::uint32_t shadow_address = 0;
    const std::uint8_t prefix = bytes[0];
    switch (prefix)
    {
        case 0x65U:
            selector = context->guest_gs;
            shadow_address = static_cast<std::uint32_t>(
                reinterpret_cast<std::uintptr_t>(&context->guest_gs));
            break;
        case 0x64U:
            selector = context->guest_fs;
            shadow_address = static_cast<std::uint32_t>(
                reinterpret_cast<std::uintptr_t>(&context->guest_fs));
            break;
        case 0x3EU:
            selector = context->guest_ds;
            shadow_address = static_cast<std::uint32_t>(
                reinterpret_cast<std::uintptr_t>(&context->guest_ds));
            break;
        case 0x26U:
            selector = context->guest_es;
            shadow_address = static_cast<std::uint32_t>(
                reinterpret_cast<std::uintptr_t>(&context->guest_es));
            break;
        case 0x36U:
            selector = context->guest_ss;
            shadow_address = static_cast<std::uint32_t>(
                reinterpret_cast<std::uintptr_t>(&context->guest_ss));
            break;
        default: return; // not a segment-override prefix (CS 0x2E has no shadow)
    }
    AotSegmentResolution resolution{};
    BuildAotSegmentResolution(
        context->selector_table, shadow_address, selector, &resolution);
    if (resolution.policy == AotSegmentAccessPolicy::kNativeFolded)
    {
        context->aot_selector_guard_mismatch_count.fetch_add(
            1U, std::memory_order_relaxed);
    }
    else
    {
        context->aot_selector_guard_hle_exit_count.fetch_add(
            1U, std::memory_order_relaxed);
    }
    if (context->shared_live_telemetry == nullptr)
    {
        return;
    }
    std::uint32_t base = 0;
    std::uint32_t linear = 0;
    if (repiu::runtime::TranslateSelectorOffset(
            context->selector_table, selector, 0U, 1U, &linear))
    {
        base = linear; // linear address for offset 0 == segment base
    }
    SharedLiveTelemetry* telemetry = context->shared_live_telemetry;
    repiu::platform::AtomicIncrement(&telemetry->aot_segovr_count);
    repiu::platform::AtomicExchange(&telemetry->aot_segovr_last_prefix,
                        static_cast<long>(prefix));
    repiu::platform::AtomicExchange(&telemetry->aot_segovr_last_selector,
                        static_cast<long>(selector));
    repiu::platform::AtomicExchange(&telemetry->aot_segovr_last_base,
                        static_cast<long>(base));
    if (base == 0U)
    {
        repiu::platform::AtomicIncrement(&telemetry->aot_segovr_flat_count);
    }
    else
    {
        repiu::platform::AtomicIncrement(&telemetry->aot_segovr_nonflat_count);
    }
}

bool HandleAotReentry(const repiu::platform::FaultEvent& fault,
                      ThreadContext* context)
{
    repiu::platform::GuestCpuContext* win32_context = fault.registers;
    const ExecutionTimeScope reentry_time_scope(
        context != nullptr ? context->execution_time_profile.get() : nullptr,
        ExecutionTimeBucket::kAotReentry);
    if (win32_context == nullptr || context == nullptr ||
        context->aot_placement == nullptr)
    {
        return false;
    }
    if (fault.kind == repiu::platform::FaultKind::kBreakpoint)
    {
        ClearAotFfTargetTimingCandidate(
            &context->aot_ff_boundary_attribution.target_timing);
        const std::uint32_t cache_address = fault.instruction_address;
        std::uint32_t guest_address = 0;
        // Task 334 interval 1. `FindAotGuestAddress` scans the whole address
        // map, which Task 324 fixed only in the opposite direction.
        bool located = false;
        {
            const ExecutionTimeScope guest_lookup_scope(
                context->execution_time_profile.get(),
                ExecutionTimeBucket::kAotReentryGuestLookup);
            located = FindAotDbtDirectEdgeFallbackTarget(
                context, cache_address, &guest_address) ||
                FindAotGuestAddress(*context->aot_placement,
                                    cache_address, &guest_address);
        }
        if (!located)
        {
            return false;
        }
        context->aot_reentry_cache_address = cache_address;
        if (ActivateGlideGateDirectTarget(
                context, cache_address, guest_address))
        {
            win32_context->Eip = guest_address;
            win32_context->EFlags &= ~0x00000100U;
            context->aot_reentry_pending = false;
            context->enable_single_step_trace = false;
            return true;
        }
        // A tracked execution-trace sentinel byte can stop being hit again on
        // later calls to the same guest address for reasons that go beyond
        // formal cache-entry retirement (empirically, the retirement check
        // below never fired for a sentinel that still stopped re-triggering —
        // see docs/design/20260717-223-guest-stack-watchpoint-veh-coexistence.md
        // §9). Re-installing the sentinel at whatever cache address is
        // *currently* resolved for the guest address, on every hit, is a
        // strictly more robust fix: it self-heals regardless of the exact
        // underlying mechanism (retranslation, alternate cache copy, etc.).
        // Task 334 interval 2: sentinel test, provenance classification, and
        // the retired-entry test. Closed before the next interval starts so the
        // six stay mutually exclusive.
        bool is_tracked_trace_address = false;
        bool retired_entry = false;
        {
            const ExecutionTimeScope provenance_scope(
                context->execution_time_profile.get(),
                ExecutionTimeBucket::kAotReentryProvenance);
            is_tracked_trace_address =
                context->execution_trace_configured &&
                (guest_address == context->runtime_base +
                                       context->execution_trace_start_offset ||
                 (context->execution_trace_sentinel2_configured &&
                  guest_address ==
                      context->runtime_base +
                          context->execution_trace_sentinel2_offset));
            RecordAotBreakpointProvenance(
                context,
                ClassifyAotCacheBreakpointProvenance(
                    *context->aot_placement, cache_address,
                    is_tracked_trace_address));
            retired_entry = IsAotCacheAddressRetired(
                *context->aot_placement, cache_address);
        }
        // Task 334 interval 3. The early return inside still closes the scope,
        // so a resolved retired trap is attributed here rather than lost.
        if (retired_entry)
        {
            const ExecutionTimeScope retired_scope(
                context->execution_time_profile.get(),
                ExecutionTimeBucket::kAotReentryRetired);
            BumpAotRetiredEntryTrapCount(context);
            AotRetiredTrapProfile* retired_profile =
                AotRetiredTrapProfileEnabled()
                    ? &context->aot_retired_trap_profile
                    : nullptr;
            if (retired_profile != nullptr)
            {
                RecordAotRetiredTrap(
                    retired_profile, *context->aot_placement,
                    cache_address, guest_address);
            }
            if (!is_tracked_trace_address)
            {
                std::uint32_t latest_cache_address = guest_address;
                AotRetiredTrapResolution resolution =
                    AotRetiredTrapResolution::kFallback;
                const bool resolved = ResolveAotTransferTarget(
                    context, guest_address, &latest_cache_address, true,
                    retired_profile != nullptr ? &resolution : nullptr);
                if (retired_profile != nullptr)
                {
                    RecordAotRetiredTrapResolution(
                        retired_profile, resolution);
                }
                if (resolved)
                {
                    NoteVehExitSite(
                        context, VehExitSite::kAotReentryRetiredResolved);
                    win32_context->Eip = latest_cache_address;
                    win32_context->EFlags &= ~0x00000100U;
                    context->aot_reentry_pending = false;
                    context->aot_legacy_fallback = false;
                    context->enable_single_step_trace = false;
                    AccumulateAotResidency(context, guest_address);
                    BumpAotReentryCount(context);
                    return true;
                }
            }
            else if (retired_profile != nullptr)
            {
                RecordAotRetiredTrapResolution(
                    retired_profile,
                    AotRetiredTrapResolution::kTraceSentinel);
            }
        }
        // Task 410: this branch returns false, so a later site normally
        // overwrites the tag. It is set anyway, because "the boundary set the
        // state and nothing else claimed the exception" is exactly the reading
        // that has to be distinguishable.
        NoteVehExitSite(context, VehExitSite::kAotReentryBoundary);
        win32_context->Eip = guest_address;
        RecordExecutionProbe(win32_context, context);
        win32_context->EFlags |= 0x00000100U;
        context->aot_reentry_pending = true;
        context->enable_single_step_trace = true;
        if (context->shared_live_telemetry != nullptr)
        {
            repiu::platform::AtomicExchange(
                &context->shared_live_telemetry->aot_boundary_guest_eip,
                static_cast<long>(guest_address));
        }
        BumpAotBoundaryCount(context);
        // Attribute this exit to the kind of guest instruction the translated
        // block ended on (Task 262). Read up to four bytes -- two suffice to
        // classify the 0F/FF two-byte forms, the rest feed the `other` sample
        // (Task 263a) -- honoring readability so a boundary at a page edge falls
        // into kOther instead of faulting.
        {
            // Task 334 interval 4.
            const ExecutionTimeScope boundary_reason_scope(
                context->execution_time_profile.get(),
                ExecutionTimeBucket::kAotReentryBoundaryReason);
            const auto* boundary_bytes = reinterpret_cast<const std::uint8_t*>(
                static_cast<std::uintptr_t>(guest_address));
            std::size_t readable = 0;
            if (IsGuestRangeReadable(context, boundary_bytes, 4U))
            {
                readable = 4;
            }
            else if (IsGuestRangeReadable(context, boundary_bytes, 2U))
            {
                readable = 2;
            }
            else if (IsGuestRangeReadable(context, boundary_bytes, 1U))
            {
                readable = 1;
            }
            const AotBoundaryReason reason =
                ClassifyAotBoundaryInstruction(boundary_bytes, readable);
            BumpAotBoundaryReason(context, reason);
            if (reason == AotBoundaryReason::kOther)
            {
                RecordAotOtherBoundarySample(context, guest_address,
                                             boundary_bytes, readable);
                RecordAotFfBoundaryTargetSample(
                    &context->aot_ff_boundary_attribution,
                    context,
                    win32_context,
                    guest_address,
                    boundary_bytes,
                    readable);
            }
            // Task 264 prerequisite probe (instrumentation only): at a
            // push-segment boundary, compare the host segment register against
            // the shadow selector to settle whether the guest runs with its own
            // selectors loaded or host-flat.
            ProbePushSegBoundary(context, win32_context, boundary_bytes,
                                 readable);
            ProbeSegmentOverrideBoundary(context, boundary_bytes, readable);
        }
        if (is_tracked_trace_address)
        {
            if (InstallAotProbeSentinel(
                    context->aot_placement, guest_address))
            {
                ++context->execution_trace_sentinel_rearm_count;
            }
        }
        if (retired_entry && !is_tracked_trace_address &&
            RetiredTrapNativeSpanEnabled(context->execution_backend))
        {
            // Task 334 interval 5.
            const ExecutionTimeScope native_span_scope(
                context->execution_time_profile.get(),
                ExecutionTimeBucket::kAotReentryNativeSpan);
            if (TryEnterRetiredTrapNativeSpan(win32_context, context))
            {
                return true;
            }
        }
        return false;
    }
    if (fault.kind != repiu::platform::FaultKind::kSingleStep ||
        !context->aot_reentry_pending)
    {
        return false;
    }
    // Task 334 interval 6: the single-step resumption path in full.
    const ExecutionTimeScope single_step_scope(
        context->execution_time_profile.get(),
        ExecutionTimeBucket::kAotReentrySingleStep);
    const std::uint32_t current = static_cast<std::uint32_t>(win32_context->Eip);
    if (IsAotCacheAddress(context, current))
    {
        NoteVehExitSite(context, VehExitSite::kAotReentryCacheAddress);
        win32_context->EFlags &= ~0x00000100U;
        context->aot_reentry_pending = false;
        context->enable_single_step_trace = false;
        BumpAotReentryCount(context);
        return true;
    }
    if (IsAotGuestPageQuarantined(
            *context->aot_placement, current))
    {
        NoteVehExitSite(context, VehExitSite::kAotReentryQuarantined);
        win32_context->EFlags |= 0x00000100U;
        context->aot_reentry_pending = true;
        context->aot_legacy_fallback = false;
        context->enable_single_step_trace = true;
        return false;
    }
    std::uint32_t cache_address = current;
    if (ResolveAotTransferTarget(context, current, &cache_address))
    {
        NoteVehExitSite(context, VehExitSite::kAotReentryResolved);
        if (AotFfTargetTimingEnabled())
        {
            BeginAotFfTargetTimingIfMatched(
                &context->aot_ff_boundary_attribution.target_timing,
                current,
                cache_address,
                repiu::platform::ReadCycleCounter());
        }
        win32_context->Eip = cache_address;
        win32_context->EFlags &= ~0x00000100U;
        context->aot_reentry_pending = false;
        context->aot_legacy_fallback = false;
        context->enable_single_step_trace = false;
        AccumulateAotResidency(context, current);
        BumpAotReentryCount(context);
        return true;
    }
    if (IsAotGuestPageQuarantined(
            *context->aot_placement, current))
    {
        NoteVehExitSite(context, VehExitSite::kAotReentryQuarantined);
        win32_context->EFlags |= 0x00000100U;
        context->aot_reentry_pending = true;
        context->aot_legacy_fallback = false;
        context->enable_single_step_trace = true;
        return false;
    }
    NoteVehExitSite(context, VehExitSite::kAotReentryLegacyFallback);
    context->aot_reentry_pending = false;
    context->aot_legacy_fallback = true;
    context->enable_single_step_trace = true;
    context->aot_legacy_fallback_count.fetch_add(
        1, std::memory_order_relaxed);
    context->aot_last_fallback_address.store(current,
                                              std::memory_order_relaxed);
    if (context->shared_live_telemetry != nullptr)
    {
        repiu::platform::AtomicIncrement(
            &context->shared_live_telemetry->aot_legacy_fallback_count);
        repiu::platform::AtomicExchange(
            &context->shared_live_telemetry->aot_last_fallback_address,
            static_cast<long>(current));
    }
    return false;
}

} // namespace repiu::engine
