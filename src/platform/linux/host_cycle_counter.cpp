#include "repiu/platform/host_time.h"

// Task 758. The Linux cycle counter: the time-stamp counter, on i386 and
// x86-64 alike.

#include <x86intrin.h>

namespace repiu::platform
{

std::uint64_t ReadCycleCounter()
{
    return __rdtsc();
}

}  // namespace repiu::platform
