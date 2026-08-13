#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace repiu::platform::win32
{

struct Win32AotCodeCachePlacement;

constexpr std::uint32_t kAotReturnMegamorphicMissThreshold = 16U;
constexpr std::size_t kAotReturnMegamorphicTargetCapacity = 8U;

struct Win32AotReturnPatchSiteState
{
    std::uint32_t miss_count = 0;
    std::array<std::uint32_t, kAotReturnMegamorphicTargetCapacity> targets{};
    std::uint32_t target_count = 0;
    bool megamorphic = false;
};

struct Win32AotReturnPatchPolicy
{
    std::vector<Win32AotReturnPatchSiteState> sites;
    std::uint32_t observation_count = 0;
    std::uint32_t megamorphic_site_count = 0;
    std::uint32_t bypass_count = 0;
};

enum class AotReturnPatchAction : std::uint8_t
{
    kPatch = 0,
    kBypass,
};

void SyncAotReturnPatchPolicy(Win32AotCodeCachePlacement* placement);

AotReturnPatchAction ObserveAotReturnPatchMiss(
    Win32AotCodeCachePlacement* placement,
    std::uint32_t site_index,
    std::uint32_t guest_target);

}  // namespace repiu::platform::win32
