#include "repiu/runtime/execution_timeout.h"

#include <charconv>
#include <string_view>

namespace repiu::runtime
{
namespace
{

std::uint32_t ResolveMilliseconds(const char* value,
                                  std::uint32_t default_milliseconds)
{
    if (value == nullptr || *value == '\0')
    {
        return default_milliseconds;
    }

    const std::string_view text(value);
    std::uint32_t milliseconds = 0;
    const auto result = std::from_chars(
        text.data(), text.data() + text.size(), milliseconds);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
    {
        return default_milliseconds;
    }
    return milliseconds;
}

}  // namespace

std::uint32_t ResolveExecutionTimeoutMilliseconds(const char* value)
{
    // The whole string must be the number. Reading `1000ms` as 1,000 would run
    // a procedure on a budget it did not ask for.
    return ResolveMilliseconds(value, kDefaultExecutionTimeoutMilliseconds);
}

std::uint32_t ResolveStallTimeoutMilliseconds(const char* value)
{
    return ResolveMilliseconds(value, kDefaultStallTimeoutMilliseconds);
}

bool HasExecutionProgress(const ExecutionProgressSnapshot& previous,
                          const ExecutionProgressSnapshot& current)
{
    return previous.diagnostic != current.diagnostic ||
        previous.single_step != current.single_step ||
        previous.aot != current.aot ||
        previous.glide_direct != current.glide_direct;
}

}  // namespace repiu::runtime
