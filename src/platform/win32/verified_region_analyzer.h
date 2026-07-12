#pragma once

#include <cstdint>
#include <unordered_map>

namespace repiu::platform::win32::detail
{

struct VerifiedRegionFailure
{
    std::uint32_t instruction = 0;
    std::uint32_t opcode = 0;
    std::uint32_t bytes_low = 0;
    std::uint32_t bytes_high = 0;
};

bool VerifyNativeFunctionWithZydis(
    std::uint32_t entry,
    std::uint32_t runtime_base,
    std::uint32_t runtime_size,
    std::unordered_map<std::uint32_t, std::int8_t>* cache,
    VerifiedRegionFailure* failure);

}  // namespace repiu::platform::win32::detail
