#pragma once

#include "repiu/platform/win32/execution_time_profile.h"

#include <cstdint>

namespace repiu::platform::win32
{

// Task 511. The execution time attribution, printed while the run is going.
//
// Everything this engine measures has until now been reported by the loader
// after the guest thread stopped. On Linux a run that reaches rendering never
// stops it -- twenty-one runs across Tasks 509 and 510 all reported
// `stopped=0`, and that arm ends at `_Exit` (Task 508). So the attribution that
// would say where a frame's time goes could not be read on the host where the
// question was being asked.
//
// **This runs on the guest thread**, which is the thread that writes every
// counter it reads (the scopes in the boundary handler). That is the whole
// reason it needs no lock and cannot tear: writer and reader are the same
// thread. A reporter on any other thread would need both.
//
// It is called at one place only -- the `grBufferSwap` gate, once a frame --
// because a clock read on every gate entry would make the instrument part of
// what it measures. Task 353 set that rule and the profile's own comments cite
// it.
//
// Reports are spaced by `REPIU_LIVE_PROFILE_INTERVAL_MS`. Unset or zero is off,
// and off costs one comparison a frame.

// True when the interval is set to something usable. Cheap; the environment is
// read once.
[[nodiscard]] bool LiveExecutionProfileReportEnabled();

// Emits one line if the interval has elapsed since the last one. `profile` may
// be null, which is the case when the profile itself was never enabled -- there
// is then nothing to report and this returns immediately.
//
// `frames` is the run's presented-frame count, carried so the line can state
// cycles per frame rather than leaving the reader to divide by a number printed
// somewhere else.
void ReportLiveExecutionProfileIfDue(
    const Win32ExecutionTimeProfile* profile,
    std::uint64_t frames);

}  // namespace repiu::platform::win32
