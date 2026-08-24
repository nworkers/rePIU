#pragma once

// Live telemetry mapping and execution-snapshot helpers extracted from
// execution_trampoline.cpp (Phase 1 increment 4). BuildDosEnvironmentBlock,
// which was interleaved among these functions, stays in the trampoline.

#include "thread_context.h"
#include "native_phase_sampler.h"

#include "repiu/platform/guest_cpu_context.h"
#include "repiu/platform/host_thread.h"

#include <cstdint>
#include <vector>

// Task 503d-14. Fenced, because what is behind it is the cross-process
// diagnostics: a shared section another process maps, and the thread handles a
// watchdog waits on. Neither is needed to run the guest, so Linux starts
// without them rather than with an invented counterpart.
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace repiu::platform::win32
{

#if defined(_WIN32)
constexpr DWORD kWin32HostExitRequested = WAIT_ABANDONED_0;

struct SharedTelemetryMapping
{
    HANDLE mapping = nullptr;
    Win32SharedLiveTelemetry* telemetry = nullptr;

    SharedTelemetryMapping() = default;
    SharedTelemetryMapping(const SharedTelemetryMapping&) = delete;
    SharedTelemetryMapping& operator=(const SharedTelemetryMapping&) = delete;
    SharedTelemetryMapping(SharedTelemetryMapping&& other) noexcept
        : mapping(other.mapping), telemetry(other.telemetry)
    {
        other.mapping = nullptr;
        other.telemetry = nullptr;
    }

    ~SharedTelemetryMapping()
    {
        if (telemetry != nullptr)
        {
            UnmapViewOfFile(telemetry);
        }
        if (mapping != nullptr)
        {
            CloseHandle(mapping);
        }
    }
};

// Task 503d-18: the thread is the layer's handle, and the loop asks it whether
// the guest is still running rather than reading GetExitCodeThread and comparing
// against a sentinel that is also a legal exit code.
DWORD PollThreadUntilExit(const repiu::platform::HostThread& thread,
                          DWORD timeout_milliseconds,
                          DWORD stall_timeout_milliseconds,
                          ThreadContext* progress_context,
                          ThreadContext* host_context,
                          DWORD* exit_code,
                          bool* stall_timed_out);

SharedTelemetryMapping OpenSharedTelemetryMapping();
#endif

// Task 503d-14: CONTEXT becomes GuestCpuContext, which is an alias for it on
// Windows, so neither the definition nor its callers change.
void CopySnapshotFromContextRecord(
    const repiu::platform::GuestCpuContext& source,
    X86ExecutionSnapshot* snapshot);

void CopyThreadObservationToAttempt(const ThreadContext& context,
                                    Win32MinimalExecutionAttempt* attempt);

} // namespace repiu::platform::win32
