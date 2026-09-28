#ifndef REPIU_RUNTIME_WIN32_CYCLE_CLOCK_WIN32_H_
#define REPIU_RUNTIME_WIN32_CYCLE_CLOCK_WIN32_H_

// Task 758. The Win32 cycle source: the MSVC time-stamp counter intrinsic.
// Include cycle_clock.h, not this.

#include <cstdint>

#include <intrin.h>

namespace repiu::runtime
{

inline std::uint64_t ReadCycleCounter()
{
    return __rdtsc();
}

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_WIN32_CYCLE_CLOCK_WIN32_H_
