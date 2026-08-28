#ifndef REPIU_PLATFORM_WIN32_AOT_RETIRED_TRAP_PROFILE_H_
#define REPIU_PLATFORM_WIN32_AOT_RETIRED_TRAP_PROFILE_H_

#include <array>
#include <cstdint>
#include <string_view>
#include <unordered_map>

namespace repiu::engine
{

struct Win32AotCodeCachePlacement;

constexpr std::uint32_t kWin32AotRetiredTrapHotspotCapacity = 16U;
constexpr std::uint32_t kWin32AotRetiredTrapHistogramCapacity = 65536U;

enum class AotRetiredTrapResolution : std::uint32_t
{
    kActiveHit = 0,
    kGenerationPublished,
    kQuarantined,
    kGenerationFailure,
    kFallback,
    kTraceSentinel,
    kCount,
};

constexpr std::uint32_t kAotRetiredTrapResolutionCount =
    static_cast<std::uint32_t>(AotRetiredTrapResolution::kCount);

struct Win32AotRetiredTrapCacheSample
{
    std::uint32_t cache_address = 0;
    std::uint32_t guest_address = 0;
    std::uint32_t trap_count = 0;
    std::uint32_t generation = 0;
    std::uint8_t guest_length = 0;
    std::uint8_t emitted_length = 0;
    bool metadata_valid = false;
};

struct Win32AotRetiredTrapProfile
{
    bool enabled = false;
    std::uint32_t total_trap_count = 0;
    std::uint32_t relinkable_trap_count = 0;
    std::uint32_t short_trap_count = 0;
    std::uint32_t metadata_miss_count = 0;
    std::uint32_t guest_histogram_overflow_count = 0;
    std::uint32_t cache_histogram_overflow_count = 0;
    std::array<std::uint32_t, kAotRetiredTrapResolutionCount>
        resolution_counts = {};
    std::unordered_map<std::uint32_t, std::uint32_t> guest_histogram;
    std::unordered_map<std::uint32_t, Win32AotRetiredTrapCacheSample>
        cache_histogram;
};

struct Win32AotRetiredTrapGuestHotspot
{
    std::uint32_t guest_address = 0;
    std::uint32_t trap_count = 0;
};

struct Win32AotRetiredTrapProfileSnapshot
{
    bool enabled = false;
    std::uint32_t total_trap_count = 0;
    std::uint32_t distinct_guest_count = 0;
    std::uint32_t distinct_cache_count = 0;
    std::uint32_t top_guest_coverage_count = 0;
    std::uint32_t relinkable_trap_count = 0;
    std::uint32_t short_trap_count = 0;
    std::uint32_t metadata_miss_count = 0;
    std::uint32_t guest_histogram_overflow_count = 0;
    std::uint32_t cache_histogram_overflow_count = 0;
    std::array<std::uint32_t, kAotRetiredTrapResolutionCount>
        resolution_counts = {};
    std::uint32_t guest_hotspot_count = 0;
    std::uint32_t cache_hotspot_count = 0;
    std::array<Win32AotRetiredTrapGuestHotspot,
               kWin32AotRetiredTrapHotspotCapacity> guest_hotspots = {};
    std::array<Win32AotRetiredTrapCacheSample,
               kWin32AotRetiredTrapHotspotCapacity> cache_hotspots = {};
};

bool ResolveAotRetiredTrapProfileEnabled(std::string_view setting);
bool AotRetiredTrapProfileEnabled();
void RecordAotRetiredTrap(
    Win32AotRetiredTrapProfile* profile,
    const Win32AotCodeCachePlacement& placement,
    std::uint32_t cache_address,
    std::uint32_t guest_address);
void RecordAotRetiredTrapResolution(
    Win32AotRetiredTrapProfile* profile,
    AotRetiredTrapResolution resolution);
Win32AotRetiredTrapProfileSnapshot SnapshotAotRetiredTrapProfile(
    const Win32AotRetiredTrapProfile& profile);

} // namespace repiu::engine

#endif
