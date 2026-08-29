#ifndef REPIU_ENGINE_AOT_TIMER_SOURCE_PROFILE_H_
#define REPIU_ENGINE_AOT_TIMER_SOURCE_PROFILE_H_

#include <cstdint>
#include <string_view>
#include <vector>

namespace repiu::engine
{

constexpr std::uint32_t kAotTimerSourceProfileCapacity = 1024U;

struct AotTimerSourceProfileEntry
{
    std::uint32_t guest_source = 0;
    std::uint32_t trap_count = 0;
    std::uint32_t injected_count = 0;
    std::uint32_t deferred_count = 0;
    std::uint64_t attributed_tick_count = 0;
    std::uint32_t first_global_tick = 0;
    std::uint32_t last_global_tick = 0;
};

struct AotTimerSourceProfile
{
    bool enabled = false;
    std::uint32_t entry_count = 0;
    std::uint32_t overflow_count = 0;
    std::uint64_t attributed_tick_count = 0;
    AotTimerSourceProfileEntry
        entries[kAotTimerSourceProfileCapacity] = {};
};

bool ResolveAotTimerSourceProfileEnabled(std::string_view setting);
bool AotTimerSourceProfileEnabled();

void InitializeAotTimerSourceProfile(
    bool enabled,
    AotTimerSourceProfile* profile);

void RecordAotTimerSourceEvent(
    AotTimerSourceProfile* profile,
    std::uint32_t guest_source,
    std::uint32_t global_tick,
    bool injected,
    std::uint32_t attributed_ticks);

std::vector<AotTimerSourceProfileEntry>
BuildAotTimerSourceProfileTopEntries(
    const AotTimerSourceProfile& profile,
    std::uint32_t maximum_count = kAotTimerSourceProfileCapacity);

}  // namespace repiu::engine

#endif
