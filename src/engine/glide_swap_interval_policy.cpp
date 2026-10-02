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

std::uint32_t ResolveGlideSwapPacingPeriodMicroseconds(
    const std::int32_t interval, const double refresh_rate_hz)
{
    const std::int32_t frames = interval < 0 ? 1 : interval;
    if (frames <= 0 || frames > kMaxGlideSwapInterval)
    {
        return 0U;
    }
    const double rate = refresh_rate_hz > 1.0 && refresh_rate_hz <= 1000.0
        ? refresh_rate_hz : 60.0;
    const double period = 1000000.0 * static_cast<double>(frames) / rate;
    return static_cast<std::uint32_t>(period + 0.5);
}

bool ResolveGlideSwapInterval(const char* value, std::int32_t* interval)
{
    if (interval == nullptr)
    {
        return false;
    }
    if (value != nullptr && ResolveGlideSwapIntervalOverride(value, interval))
    {
        return true;
    }
    *interval = kDefaultGlideSwapInterval;
    return false;
}

bool ReadGlideSwapInterval(std::int32_t* interval)
{
    return ResolveGlideSwapInterval(
        std::getenv("REPIU_GLIDE_SWAP_INTERVAL"), interval);
}

}  // namespace repiu::engine
