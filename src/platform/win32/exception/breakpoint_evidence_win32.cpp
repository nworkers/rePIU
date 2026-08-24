#include "breakpoint_evidence_win32.h"

#include "execution_internal.h"
#include "thread_context.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include "repiu/platform/guest_cpu_context.h"
#include "repiu/platform/safe_memory_copy.h"
#include "repiu/platform/virtual_memory.h"

namespace repiu::platform::win32
{
namespace
{

std::uint32_t CaptureStateFlags(const ThreadContext& context)
{
    std::uint32_t flags = 0;
    if (context.aot_reentry_pending)
    {
        flags |= kWin32BreakpointAotReentryPending;
    }
    if (context.enable_single_step_trace)
    {
        flags |= kWin32BreakpointSingleStepTrace;
    }
    if (context.native_fast_path.active)
    {
        flags |= kWin32BreakpointNativeFastPath;
    }
    if (context.native_fast_path.linear_span_active)
    {
        flags |= kWin32BreakpointNativeLinearSpan;
    }
    if (context.native_fast_path.region_active)
    {
        flags |= kWin32BreakpointNativeRegion;
    }
    return flags;
}

void CaptureByteWindow(
    std::uint32_t focus,
    std::uint32_t* window_base,
    std::uint8_t* bytes,
    std::uint32_t* byte_count)
{
    *window_base = 0;
    *byte_count = 0;
    if (focus == 0U)
    {
        return;
    }

    const repiu::platform::MemoryRegion region =
        repiu::platform::QueryMemory(reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(focus)));
    if (!region.valid || !region.committed || !region.readable)
    {
        return;
    }

    const std::uintptr_t region_begin =
        reinterpret_cast<std::uintptr_t>(region.base);
    const std::uintptr_t region_end = region_begin + region.size;
    const std::uintptr_t preferred_begin =
        focus >= kWin32BreakpointByteWindowCapacity / 2U
            ? static_cast<std::uintptr_t>(
                  focus - kWin32BreakpointByteWindowCapacity / 2U)
            : static_cast<std::uintptr_t>(focus);
    const std::uintptr_t begin = std::max(region_begin, preferred_begin);
    const std::size_t available = region_end > begin
        ? static_cast<std::size_t>(region_end - begin)
        : 0U;
    const std::size_t requested = std::min<std::size_t>(
        kWin32BreakpointByteWindowCapacity, available);
    if (requested == 0U)
    {
        return;
    }

    const repiu::platform::SafeCopyResult read =
        repiu::platform::CopyMemoryWithoutFaulting(
            bytes, reinterpret_cast<const void*>(begin), requested);
    *window_base = static_cast<std::uint32_t>(begin);
    *byte_count = static_cast<std::uint32_t>(
        std::min<std::size_t>(read.bytes_copied, requested));
}

void CaptureStackTop(
    std::uint32_t esp,
    std::uint32_t* dwords,
    std::uint32_t* valid_mask)
{
    *valid_mask = 0;
    for (std::uint32_t index = 0; index < 4U; ++index)
    {
        const std::uintptr_t address =
            static_cast<std::uintptr_t>(esp) +
            index * sizeof(std::uint32_t);
        if (repiu::platform::CopyMemoryWithoutFaulting(
                &dwords[index], reinterpret_cast<const void*>(address),
                sizeof(dwords[index])).complete)
        {
            *valid_mask |= 1U << index;
        }
    }
}

void CaptureMapping(
    const ThreadContext& context,
    std::uint32_t address,
    bool* exact_valid,
    std::uint32_t* exact_guest,
    bool* previous_valid,
    std::uint32_t* previous_guest)
{
    if (context.aot_placement == nullptr)
    {
        return;
    }
    *exact_valid = FindAotGuestAddress(
        *context.aot_placement, address, exact_guest);
    if (address != 0U)
    {
        *previous_valid = FindAotGuestAddress(
            *context.aot_placement, address - 1U, previous_guest);
    }
}

void CaptureProvenance(
    const ThreadContext& context,
    std::uint32_t address,
    bool* exact_valid,
    std::uint32_t* exact_provenance,
    bool* previous_valid,
    std::uint32_t* previous_provenance)
{
    if (context.aot_placement == nullptr)
    {
        return;
    }
    if (IsAotCacheAddress(&context, address))
    {
        *exact_valid = true;
        *exact_provenance = static_cast<std::uint32_t>(
            ClassifyAotCacheBreakpointProvenance(
                *context.aot_placement, address, false));
    }
    if (address != 0U &&
        IsAotCacheAddress(&context, address - 1U))
    {
        *previous_valid = true;
        *previous_provenance = static_cast<std::uint32_t>(
            ClassifyAotCacheBreakpointProvenance(
                *context.aot_placement, address - 1U, false));
    }
}

}  // namespace

