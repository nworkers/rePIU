#ifndef REPIU_RUNTIME_LINUX_CYCLE_CLOCK_H_
#define REPIU_RUNTIME_LINUX_CYCLE_CLOCK_H_

// Task 758. The Linux cycle source: the GCC time-stamp counter builtin, on
// i386 and x86-64 alike.
// Include cycle_clock.h, not this.

#include <cstdint>

namespace repiu::runtime
{

inline std::uint64_t ReadCycleCounter()
{
    return __builtin_ia32_rdtsc();
}

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_LINUX_CYCLE_CLOCK_H_
