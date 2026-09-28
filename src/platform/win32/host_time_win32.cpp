#include "repiu/platform/host_time.h"

#include "../host_time_platform.h"

// Task 758. Win32 host time: the time-stamp counter, the performance counter,
// localtime_s and Sleep.

#include <intrin.h>
#include <windows.h>

namespace repiu::platform
{

std::uint64_t ReadCycleCounter()
{
    return __rdtsc();
}

std::int64_t PerformanceCounterFrequency()
{
    static const std::int64_t frequency = []() -> std::int64_t {
        LARGE_INTEGER value = {};
        QueryPerformanceFrequency(&value);
        return value.QuadPart != 0 ? value.QuadPart : 1;
    }();
    return frequency;
}

std::int64_t PerformanceCounterTicks()
{
    LARGE_INTEGER value = {};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

namespace host_time_platform
{

bool ConvertLocalTime(const std::time_t& seconds, std::tm* converted)
{
    return localtime_s(converted, &seconds) == 0;
}

}  // namespace host_time_platform

// Task 503d-19. `Sleep(0)` on Windows yields to a ready thread of equal
// priority and returns immediately; `sched_yield` is the POSIX counterpart, and
// `nanosleep` is not -- it would enter the kernel's timer machinery for a
// request that is about scheduling.
//
// For a non-zero request, `nanosleep` rather than `usleep`: the second is
// obsolescent, and the first is the one that resumes correctly after a signal
// without the caller having to think about it. This path is interrupted by
// signals routinely, because that is how the engine delivers faults.
void YieldMilliseconds(const std::uint32_t milliseconds)
{
    Sleep(static_cast<DWORD>(milliseconds));
}

}  // namespace repiu::platform
