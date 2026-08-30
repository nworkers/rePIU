#include "repiu/engine/aot_ff_target_timing.h"

#include <cstdlib>
#include <cstring>

namespace repiu::engine
{
namespace
{

bool ReadAotFfTargetTimingSetting()
{
    const char* value = std::getenv("REPIU_AOT_FF_TARGET_TIMING");
    return value != nullptr &&
        (std::strcmp(value, "1") == 0 ||
         std::strcmp(value, "on") == 0 ||
         std::strcmp(value, "true") == 0);
}

bool SameKey(const AotFfTargetTimingEntry& entry,
             std::uint32_t source_guest_eip,
             std::uint32_t target_guest_eip,
             std::uint32_t cache_target,
             bool index_value_valid,
             std::uint8_t index_register,
             std::uint32_t index_value)
{
    return entry.valid &&
        entry.source_guest_eip == source_guest_eip &&
        entry.target_guest_eip == target_guest_eip &&
        entry.cache_target == cache_target &&
        entry.index_value_valid == index_value_valid &&
        (!index_value_valid ||
         (entry.index_register == index_register &&
          entry.index_value == index_value));
}

}  // namespace

bool AotFfTargetTimingEnabled()
{
    static const bool enabled = ReadAotFfTargetTimingSetting();
    return enabled;
}

void ClearAotFfTargetTimingCandidate(AotFfTargetTimingProfile* profile)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->candidate_valid = false;
    profile->candidate_source_guest_eip = 0;
    profile->candidate_target_guest_eip = 0;
    profile->candidate_index_value_valid = false;
    profile->candidate_index_register = 0;
    profile->candidate_index_value = 0;
}

void SetAotFfTargetTimingCandidate(
    AotFfTargetTimingProfile* profile,
    std::uint32_t source_guest_eip,
    std::uint32_t target_guest_eip,
    bool index_value_valid,
    std::uint8_t index_register,
    std::uint32_t index_value)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->candidate_valid = true;
    profile->candidate_source_guest_eip = source_guest_eip;
    profile->candidate_target_guest_eip = target_guest_eip;
    profile->candidate_index_value_valid = index_value_valid;
    profile->candidate_index_register = index_value_valid ? index_register : 0;
    profile->candidate_index_value = index_value_valid ? index_value : 0;
}

bool BeginAotFfTargetTimingIfMatched(
    AotFfTargetTimingProfile* profile,
    std::uint32_t target_guest_eip,
    std::uint32_t cache_target,
    std::uint64_t start_cycles)
{
    if (profile == nullptr || !profile->candidate_valid)
    {
        return false;
    }
    if (profile->candidate_target_guest_eip != target_guest_eip)
    {
        ++profile->candidate_mismatch_count;
        ClearAotFfTargetTimingCandidate(profile);
        return false;
    }
    if (profile->interval_active)
    {
        ++profile->discarded_interval_count;
        ClearAotFfTargetTimingCandidate(profile);
        return false;
    }

    profile->interval_active = true;
    profile->interval_start_cycles = start_cycles;
    profile->active_source_guest_eip = profile->candidate_source_guest_eip;
    profile->active_target_guest_eip = target_guest_eip;
    profile->active_cache_target = cache_target;
    profile->active_index_value_valid =
        profile->candidate_index_value_valid;
    profile->active_index_register = profile->candidate_index_register;
    profile->active_index_value = profile->candidate_index_value;
    ++profile->interval_started_count;
    ClearAotFfTargetTimingCandidate(profile);
    return true;
}

bool CompleteAotFfTargetTiming(
    AotFfTargetTimingProfile* profile,
    std::uint64_t end_cycles)
{
    if (profile == nullptr || !profile->interval_active)
    {
        return false;
    }
    const std::uint64_t start_cycles = profile->interval_start_cycles;
    profile->interval_active = false;
    if (end_cycles < start_cycles)
    {
        ++profile->discarded_interval_count;
        return false;
    }

    AotFfTargetTimingEntry* entry = nullptr;
    for (std::size_t slot = 0U;
         slot < kAotFfTargetTimingEntryCapacity; ++slot)
    {
        if (SameKey(profile->entries[slot],
                    profile->active_source_guest_eip,
                    profile->active_target_guest_eip,
                    profile->active_cache_target,
                    profile->active_index_value_valid,
                    profile->active_index_register,
                    profile->active_index_value))
        {
            entry = &profile->entries[slot];
            break;
        }
    }
    if (entry == nullptr)
    {
        for (std::size_t slot = 0U;
             slot < kAotFfTargetTimingEntryCapacity; ++slot)
        {
            if (!profile->entries[slot].valid)
            {
                entry = &profile->entries[slot];
                entry->valid = true;
                entry->source_guest_eip = profile->active_source_guest_eip;
                entry->target_guest_eip = profile->active_target_guest_eip;
                entry->cache_target = profile->active_cache_target;
                entry->index_value_valid = profile->active_index_value_valid;
                entry->index_register = profile->active_index_register;
                entry->index_value = profile->active_index_value;
                break;
            }
        }
    }
    ++profile->interval_completed_count;
    if (entry == nullptr)
    {
        ++profile->entry_overflow_count;
        return true;
    }

    const std::uint64_t cycles = end_cycles - start_cycles;
    ++entry->interval_count;
    entry->total_cycles += cycles;
    if (entry->interval_count == 1U || cycles < entry->min_cycles)
    {
        entry->min_cycles = cycles;
    }
    if (cycles > entry->max_cycles)
    {
        entry->max_cycles = cycles;
    }
    return true;
}

}  // namespace repiu::engine
