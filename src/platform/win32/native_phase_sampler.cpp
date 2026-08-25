#include "native_phase_sampler.h"

#include "repiu/platform/win32/aot_code_cache_win32.h"
#include "repiu/platform/win32/live_telemetry.h"

// Task 503d-14 fenced this file, saying Linux had no counterpart for stopping a
// running thread and reading its registers. Task 503d-20 built one, so the fence
// is gone and the sampler works on both hosts.

#include "repiu/platform/host_error_stream.h"
#include "repiu/platform/host_thread.h"
#include "repiu/platform/safe_memory_copy.h"

#include <cstdio>
#include "repiu/platform/guest_cpu_context.h"
#include "repiu/platform/atomic_ops.h"

namespace repiu::platform::win32
{
namespace
{

#if defined(_M_IX86) || defined(__i386__)
// Task 412. Scans the interrupted thread's stack for the first value inside
// [module_base, module_base + module_size) and returns it, or zero when none is
// found.
//
// Task 503d-21: the SEH `__try` that used to wrap this is gone. The stack of a
// thread that may be anywhere is exactly the "suspect by definition" address
// 3d-7 built `CopyMemoryWithoutFaulting` for, and unlike SEH it works on both
// hosts. It is also one syscall with no lock and no allocation, which is what
// lets this run where it now runs -- inside a signal handler on Linux, on the
// target thread itself.
std::uint32_t ScanStackForModuleReturn(std::uint32_t esp,
                                       std::uint32_t module_base,
                                       std::uint32_t module_size,
                                       bool* failed)
{
    constexpr std::uint32_t kScanDwords = 64U;
    *failed = false;
    if (esp == 0U || module_size == 0U)
    {
        return 0U;
    }
    const std::uint32_t module_end = module_base + module_size;

    std::uint32_t stack[kScanDwords] = {};
    const repiu::platform::SafeCopyResult copied =
        repiu::platform::CopyMemoryWithoutFaulting(
            stack,
            reinterpret_cast<const void*>(static_cast<std::uintptr_t>(esp)),
            sizeof(stack));
    // A partial read is still worth scanning: the words that arrived are the
    // ones nearest the stack pointer, which is where a return address is.
    const std::uint32_t readable =
        static_cast<std::uint32_t>(copied.bytes_copied / sizeof(std::uint32_t));
    if (!copied.complete)
    {
        *failed = true;
    }
    for (std::uint32_t index = 0; index < readable; ++index)
    {
        if (stack[index] >= module_base && stack[index] < module_end)
        {
            return stack[index];
        }
    }
    return 0U;
}

// Task 503d-21. What the interrupt callback carries between the two threads.
struct NativePhaseCaptureRequest
{
    Win32NativePhaseSample* sample = nullptr;
    std::uint32_t module_base = 0;
    std::uint32_t module_size = 0;
};

// Runs on the calling thread on Windows with the target frozen, and on the
// target thread itself on Linux. Everything here is bounded, takes no lock and
// allocates nothing, which is what that second case requires.
//
// The AOT address mapping is deliberately *not* here: it is a pure function of
// the sampled EIP and can degrade to a scan of a hundred thousand entries, so
// it runs after the target is moving again.
void CaptureNativePhaseRegisters(repiu::platform::GuestCpuContext* registers,
                                 void* user_data)
{
    auto* request = static_cast<NativePhaseCaptureRequest*>(user_data);
    Win32NativePhaseSample* sample = request->sample;
    sample->captured = true;
    sample->eip = registers->Eip;
    sample->eax = registers->Eax;
    sample->ebx = registers->Ebx;
    sample->ecx = registers->Ecx;
    sample->edx = registers->Edx;
    sample->esi = registers->Esi;
    sample->edi = registers->Edi;
    sample->esp = registers->Esp;
    sample->ebp = registers->Ebp;
    sample->eflags = registers->EFlags;

    if (request->module_size != 0U)
    {
        bool scan_failed = false;
        sample->host_scan_attempted = true;
        sample->host_call_site = ScanStackForModuleReturn(
            sample->esp, request->module_base, request->module_size,
            &scan_failed);
        sample->host_scan_failed = scan_failed;
    }
}
#endif

}  // namespace

bool CaptureWin32NativePhaseSample(const repiu::platform::HostThread& thread,
                                   const Win32AotCodeCachePlacement* placement,
                                   Win32SharedLiveTelemetry* telemetry,
                                   Win32NativePhaseSample* sample,
                                   std::uint32_t module_base,
                                   std::uint32_t module_size)
{
    if (!thread.valid || sample == nullptr)
    {
        return false;
    }
    *sample = Win32NativePhaseSample{};
    const auto mark_stage = [telemetry](long stage) {
        if (telemetry != nullptr)
        {
            repiu::platform::AtomicExchange(&telemetry->native_sample_stage, stage);
        }
    };

#if defined(_M_IX86) || defined(__i386__)
    // Task 503d-21: a bounded wait. This is a diagnostic sampling a thread that
    // may be in trouble, and one that hangs waiting for an answer stops the
    // loop whose job is to notice the trouble.
    constexpr std::uint32_t kSampleTimeoutMilliseconds = 200U;

    NativePhaseCaptureRequest request;
    request.sample = sample;
    request.module_base = module_base;
    request.module_size = module_size;

    mark_stage(1);
    repiu::platform::ThreadInterruptFailure failure =
        repiu::platform::ThreadInterruptFailure::kNone;
    if (!repiu::platform::InterruptHostThread(thread,
                                              &CaptureNativePhaseRegisters,
                                              &request,
                                              kSampleTimeoutMilliseconds,
                                              &failure))
    {
        sample->failure_stage = 1;
        // Reported where the Windows error used to go: "it failed" is not a
        // finding, and which of the three it was decides what to look at next.
        sample->windows_error = static_cast<std::uint32_t>(failure);
        mark_stage(0);
        return false;
    }
    mark_stage(3);

    // Outside the interrupt on purpose: this is a pure function of the sampled
    // EIP, and it degrades to a scan of the whole address map when the index is
    // stale -- not something to do while the target is stopped, and on Linux
    // not something to do in a signal handler.
    if (placement != nullptr && placement->placed)
    {
        const std::uint64_t cache_end =
            static_cast<std::uint64_t>(placement->base_address) +
            placement->size;
        if (sample->eip >= placement->base_address &&
            sample->eip < cache_end)
        {
            std::uint32_t guest_address = 0;
            if (FindAotGuestAddress(*placement, sample->eip, &guest_address))
            {
                sample->mapped = true;
                sample->guest_eip = guest_address;
            }
        }
    }
    mark_stage(4);
    return sample->captured;
#else
    (void)placement;
    (void)module_base;
    (void)module_size;
    sample->failure_stage = 3;
    mark_stage(0);
    return false;
#endif
}

void RecordWin32NativePhaseSample(const Win32NativePhaseSample& sample,
                                  Win32NativePhaseSamplerState* state,
                                  Win32SharedLiveTelemetry* telemetry)
{
    if (!sample.captured || state == nullptr)
    {
        return;
    }

    ++state->sample_count;
    if (!sample.mapped)
    {
        ++state->unmapped_count;
    }
    const std::uint32_t slot =
        state->ring_cursor % Win32NativePhaseSamplerState::kRingCapacity;
    state->ring[slot] = sample.mapped ? sample.guest_eip : sample.eip;
    if (sample.mapped)
    {
        state->ring_mapped_bits |= 1U << slot;
    }
    else
    {
        state->ring_mapped_bits &= ~(1U << slot);
    }
    state->ring_cursor =
        (state->ring_cursor + 1U) %
        Win32NativePhaseSamplerState::kRingCapacity;

    if (telemetry == nullptr)
    {
        return;
    }
    repiu::platform::AtomicExchange(&telemetry->native_sample_count,
                        static_cast<long>(state->sample_count));
    repiu::platform::AtomicExchange(&telemetry->native_sample_unmapped_count,
                        static_cast<long>(state->unmapped_count));
    repiu::platform::AtomicExchange(&telemetry->native_sample_eip,
                        static_cast<long>(sample.eip));
    repiu::platform::AtomicExchange(&telemetry->native_sample_guest_eip,
                        static_cast<long>(sample.guest_eip));
    repiu::platform::AtomicExchange(&telemetry->native_sample_eax,
                        static_cast<long>(sample.eax));
    repiu::platform::AtomicExchange(&telemetry->native_sample_ebx,
                        static_cast<long>(sample.ebx));
    repiu::platform::AtomicExchange(&telemetry->native_sample_ecx,
                        static_cast<long>(sample.ecx));
    repiu::platform::AtomicExchange(&telemetry->native_sample_edx,
                        static_cast<long>(sample.edx));
    repiu::platform::AtomicExchange(&telemetry->native_sample_esi,
                        static_cast<long>(sample.esi));
    repiu::platform::AtomicExchange(&telemetry->native_sample_edi,
                        static_cast<long>(sample.edi));
    repiu::platform::AtomicExchange(&telemetry->native_sample_esp,
                        static_cast<long>(sample.esp));
    repiu::platform::AtomicExchange(&telemetry->native_sample_ebp,
                        static_cast<long>(sample.ebp));
    repiu::platform::AtomicExchange(&telemetry->native_sample_eflags,
                        static_cast<long>(sample.eflags));
    repiu::platform::AtomicExchange(&telemetry->native_sample_indirect_source,
                        static_cast<long>(sample.last_indirect_source));
    repiu::platform::AtomicExchange(&telemetry->native_sample_indirect_target,
                        static_cast<long>(sample.last_indirect_target));
    static_assert(Win32NativePhaseSamplerState::kRingCapacity ==
                  kWin32NativeSampleRingCapacity);
    for (std::uint32_t index = 0;
         index < Win32NativePhaseSamplerState::kRingCapacity; ++index)
    {
        repiu::platform::AtomicExchange(&telemetry->native_sample_ring[index],
                            static_cast<long>(state->ring[index]));
    }
    repiu::platform::AtomicExchange(&telemetry->native_sample_ring_mapped_bits,
                        static_cast<long>(state->ring_mapped_bits));
    repiu::platform::AtomicExchange(&telemetry->native_sample_ring_cursor,
                        static_cast<long>(state->ring_cursor));
}

void WriteWin32NativePhaseSampleLine(
    const Win32NativePhaseSample& sample,
    const Win32NativePhaseSamplerState& state,
    std::uint32_t elapsed_milliseconds)
{
    char buffer[256] = {};
    int length = 0;
    if (sample.captured)
    {
        length = std::snprintf(
            buffer,
            sizeof(buffer),
            "[repiu-sample] elapsed_ms=%lu count=%lu unmapped=%lu "
            "eip=0x%08X guest=0x%08X eax=0x%08X ecx=0x%08X esi=0x%08X "
            "edi=0x%08X esp=0x%08X indirect=0x%08X->0x%08X\r\n",
            static_cast<unsigned long>(elapsed_milliseconds),
            static_cast<unsigned long>(state.sample_count),
            static_cast<unsigned long>(state.unmapped_count),
            sample.eip,
            sample.guest_eip,
            sample.eax,
            sample.ecx,
            sample.esi,
            sample.edi,
            sample.esp,
            sample.last_indirect_source,
            sample.last_indirect_target);
    }
    else
    {
        length = std::snprintf(
            buffer,
            sizeof(buffer),
            "[repiu-sample] elapsed_ms=%lu capture_failed stage=%lu "
            "error=%lu\r\n",
            static_cast<unsigned long>(elapsed_milliseconds),
            static_cast<unsigned long>(sample.failure_stage),
            static_cast<unsigned long>(sample.windows_error));
    }
    if (length <= 0)
    {
        return;
    }
    const std::size_t byte_count = static_cast<std::size_t>(
        length < static_cast<int>(sizeof(buffer)) ? length
                                                  : sizeof(buffer) - 1U);
    repiu::platform::WriteHostErrorStream(buffer, byte_count);
}

}  // namespace repiu::platform::win32
