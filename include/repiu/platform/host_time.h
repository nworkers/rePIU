#ifndef REPIU_PLATFORM_HOST_TIME_H_
#define REPIU_PLATFORM_HOST_TIME_H_

#include <chrono>
#include <cstdint>

// Task 503d-3. A millisecond counter that only differences are taken of.
//
// The engine reads `GetTickCount` at eleven places, and every one of them
// subtracts two readings: how long a snapshot has been running, how long the
// guest has been quiet, whether a timeout has passed. None reads an absolute
// time, which is what makes this replaceable at all -- `GetTickCount` counts
// from boot and `steady_clock` from an unspecified origin, and that difference
// cannot be seen by a caller that only ever subtracts.
//
// The 32-bit width is kept deliberately. It wraps, roughly every 49 days, and
// unsigned subtraction gives the right answer across the wrap; widening it
// would change the arithmetic the call sites already rely on.

namespace repiu::platform
{

inline std::uint32_t MillisecondTicks()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return static_cast<std::uint32_t>(milliseconds);
}

// A cheap monotonic counter for measuring short intervals.
//
// The engine reads the timestamp counter directly where it can, because these
// sit inside paths measured in cycles -- the worker wake latency, the Glide gate
// timings, the JAMMA port scan. Where it cannot, a steady clock stands in; the
// callers all take differences, so the unit only has to be consistent with
// itself.
//
// This was the same six lines written out in seven files, each guarding
// <intrin.h> and falling back the same way. Task 503d-8 collapsed them, which
// the port surfaced by finding the one file that had been left out.
[[nodiscard]] std::uint64_t ReadCycleCounter();

// A high-resolution counter and the frequency to interpret it with, kept as a
// pair because the caller converts microseconds into ticks and back.
//
// Windows keeps its own performance counter, so that is what it uses; nothing
// about the timing changes there. Elsewhere the counter is nanoseconds from a
// steady clock, and the frequency says so.
[[nodiscard]] std::int64_t PerformanceCounterTicks();
[[nodiscard]] std::int64_t PerformanceCounterFrequency();

// The host's local wall clock, for the DOS date and time services.
//
// Field names and widths follow Windows' SYSTEMTIME, because the three call
// sites were written against it and read the parts straight into guest
// registers. Only the fields those sites use are here; a caller wanting more
// should add them rather than reach for the host's own structure.
struct LocalWallClock
{
    std::uint16_t year = 1980U;
    std::uint16_t month = 1U;
    std::uint16_t day = 1U;
    std::uint16_t hour = 0U;
    std::uint16_t minute = 0U;
    std::uint16_t second = 0U;
    std::uint16_t milliseconds = 0U;
};

// Local time, not UTC: DOS has no notion of a time zone, and a guest that asks
// the date expects the one on the wall behind the cabinet.
LocalWallClock ReadLocalWallClock();

// Task 503d-19. Giving the processor up for a moment, as the host poll loop
// does between questions.
//
// Zero and one are not the same request and the distinction is load-bearing
// here. Zero yields -- Task 366 uses it in the final millisecond before a 240Hz
// timer edge so the next iteration observes that edge at high resolution -- and
// one is a short wait, which is what the loop does when the next edge is
// distant. Task 333 replaced an unconditional one-millisecond sleep on this
// path with a command wait and measured the gate cost down, so anything that
// collapses the two would undo a measurement.
void YieldMilliseconds(std::uint32_t milliseconds);

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_HOST_TIME_H_
