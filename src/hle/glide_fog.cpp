#include "repiu/hle/glide_fog.h"

#include <cmath>

namespace repiu::hle
{

namespace
{

float GlideFogTableWorldDistance(std::uint32_t index)
{
    const int exponent = 3 + static_cast<int>(index >> 2U);
    const float denominator = static_cast<float>(8U - (index & 3U));
    return std::ldexp(1.0F, exponent) / denominator;
}

}  // namespace

bool CalculateGlideFogTableSample(float oow,
                                  std::uint32_t* lower_index,
                                  float* fraction)
{
    if (lower_index == nullptr || fraction == nullptr ||
        !std::isfinite(oow) || oow <= 0.0F)
    {
        return false;
    }

    const float world_distance = 1.0F / oow;
    if (!std::isfinite(world_distance) ||
        world_distance >= GlideFogTableWorldDistance(63U))
    {
        *lower_index = 63U;
        *fraction = 0.0F;
        return true;
    }
    if (world_distance <= GlideFogTableWorldDistance(0U))
    {
        *lower_index = 0U;
        *fraction = 0.0F;
        return true;
    }

    for (std::uint32_t index = 0U; index < 63U; ++index)
    {
        const float lower_distance = GlideFogTableWorldDistance(index);
        const float upper_distance = GlideFogTableWorldDistance(index + 1U);
        if (world_distance <= upper_distance)
        {
            *lower_index = index;
            *fraction = (world_distance - lower_distance) /
                (upper_distance - lower_distance);
            return true;
        }
    }

    *lower_index = 63U;
    *fraction = 0.0F;
    return true;
}

float EvaluateGlideFogTable(const GlideFogTable& table, float oow)
{
    std::uint32_t lower_index = 0U;
    float fraction = 0.0F;
    if (!CalculateGlideFogTableSample(oow, &lower_index, &fraction))
    {
        return 0.0F;
    }
    const float lower =
        static_cast<float>(table[lower_index]) / 255.0F;
    if (lower_index == 63U)
    {
        return lower;
    }
    const float upper =
        static_cast<float>(table[lower_index + 1U]) / 255.0F;
    return lower + (upper - lower) * fraction;
}

}  // namespace repiu::hle
