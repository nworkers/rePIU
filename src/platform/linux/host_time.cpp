#include "repiu/platform/host_time.h"

#include "../host_time_platform.h"

// Task 758. POSIX host time: steady_clock, localtime_r and nanosleep, used by
// the Linux and web builds. The cycle counter is separate because it differs
// between them (host_cycle_counter.cpp here and in web/).

#include <sched.h>
#include <time.h>

#include <cerrno>

namespace repiu::platform
{

std::int64_t PerformanceCounterFrequency()
{
    static const std::int64_t frequency = []() -> std::int64_t {
        // steady_clock is nanoseconds on every implementation this builds
        // against, and the ratio says so rather than the number being assumed.
        return static_cast<std::int64_t>(
            std::chrono::steady_clock::period::den /
            std::chrono::steady_clock::period::num);
    }();
    return frequency;
}

std::int64_t PerformanceCounterTicks()
{
    return static_cast<std::int64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
}

namespace host_time_platform
{

bool ConvertLocalTime(const std::time_t& seconds, std::tm* converted)
{
    return localtime_r(&seconds, converted) != nullptr;
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
    if (milliseconds == 0U)
    {
        sched_yield();
        return;
    }
    timespec request{};
    request.tv_sec = static_cast<time_t>(milliseconds / 1000U);
    request.tv_nsec = static_cast<long>(milliseconds % 1000U) * 1000000L;
    timespec remaining{};
    // A signal shortens the sleep, and the remainder is what is left to serve.
    // Resuming it keeps the loop's cadence from drifting with fault traffic.
    // Only EINTR is resumed: any other failure is a malformed request, and
    // retrying it forever would hang the loop this function exists to pace.
    while (nanosleep(&request, &remaining) != 0 && errno == EINTR)
    {
        request = remaining;
    }
}

}  // namespace repiu::platform
