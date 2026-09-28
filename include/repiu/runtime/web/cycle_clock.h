#ifndef REPIU_RUNTIME_WEB_CYCLE_CLOCK_H_
#define REPIU_RUNTIME_WEB_CYCLE_CLOCK_H_

// Task 758. The web cycle source: wasm has no time-stamp counter.
// Include cycle_clock.h, not this.

#include <chrono>
#include <cstdint>

namespace repiu::runtime
{

inline std::uint64_t ReadCycleCounter()
{
    return static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
}

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_WEB_CYCLE_CLOCK_H_
