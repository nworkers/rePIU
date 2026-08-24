#include "repiu/platform/host_time.h"

#include <ctime>

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#elif defined(__i386__) || defined(__x86_64__)
#include <x86intrin.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#endif

namespace repiu::platform
{

std::uint64_t ReadCycleCounter()
{
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    return __rdtsc();
#elif defined(__i386__) || defined(__x86_64__)
    return __rdtsc();
#else
    // Not cycles, but monotonic and fine-grained, which is all the callers ask
    // of it: every one of them subtracts two readings.
    return static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

std::int64_t PerformanceCounterFrequency()
{
    static const std::int64_t frequency = []() -> std::int64_t {
#if defined(_WIN32)
        LARGE_INTEGER value = {};
        QueryPerformanceFrequency(&value);
        return value.QuadPart != 0 ? value.QuadPart : 1;
#else
        // steady_clock is nanoseconds on every implementation this builds
        // against, and the ratio says so rather than the number being assumed.
        return static_cast<std::int64_t>(
            std::chrono::steady_clock::period::den /
            std::chrono::steady_clock::period::num);
#endif
    }();
    return frequency;
}

std::int64_t PerformanceCounterTicks()
{
#if defined(_WIN32)
    LARGE_INTEGER value = {};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
#else
    return static_cast<std::int64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

LocalWallClock ReadLocalWallClock()
{
    LocalWallClock wall_clock;

    const auto now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);

    std::tm parts{};
#if defined(_WIN32)
    if (localtime_s(&parts, &seconds) != 0)
    {
        // The defaults are the DOS epoch, which is what the guest sees if the
        // host cannot say what day it is. Reporting a plausible-looking wrong
        // date would be worse.
        return wall_clock;
    }
#else
    if (localtime_r(&seconds, &parts) == nullptr)
    {
        return wall_clock;
    }
#endif

    wall_clock.year = static_cast<std::uint16_t>(parts.tm_year + 1900);
    wall_clock.month = static_cast<std::uint16_t>(parts.tm_mon + 1);
    wall_clock.day = static_cast<std::uint16_t>(parts.tm_mday);
    wall_clock.hour = static_cast<std::uint16_t>(parts.tm_hour);
    wall_clock.minute = static_cast<std::uint16_t>(parts.tm_min);
    // A leap second would report 60 here. DOS has no room for it, so it is
    // clamped rather than handed to a guest that would encode it wrongly.
    wall_clock.second = static_cast<std::uint16_t>(parts.tm_sec > 59 ? 59
                                                                     : parts.tm_sec);

    // The sub-second part comes from the same reading, not a second call to the
    // clock, so the milliseconds cannot belong to a different second than the
    // fields above.
    const auto since_epoch = now.time_since_epoch();
    const auto whole_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(since_epoch);
    const auto remainder =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            since_epoch - whole_seconds);
    wall_clock.milliseconds =
        static_cast<std::uint16_t>(remainder.count());

    return wall_clock;
}

}  // namespace repiu::platform
