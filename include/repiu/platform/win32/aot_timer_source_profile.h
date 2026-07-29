#ifndef REPIU_PLATFORM_WIN32_AOT_TIMER_SOURCE_PROFILE_H_
#define REPIU_PLATFORM_WIN32_AOT_TIMER_SOURCE_PROFILE_H_

#include <cstdint>
#include <string_view>
#include <vector>

namespace repiu::platform::win32
{

constexpr std::uint32_t kAotTimerSourceProfileCapacity = 1024U;

struct Win32AotTimerSourceProfileEntry
{
    std::uint32_t guest_source = 0;
    std::uint32_t trap_count = 0;
    std::uint32_t injected_count = 0;
    std::uint32_t deferred_count = 0;
    std::uint64_t attributed_tick_count = 0;
    std::uint32_t first_global_tick = 0;
    std::uint32_t last_global_tick = 0;
};

struct Win32AotTimerSourceProfile
{
    bool enabled = false;
    std::uint32_t entry_count = 0;
    std::uint32_t overflow_count = 0;
    std::uint64_t attributed_tick_count = 0;
    Win32AotTimerSourceProfileEntry
        entries[kAotTimerSourceProfileCapacity] = {};
};

bool ResolveAotTimerSourceProfileEnabled(std::string_view setting);
bool AotTimerSourceProfileEnabled();

void InitializeAotTimerSourceProfile(
    bool enabled,
    Win32AotTimerSourceProfile* profile);

void RecordAotTimerSourceEvent(
    Win32AotTimerSourceProfile* profile,
    std::uint32_t guest_source,
    std::uint32_t global_tick,
    bool injected,
    std::uint32_t attributed_ticks);

std::vector<Win32AotTimerSourceProfileEntry>
BuildAotTimerSourceProfileTopEntries(
    const Win32AotTimerSourceProfile& profile,
    std::uint32_t maximum_count = kAotTimerSourceProfileCapacity);

}  // namespace repiu::platform::win32

#endif
