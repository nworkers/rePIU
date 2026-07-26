#include "repiu/platform/win32/aot_retired_trap_profile.h"

#include "repiu/platform/win32/aot_code_cache_win32.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace repiu::platform::win32
{
namespace
{

bool ReadAotRetiredTrapProfileSetting()
{
    const char* value = std::getenv("REPIU_AOT_RETIRED_TRAP_PROFILE");
    return value != nullptr && ResolveAotRetiredTrapProfileEnabled(value);
}

} // namespace

bool ResolveAotRetiredTrapProfileEnabled(std::string_view setting)
{
    return setting == "1" || setting == "on" || setting == "true";
}

bool AotRetiredTrapProfileEnabled()
{
    static const bool enabled = ReadAotRetiredTrapProfileSetting();
    return enabled;
}

void RecordAotRetiredTrap(
    Win32AotRetiredTrapProfile* profile,
    const Win32AotCodeCachePlacement& placement,
    std::uint32_t cache_address,
    std::uint32_t guest_address)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->enabled = true;
    ++profile->total_trap_count;

    const auto guest = profile->guest_histogram.find(guest_address);
    if (guest != profile->guest_histogram.end())
    {
        ++guest->second;
    }
    else if (profile->guest_histogram.size() <
             kWin32AotRetiredTrapHistogramCapacity)
    {
        profile->guest_histogram.emplace(guest_address, 1U);
    }
    else
    {
        ++profile->guest_histogram_overflow_count;
    }

    const auto cached = profile->cache_histogram.find(cache_address);
    if (cached != profile->cache_histogram.end())
    {
        ++cached->second.trap_count;
        if (!cached->second.metadata_valid)
        {
            ++profile->metadata_miss_count;
        }
        else if (cached->second.emitted_length >= 5U)
        {
            ++profile->relinkable_trap_count;
        }
        else
        {
            ++profile->short_trap_count;
        }
        return;
    }
    if (profile->cache_histogram.size() >=
        kWin32AotRetiredTrapHistogramCapacity)
    {
        ++profile->cache_histogram_overflow_count;
        return;
    }

    Win32AotRetiredTrapCacheSample sample;
    sample.cache_address = cache_address;
    sample.guest_address = guest_address;
    sample.trap_count = 1U;
    bool metadata_found = false;
    if (placement.placed && cache_address >= placement.base_address)
    {
        const std::uint32_t cache_offset =
            cache_address - placement.base_address;
        const auto map_position =
            placement.inactive_map_index_by_cache_offset.find(cache_offset);
        if (map_position !=
                placement.inactive_map_index_by_cache_offset.end() &&
            map_position->second < placement.address_map.size() &&
            map_position->second < placement.address_map_states.size())
        {
            const runtime::AotAddressMapEntry& entry =
                placement.address_map[map_position->second];
            const Win32AotAddressMapState& state =
                placement.address_map_states[map_position->second];
            sample.guest_address = entry.guest_address;
            sample.generation = state.generation;
            sample.guest_length = entry.guest_length;
            sample.emitted_length = entry.emitted_length;
            sample.metadata_valid = true;
            metadata_found = true;
        }
    }
    if (!metadata_found)
    {
        ++profile->metadata_miss_count;
    }
    else if (sample.emitted_length >= 5U)
    {
        ++profile->relinkable_trap_count;
    }
    else
    {
        ++profile->short_trap_count;
    }
    profile->cache_histogram.emplace(cache_address, sample);
}

void RecordAotRetiredTrapResolution(
    Win32AotRetiredTrapProfile* profile,
    AotRetiredTrapResolution resolution)
{
    if (profile == nullptr)
    {
        return;
    }
    const std::uint32_t index = static_cast<std::uint32_t>(resolution);
    if (index < profile->resolution_counts.size())
    {
        ++profile->resolution_counts[index];
    }
}

Win32AotRetiredTrapProfileSnapshot SnapshotAotRetiredTrapProfile(
    const Win32AotRetiredTrapProfile& profile)
{
    Win32AotRetiredTrapProfileSnapshot snapshot;
    snapshot.enabled = profile.enabled;
    snapshot.total_trap_count = profile.total_trap_count;
    snapshot.distinct_guest_count = static_cast<std::uint32_t>(
        profile.guest_histogram.size());
    snapshot.distinct_cache_count = static_cast<std::uint32_t>(
        profile.cache_histogram.size());
    snapshot.relinkable_trap_count = profile.relinkable_trap_count;
    snapshot.short_trap_count = profile.short_trap_count;
    snapshot.metadata_miss_count = profile.metadata_miss_count;
    snapshot.guest_histogram_overflow_count =
        profile.guest_histogram_overflow_count;
    snapshot.cache_histogram_overflow_count =
        profile.cache_histogram_overflow_count;
    snapshot.resolution_counts = profile.resolution_counts;

    std::vector<Win32AotRetiredTrapGuestHotspot> guest_hotspots;
    guest_hotspots.reserve(profile.guest_histogram.size());
    for (const auto& entry : profile.guest_histogram)
    {
        guest_hotspots.push_back({entry.first, entry.second});
    }
    std::sort(guest_hotspots.begin(), guest_hotspots.end(),
              [](const auto& left, const auto& right) {
                  return left.trap_count != right.trap_count
                      ? left.trap_count > right.trap_count
                      : left.guest_address < right.guest_address;
              });
    snapshot.guest_hotspot_count = std::min<std::uint32_t>(
        static_cast<std::uint32_t>(guest_hotspots.size()),
        kWin32AotRetiredTrapHotspotCapacity);
    for (std::uint32_t index = 0;
         index < snapshot.guest_hotspot_count; ++index)
    {
        snapshot.guest_hotspots[index] = guest_hotspots[index];
        snapshot.top_guest_coverage_count += guest_hotspots[index].trap_count;
    }

    std::vector<Win32AotRetiredTrapCacheSample> cache_hotspots;
    cache_hotspots.reserve(profile.cache_histogram.size());
    for (const auto& entry : profile.cache_histogram)
    {
        cache_hotspots.push_back(entry.second);
    }
    std::sort(cache_hotspots.begin(), cache_hotspots.end(),
              [](const auto& left, const auto& right) {
                  return left.trap_count != right.trap_count
                      ? left.trap_count > right.trap_count
                      : left.cache_address < right.cache_address;
              });
    snapshot.cache_hotspot_count = std::min<std::uint32_t>(
        static_cast<std::uint32_t>(cache_hotspots.size()),
        kWin32AotRetiredTrapHotspotCapacity);
    for (std::uint32_t index = 0;
         index < snapshot.cache_hotspot_count; ++index)
    {
        snapshot.cache_hotspots[index] = cache_hotspots[index];
    }
    return snapshot;
}

} // namespace repiu::platform::win32
