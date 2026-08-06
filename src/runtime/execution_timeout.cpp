#include "repiu/runtime/execution_timeout.h"

#include <charconv>
#include <string_view>

namespace repiu::runtime
{

std::uint32_t ResolveExecutionTimeoutMilliseconds(const char* value)
{
    if (value == nullptr || *value == '\0')
    {
        return kDefaultExecutionTimeoutMilliseconds;
    }

    const std::string_view text(value);
    std::uint32_t milliseconds = 0;
    const auto result = std::from_chars(
        text.data(), text.data() + text.size(), milliseconds);
    // The whole string must be the number. Reading `1000ms` as 1,000 would run
    // a procedure on a budget it did not ask for.
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
    {
        return kDefaultExecutionTimeoutMilliseconds;
    }
    return milliseconds;
}

}  // namespace repiu::runtime
