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

// Task 503d-19. What the host poll loop concluded.
//
// It used to answer with Win32 wait codes -- WAIT_OBJECT_0, WAIT_TIMEOUT,
// WAIT_FAILED and WAIT_ABANDONED_0 standing in for "the window closed" -- which
// are constants belonging to a wait this loop does not perform. It polls, and
// pumps Glide commands and delivers timer ticks between the questions.
//
// Naming the four outcomes is the same move 3b made when it answered `readable`
// instead of a protection bitmask, and 3d-18 when it answered `running` instead
// of STILL_ACTIVE.
enum class HostPollOutcome
{
    // The guest thread stopped by itself; the exit code is filled in.
    kGuestThreadExited,
    // The execution budget or the stall budget ran out.
    kTimedOut,
    // The host window asked to close, which is not a failure.
    kHostExitRequested,
    // The loop could not do its job -- a thread it was not given, and nothing
    // else today.
    kFailed,
};

#if defined(_WIN32)
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
#endif

// Task 503d-18: the thread is the layer's handle, and the loop asks it whether
// the guest is still running rather than reading GetExitCodeThread and comparing
// against a sentinel that is also a legal exit code.
// Task 503d-19: milliseconds are milliseconds on both hosts, and the outcome is
// named rather than borrowed from a wait. It left the fence with them -- the
// host loop is what drives a run, so it has to exist on the host that is being
// brought up.
HostPollOutcome PollThreadUntilExit(const repiu::platform::HostThread& thread,
                                    std::uint32_t timeout_milliseconds,
                                    std::uint32_t stall_timeout_milliseconds,
                                    ThreadContext* progress_context,
                                    ThreadContext* host_context,
                                    std::uint32_t* exit_code,
                                    bool* stall_timed_out);

#if defined(_WIN32)
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
