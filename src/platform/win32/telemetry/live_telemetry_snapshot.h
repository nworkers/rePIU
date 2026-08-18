#pragma once

// Live telemetry mapping and execution-snapshot helpers extracted from
// execution_trampoline.cpp (Phase 1 increment 4). BuildDosEnvironmentBlock,
// which was interleaved among these functions, stays in the trampoline.

#include "thread_context.h"
#include "native_phase_sampler.h"

#include <cstdint>
#include <vector>
#include <windows.h>

namespace repiu::platform::win32
{

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

DWORD PollThreadUntilExit(HANDLE thread,
                          DWORD timeout_milliseconds,
                          DWORD stall_timeout_milliseconds,
                          ThreadContext* progress_context,
                          ThreadContext* host_context,
                          DWORD* exit_code,
                          bool* stall_timed_out);

SharedTelemetryMapping OpenSharedTelemetryMapping();

void CopySnapshotFromContextRecord(const CONTEXT& source,
                                   X86ExecutionSnapshot* snapshot);

void CopyThreadObservationToAttempt(const ThreadContext& context,
                                    Win32MinimalExecutionAttempt* attempt);

} // namespace repiu::platform::win32