Win32UnhandledBreakpointEvidence CaptureBreakpointEvidence(
    const repiu::platform::FaultEvent& fault,
    const ThreadContext* context)
{
    Win32UnhandledBreakpointEvidence evidence;
    if (fault.registers == nullptr || context == nullptr ||
        fault.kind != repiu::platform::FaultKind::kBreakpoint)
    {
        return evidence;
    }

    const repiu::platform::GuestCpuContext& win32_context = *fault.registers;
    evidence.code =
        fault.host_code;
    evidence.exception_address = static_cast<std::uint32_t>(
        static_cast<std::uintptr_t>(fault.instruction_address));
    evidence.entry_eip = static_cast<std::uint32_t>(win32_context.Eip);
    evidence.entry_esp = static_cast<std::uint32_t>(win32_context.Esp);
    evidence.entry_eflags =
        static_cast<std::uint32_t>(win32_context.EFlags);
    evidence.entry_dr6 = static_cast<std::uint32_t>(win32_context.Dr6);
    evidence.entry_dr7 = static_cast<std::uint32_t>(win32_context.Dr7);
    evidence.entry_state_flags = CaptureStateFlags(*context);
    evidence.entry_aot_reentry_cache_address =
        context->aot_reentry_cache_address;
    evidence.entry_aot_return_dispatch_count =
        context->aot_return_dispatch_count.load(std::memory_order_relaxed);
    evidence.entry_aot_last_return_source =
        context->aot_last_return_source.load(std::memory_order_relaxed);
    evidence.entry_aot_last_return_target =
        context->aot_last_return_target.load(std::memory_order_relaxed);
    evidence.exception_address_in_aot_cache =
        IsAotCacheAddress(context, evidence.exception_address);
    evidence.entry_eip_in_aot_cache =
        IsAotCacheAddress(context, evidence.entry_eip);
    return evidence;
}

void CommitUnhandledBreakpointEvidence(
    Win32UnhandledBreakpointEvidence evidence,
    const repiu::platform::GuestCpuContext* final_context,
    ThreadContext* context)
{
    // The capture already refused anything that was not a breakpoint and left
    // `valid` false, so re-testing the host's raw code here only asked the same
    // question in a host-specific way.
    if (!evidence.valid || final_context == nullptr || context == nullptr)
    {
        return;
    }
    CaptureMapping(
        *context, evidence.exception_address,
        &evidence.exception_exact_mapping_valid,
        &evidence.exception_exact_guest,
        &evidence.exception_previous_mapping_valid,
        &evidence.exception_previous_guest);
    CaptureMapping(
        *context, evidence.entry_eip,
        &evidence.eip_exact_mapping_valid,
        &evidence.eip_exact_guest,
        &evidence.eip_previous_mapping_valid,
        &evidence.eip_previous_guest);
    CaptureProvenance(
        *context, evidence.exception_address,
        &evidence.exception_exact_provenance_valid,
        &evidence.exception_exact_provenance,
        &evidence.exception_previous_provenance_valid,
        &evidence.exception_previous_provenance);
    CaptureProvenance(
        *context, evidence.entry_eip,
        &evidence.eip_exact_provenance_valid,
        &evidence.eip_exact_provenance,
        &evidence.eip_previous_provenance_valid,
        &evidence.eip_previous_provenance);
    CaptureByteWindow(
        evidence.exception_address,
        &evidence.exception_window_base,
        evidence.exception_window,
        &evidence.exception_window_count);
    CaptureByteWindow(
        evidence.entry_eip,
        &evidence.eip_window_base,
        evidence.eip_window,
        &evidence.eip_window_count);
    CaptureStackTop(
        evidence.entry_esp, evidence.stack_dwords,
        &evidence.stack_valid_mask);
    evidence.valid = true;
    evidence.final_eip =
        static_cast<std::uint32_t>(final_context->Eip);
    evidence.final_esp =
        static_cast<std::uint32_t>(final_context->Esp);
    evidence.final_state_flags = CaptureStateFlags(*context);
    evidence.final_aot_return_dispatch_count =
        context->aot_return_dispatch_count.load(std::memory_order_relaxed);
    evidence.final_aot_last_return_source =
        context->aot_last_return_source.load(std::memory_order_relaxed);
    evidence.final_aot_last_return_target =
        context->aot_last_return_target.load(std::memory_order_relaxed);
    context->unhandled_breakpoint_evidence = evidence;
}

}  // namespace repiu::platform::win32
