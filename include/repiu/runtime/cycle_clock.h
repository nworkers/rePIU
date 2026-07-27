#ifndef REPIU_RUNTIME_CYCLE_CLOCK_H_
#define REPIU_RUNTIME_CYCLE_CLOCK_H_

#include <chrono>
#include <cstdint>

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#endif

namespace repiu::runtime
{

// Task 330: a platform-neutral cycle source so platform-neutral code can be
// attributed without pulling in a platform header. Mirrors the semantics of
// `platform::win32::ReadAotWorkerTimingCycles`, which stays where it is because
// it is only used by Win32 rendezvous code.
//
// The unit is a TSC tick where one exists and a steady_clock tick otherwise, so
// values are comparable only within one process and one build.
inline std::uint64_t ReadCycleCounter()
{
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    return __rdtsc();
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    return __builtin_ia32_rdtsc();
#else
    return static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

// A TSC read can move backwards across cores. Such a sample is dropped rather
// than wrapped, and counted so a run can show whether it happened at all.
inline std::uint64_t CycleDelta(std::uint64_t start,
                                std::uint64_t end,
                                std::uint32_t* clamped_count = nullptr)
{
    if (end >= start)
    {
        return end - start;
    }
    if (clamped_count != nullptr)
    {
        ++*clamped_count;
    }
    return 0U;
}

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_CYCLE_CLOCK_H_
