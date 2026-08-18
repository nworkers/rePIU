#ifndef REPIU_RUNTIME_EXECUTION_TIMEOUT_H_
#define REPIU_RUNTIME_EXECUTION_TIMEOUT_H_

#include <cstdint>

namespace repiu::runtime
{

// Task 435: the guest execution budget policy. The unit is milliseconds and `0`
// is the sentinel for "no limit" -- not a new rule, but the meaning
// `REPIU_EXECUTION_TIMEOUT_MS=0` already had. The host maps it onto Win32
// `INFINITE`, which stays there because it belongs to the wait API.
inline constexpr std::uint32_t kUnlimitedExecutionTimeoutMilliseconds = 0U;

// The budget when nothing is set. The former 1,000 ms dates from when the only
// question was whether the guest executed at all; with the render loop, timer
// ticks and CD audio connected, no real path finishes inside one second.
// Automation that needs a bound now states its own.
inline constexpr std::uint32_t kDefaultExecutionTimeoutMilliseconds =
    kUnlimitedExecutionTimeoutMilliseconds;

// Stall detection is diagnostic policy, not an implied side effect of a finite
// execution budget. It is disabled until a caller explicitly supplies a
// positive quiet-time budget.
inline constexpr std::uint32_t kDefaultStallTimeoutMilliseconds = 0U;

struct ExecutionProgressSnapshot
{
    std::uint32_t diagnostic = 0;
    std::uint32_t single_step = 0;
    std::uint32_t aot = 0;
    std::uint32_t glide_direct = 0;
};

// Resolves one environment value into a millisecond budget. Unset, empty and
// unparsable values all yield the default. Unlike the backend, a malformed
// value does not stop the run: the startup log states the budget actually in
// effect, and failing a measurement script on a typo is the worse outcome.
std::uint32_t ResolveExecutionTimeoutMilliseconds(const char* value);

std::uint32_t ResolveStallTimeoutMilliseconds(const char* value);

bool HasExecutionProgress(const ExecutionProgressSnapshot& previous,
                          const ExecutionProgressSnapshot& current);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_EXECUTION_TIMEOUT_H_
