#include "repiu/platform/win32/aot_timer_source_profile.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace repiu::platform::win32
{
namespace
{

bool ReadAotTimerSourceProfileSetting()
{
    const char* value = std::getenv("REPIU_AOT_TIMER_SOURCE_PROFILE");
    return value != nullptr &&
        ResolveAotTimerSourceProfileEnabled(value);
}

std::uint32_t SaturatingIncrement(std::uint32_t value)
{
    return value == std::numeric_limits<std::uint32_t>::max() ?
        value : value + 1U;
}

}  // namespace

bool ResolveAotTimerSourceProfileEnabled(std::string_view setting)
{
    return setting == "1" || setting == "on" || setting == "true";
}

bool AotTimerSourceProfileEnabled()
{
    static const bool enabled = ReadAotTimerSourceProfileSetting();
    return enabled;
}

void InitializeAotTimerSourceProfile(
    bool enabled,
    Win32AotTimerSourceProfile* profile)
{
    if (profile == nullptr)
    {
        return;
    }
    *profile = Win32AotTimerSourceProfile{};
    profile->enabled = enabled;
}

void RecordAotTimerSourceEvent(
    Win32AotTimerSourceProfile* profile,
    std::uint32_t guest_source,
    std::uint32_t global_tick,
    bool injected,
    std::uint32_t attributed_ticks)
{
    if (profile == nullptr || !profile->enabled || guest_source == 0U)
    {
        return;
    }

    Win32AotTimerSourceProfileEntry* entry = nullptr;
    for (std::uint32_t index = 0; index < profile->entry_count; ++index)
    {
        if (profile->entries[index].guest_source == guest_source)
        {
            entry = &profile->entries[index];
            break;
        }
    }
    if (entry == nullptr)
    {
        if (profile->entry_count >= kAotTimerSourceProfileCapacity)
        {
            profile->overflow_count =
                SaturatingIncrement(profile->overflow_count);
            return;
        }
        entry = &profile->entries[profile->entry_count++];
        entry->guest_source = guest_source;
        entry->first_global_tick = global_tick;
    }

    entry->trap_count = SaturatingIncrement(entry->trap_count);
    entry->last_global_tick = global_tick;
    if (injected)
    {
        entry->injected_count =
            SaturatingIncrement(entry->injected_count);
        entry->attributed_tick_count += attributed_ticks;
        profile->attributed_tick_count += attributed_ticks;
    }
    else
    {
        entry->deferred_count =
            SaturatingIncrement(entry->deferred_count);
    }
}

std::vector<Win32AotTimerSourceProfileEntry>
BuildAotTimerSourceProfileTopEntries(
    const Win32AotTimerSourceProfile& profile,
    std::uint32_t maximum_count)
{
    std::vector<Win32AotTimerSourceProfileEntry> entries(
        profile.entries, profile.entries + profile.entry_count);
    std::sort(
        entries.begin(), entries.end(),
        [](const Win32AotTimerSourceProfileEntry& left,
           const Win32AotTimerSourceProfileEntry& right) {
            if (left.attributed_tick_count !=
                right.attributed_tick_count)
            {
                return left.attributed_tick_count >
                    right.attributed_tick_count;
            }
            if (left.injected_count != right.injected_count)
            {
                return left.injected_count > right.injected_count;
            }
            if (left.trap_count != right.trap_count)
            {
                return left.trap_count > right.trap_count;
            }
            return left.guest_source < right.guest_source;
        });
    if (entries.size() > maximum_count)
    {
        entries.resize(maximum_count);
    }
    return entries;
}

}  // namespace repiu::platform::win32
