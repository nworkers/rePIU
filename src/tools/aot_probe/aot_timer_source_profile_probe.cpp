#include "aot_timer_source_profile_probe.h"

#include "repiu/engine/aot_timer_source_profile.h"

#include <iostream>

namespace repiu::tools
{

bool RunAotTimerSourceProfileProbe()
{
    using namespace repiu::engine;

    const bool policy =
        !ResolveAotTimerSourceProfileEnabled("") &&
        ResolveAotTimerSourceProfileEnabled("1") &&
        ResolveAotTimerSourceProfileEnabled("on") &&
        ResolveAotTimerSourceProfileEnabled("true") &&
        !ResolveAotTimerSourceProfileEnabled("0") &&
        !ResolveAotTimerSourceProfileEnabled("invalid");

    AotTimerSourceProfile profile;
    InitializeAotTimerSourceProfile(true, &profile);
    RecordAotTimerSourceEvent(
        &profile, 0x03001000U, 10U, false, 0U);
    RecordAotTimerSourceEvent(
        &profile, 0x03001000U, 11U, true, 2U);
    RecordAotTimerSourceEvent(
        &profile, 0x03002000U, 12U, true, 1U);
    const bool aggregation =
        profile.enabled &&
        profile.entry_count == 2U &&
        profile.overflow_count == 0U &&
        profile.attributed_tick_count == 3U &&
        profile.entries[0].guest_source == 0x03001000U &&
        profile.entries[0].trap_count == 2U &&
        profile.entries[0].injected_count == 1U &&
        profile.entries[0].deferred_count == 1U &&
        profile.entries[0].attributed_tick_count == 2U &&
        profile.entries[0].first_global_tick == 10U &&
        profile.entries[0].last_global_tick == 11U;

    const auto top =
        BuildAotTimerSourceProfileTopEntries(profile, 2U);
    const bool ranking =
        top.size() == 2U &&
        top[0].guest_source == 0x03001000U &&
        top[1].guest_source == 0x03002000U;

    AotTimerSourceProfile overflow;
    InitializeAotTimerSourceProfile(true, &overflow);
    for (std::uint32_t index = 0;
         index < kAotTimerSourceProfileCapacity + 2U; ++index)
    {
        RecordAotTimerSourceEvent(
            &overflow, 0x03010000U + index, index, true, 1U);
    }
    const bool capacity =
        overflow.entry_count == kAotTimerSourceProfileCapacity &&
        overflow.overflow_count == 2U &&
        overflow.attributed_tick_count ==
            kAotTimerSourceProfileCapacity;

    AotTimerSourceProfile disabled;
    InitializeAotTimerSourceProfile(false, &disabled);
    RecordAotTimerSourceEvent(
        &disabled, 0x03001000U, 1U, true, 1U);
    const bool inert =
        !disabled.enabled && disabled.entry_count == 0U &&
        disabled.attributed_tick_count == 0U;

    const bool all =
        policy && aggregation && ranking && capacity && inert;
    std::cout
        << "aot_timer_source_profile_policy="
        << (policy ? "true" : "false")
        << "\naot_timer_source_profile_aggregation="
        << (aggregation ? "true" : "false")
        << "\naot_timer_source_profile_ranking="
        << (ranking ? "true" : "false")
        << "\naot_timer_source_profile_capacity="
        << (capacity ? "true" : "false")
        << "\naot_timer_source_profile_inert="
        << (inert ? "true" : "false")
        << "\naot_timer_source_profile_all="
        << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
