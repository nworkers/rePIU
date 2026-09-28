#ifndef REPIU_RUNTIME_CYCLE_CLOCK_H_
#define REPIU_RUNTIME_CYCLE_CLOCK_H_

#include <chrono>
#include <cstdint>

// ReadCycleCounter() -> std::uint64_t, defined per platform.
//
// Task 330: a platform-neutral cycle source so platform-neutral code can be
// attributed without pulling in a platform header. Mirrors the semantics of
// `engine::ReadAotWorkerTimingCycles`, which stays where it is because
// it is only used by Win32 rendezvous code.
//
// The unit is a TSC tick where one exists and a steady_clock tick otherwise, so
// values are comparable only within one process and one build.
//
// Task 758. Selection point: the definitions are per platform.
#if defined(_WIN32)
#include "repiu/runtime/win32/cycle_clock_win32.h"
#elif defined(__EMSCRIPTEN__)
#include "repiu/runtime/web/cycle_clock.h"
#else
#include "repiu/runtime/linux/cycle_clock.h"
#endif

namespace repiu::runtime
{

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
