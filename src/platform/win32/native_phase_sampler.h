#pragma once

#include <cstdint>

namespace repiu::platform::win32
{

struct Win32AotCodeCachePlacement;
struct Win32SharedLiveTelemetry;

struct Win32NativePhaseSample
{
    bool captured = false;
    bool mapped = false;
    // 0 = none, 1 = SuspendThread failed, 2 = GetThreadContext failed,
    // 3 = unsupported architecture.
    std::uint32_t failure_stage = 0;
    std::uint32_t windows_error = 0;
    std::uint32_t eip = 0;
    std::uint32_t guest_eip = 0;
    std::uint32_t eax = 0;
    std::uint32_t ebx = 0;
    std::uint32_t ecx = 0;
    std::uint32_t edx = 0;
    std::uint32_t esi = 0;
    std::uint32_t edi = 0;
    std::uint32_t esp = 0;
    std::uint32_t ebp = 0;
    std::uint32_t eflags = 0;
    // Latest lightweight-VEH transfer endpoints, filled in by the caller:
    // they attribute a host-code (unmapped) sample to a guest location.
    std::uint32_t last_indirect_source = 0;
    std::uint32_t last_indirect_target = 0;
};

struct Win32NativePhaseSamplerState
{
    static constexpr std::uint32_t kRingCapacity = 8;
    std::uint32_t sample_count = 0;
    std::uint32_t unmapped_count = 0;
    std::uint32_t ring[kRingCapacity] = {};
    std::uint32_t ring_mapped_bits = 0;
    std::uint32_t ring_cursor = 0;
    std::uint32_t last_sample_tick = 0;
};

// Suspends the guest thread, captures EIP and the integer registers, and
// resumes it. The EIP is reverse-mapped to a guest address only when it lies
// inside the AOT cache range: the translation worker mutates the placement
// only while the guest thread is blocked waiting on it, so a guest thread
// running inside the cache guarantees the address map is stable. Nothing
// between suspend and resume allocates, locks, or performs I/O. When
// telemetry is provided, native_sample_stage marks each capture step so an
// external reader can locate a frozen step.
bool CaptureWin32NativePhaseSample(void* thread,
                                   const Win32AotCodeCachePlacement* placement,
                                   Win32SharedLiveTelemetry* telemetry,
                                   Win32NativePhaseSample* sample);

// Updates the sampler ring and publishes the sample to shared telemetry
// (both optional consumers of a successful capture).
void RecordWin32NativePhaseSample(const Win32NativePhaseSample& sample,
                                  Win32NativePhaseSamplerState* state,
                                  Win32SharedLiveTelemetry* telemetry);

// Writes one bounded "[repiu-sample]" line to stderr without heap use.
void WriteWin32NativePhaseSampleLine(
    const Win32NativePhaseSample& sample,
    const Win32NativePhaseSamplerState& state,
    std::uint32_t elapsed_milliseconds);

}  // namespace repiu::platform::win32
