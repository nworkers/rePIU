#include "repiu/engine/glide_swap_interval_policy.h"

#include <charconv>
#include <cstdlib>

namespace repiu::engine
{

bool ResolveGlideSwapIntervalOverride(std::string_view setting,
                                      std::int32_t* interval)
{
    if (interval == nullptr || setting.empty())
    {
        return false;
    }
    std::int32_t value = 0;
    const char* begin = setting.data();
    const char* end = begin + setting.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end)
    {
        return false;
    }
    if (value < kMinGlideSwapInterval || value > kMaxGlideSwapInterval)
    {
        return false;
    }
    *interval = value;
    return true;
}

bool TryReadGlideSwapIntervalOverride(std::int32_t* interval)
{
    const char* value = std::getenv("REPIU_GLIDE_SWAP_INTERVAL");
    return value != nullptr &&
        ResolveGlideSwapIntervalOverride(value, interval);
}

}  // namespace repiu::engine
