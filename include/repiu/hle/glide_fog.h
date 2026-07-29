#ifndef REPIU_HLE_GLIDE_FOG_H_
#define REPIU_HLE_GLIDE_FOG_H_

#include <array>
#include <cstddef>
#include <cstdint>

namespace repiu::hle
{

constexpr std::size_t kGlideFogTableEntryCount = 64U;
using GlideFogTable =
    std::array<std::uint8_t, kGlideFogTableEntryCount>;

bool CalculateGlideFogTableSample(float oow,
                                  std::uint32_t* lower_index,
                                  float* fraction);

float EvaluateGlideFogTable(const GlideFogTable& table, float oow);

}  // namespace repiu::hle

#endif  // REPIU_HLE_GLIDE_FOG_H_
