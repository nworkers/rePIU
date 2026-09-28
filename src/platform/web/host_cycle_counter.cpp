#include "repiu/platform/host_time.h"

// Task 758. The web cycle counter. wasm has no time-stamp counter.

namespace repiu::platform
{

std::uint64_t ReadCycleCounter()
{
    // Not cycles, but monotonic and fine-grained, which is all the callers ask
    // of it: every one of them subtracts two readings.
    return static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
}

}  // namespace repiu::platform
