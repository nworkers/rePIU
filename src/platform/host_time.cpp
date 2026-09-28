#include "repiu/platform/host_time.h"

#include "host_time_platform.h"

// Task 758. The part of host time every host shares: the local wall clock,
// cached per second. The counters, the calendar conversion and the yield are
// per platform (host_time_platform.h, win32/, linux/, web/).

#include <ctime>

namespace repiu::platform
{

LocalWallClock ReadLocalWallClock()
{
    LocalWallClock wall_clock;

    const auto now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);

    // Task 741. The calendar conversion is the expensive part (a lock and the
    // time-zone rules), and the guest asks for the time far more often than
    // once a second: Watcom's delay() calibrates itself by calling INT 21h
    // AH=2Ch in a tight loop until the seconds change, and pumpitea made
    // 226,000 such calls a second. The broken-down time of one second is the
    // same for every reading inside it, so it is converted once per second
    // and per thread; only the millisecond part below is read every time.
    thread_local std::time_t cached_seconds = -1;
    thread_local std::tm cached_parts{};
    thread_local bool cached_valid = false;
    if (seconds != cached_seconds)
    {
        cached_seconds = seconds;
        std::tm converted{};
        cached_valid = host_time_platform::ConvertLocalTime(seconds, &converted);
        cached_parts = converted;
    }
    if (!cached_valid)
    {
        // The defaults are the DOS epoch, which is what the guest sees if the
        // host cannot say what day it is. Reporting a plausible-looking wrong
        // date would be worse.
        return wall_clock;
    }
    const std::tm& parts = cached_parts;

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
