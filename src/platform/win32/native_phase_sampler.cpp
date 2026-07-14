#include "native_phase_sampler.h"

#include "repiu/platform/win32/aot_code_cache_win32.h"
#include "repiu/platform/win32/live_telemetry.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>

namespace repiu::platform::win32
{

bool CaptureWin32NativePhaseSample(void* thread,
                                   const Win32AotCodeCachePlacement* placement,
                                   Win32SharedLiveTelemetry* telemetry,
                                   Win32NativePhaseSample* sample)
{
    if (thread == nullptr || sample == nullptr)
    {
        return false;
    }
    *sample = Win32NativePhaseSample{};
    const auto mark_stage = [telemetry](long stage) {
        if (telemetry != nullptr)
        {
            InterlockedExchange(&telemetry->native_sample_stage, stage);
        }
    };

#if defined(_M_IX86)
    HANDLE thread_handle = static_cast<HANDLE>(thread);
    mark_stage(1);
    if (SuspendThread(thread_handle) == static_cast<DWORD>(-1))
    {
        sample->failure_stage = 1;
        sample->windows_error = GetLastError();
        mark_stage(0);
        return false;
    }
    mark_stage(2);

    CONTEXT thread_context = {};
    thread_context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    if (GetThreadContext(thread_handle, &thread_context))
    {
        mark_stage(3);
        sample->captured = true;
        sample->eip = thread_context.Eip;
        sample->eax = thread_context.Eax;
        sample->ebx = thread_context.Ebx;
        sample->ecx = thread_context.Ecx;
        sample->edx = thread_context.Edx;
        sample->esi = thread_context.Esi;
        sample->edi = thread_context.Edi;
        sample->esp = thread_context.Esp;
        sample->ebp = thread_context.Ebp;
        sample->eflags = thread_context.EFlags;
        if (placement != nullptr && placement->placed)
        {
            const std::uint64_t cache_end =
                static_cast<std::uint64_t>(placement->base_address) +
                placement->size;
            if (sample->eip >= placement->base_address &&
                sample->eip < cache_end)
            {
                std::uint32_t guest_address = 0;
                if (FindAotGuestAddress(*placement, sample->eip,
                                        &guest_address))
                {
                    sample->mapped = true;
                    sample->guest_eip = guest_address;
                }
            }
        }
    }
    else
    {
        sample->failure_stage = 2;
        sample->windows_error = GetLastError();
    }
    ResumeThread(thread_handle);
    mark_stage(4);
    return sample->captured;
#else
    (void)placement;
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
    InterlockedExchange(&telemetry->native_sample_count,
                        static_cast<long>(state->sample_count));
    InterlockedExchange(&telemetry->native_sample_unmapped_count,
                        static_cast<long>(state->unmapped_count));
    InterlockedExchange(&telemetry->native_sample_eip,
                        static_cast<long>(sample.eip));
    InterlockedExchange(&telemetry->native_sample_guest_eip,
                        static_cast<long>(sample.guest_eip));
    InterlockedExchange(&telemetry->native_sample_eax,
                        static_cast<long>(sample.eax));
    InterlockedExchange(&telemetry->native_sample_ebx,
                        static_cast<long>(sample.ebx));
    InterlockedExchange(&telemetry->native_sample_ecx,
                        static_cast<long>(sample.ecx));
    InterlockedExchange(&telemetry->native_sample_edx,
                        static_cast<long>(sample.edx));
    InterlockedExchange(&telemetry->native_sample_esi,
                        static_cast<long>(sample.esi));
    InterlockedExchange(&telemetry->native_sample_edi,
                        static_cast<long>(sample.edi));
    InterlockedExchange(&telemetry->native_sample_esp,
                        static_cast<long>(sample.esp));
    InterlockedExchange(&telemetry->native_sample_ebp,
                        static_cast<long>(sample.ebp));
    InterlockedExchange(&telemetry->native_sample_eflags,
                        static_cast<long>(sample.eflags));
    InterlockedExchange(&telemetry->native_sample_indirect_source,
                        static_cast<long>(sample.last_indirect_source));
    InterlockedExchange(&telemetry->native_sample_indirect_target,
                        static_cast<long>(sample.last_indirect_target));
    static_assert(Win32NativePhaseSamplerState::kRingCapacity ==
                  kWin32NativeSampleRingCapacity);
    for (std::uint32_t index = 0;
         index < Win32NativePhaseSamplerState::kRingCapacity; ++index)
    {
        InterlockedExchange(&telemetry->native_sample_ring[index],
                            static_cast<long>(state->ring[index]));
    }
    InterlockedExchange(&telemetry->native_sample_ring_mapped_bits,
                        static_cast<long>(state->ring_mapped_bits));
    InterlockedExchange(&telemetry->native_sample_ring_cursor,
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
    HANDLE stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
    if (stderr_handle == nullptr || stderr_handle == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    const DWORD byte_count = static_cast<DWORD>(
        length < static_cast<int>(sizeof(buffer)) ? length
                                                  : sizeof(buffer) - 1U);
    WriteFile(stderr_handle, buffer, byte_count, &written, nullptr);
}

}  // namespace repiu::platform::win32
