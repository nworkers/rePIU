#ifndef REPIU_ENGINE_AOT_FF_TARGET_TIMING_H_
#define REPIU_ENGINE_AOT_FF_TARGET_TIMING_H_

#include <cstddef>
#include <cstdint>

namespace repiu::engine
{

constexpr std::size_t kAotFfTargetTimingEntryCapacity = 16U;

struct AotFfTargetTimingEntry
{
    bool valid = false;
    std::uint32_t source_guest_eip = 0;
    std::uint32_t target_guest_eip = 0;
    std::uint32_t cache_target = 0;
    bool index_value_valid = false;
    std::uint8_t index_register = 0;
    std::uint32_t index_value = 0;
    std::uint32_t interval_count = 0;
    std::uint64_t total_cycles = 0;
    std::uint64_t min_cycles = 0;
    std::uint64_t max_cycles = 0;
};

struct AotFfTargetTimingProfile
{
    bool candidate_valid = false;
    std::uint32_t candidate_source_guest_eip = 0;
    std::uint32_t candidate_target_guest_eip = 0;
    bool candidate_index_value_valid = false;
    std::uint8_t candidate_index_register = 0;
    std::uint32_t candidate_index_value = 0;
    bool interval_active = false;
    std::uint64_t interval_start_cycles = 0;
    std::uint32_t active_source_guest_eip = 0;
    std::uint32_t active_target_guest_eip = 0;
    std::uint32_t active_cache_target = 0;
    bool active_index_value_valid = false;
    std::uint8_t active_index_register = 0;
    std::uint32_t active_index_value = 0;
    std::uint32_t interval_started_count = 0;
    std::uint32_t interval_completed_count = 0;
    std::uint32_t candidate_mismatch_count = 0;
    std::uint32_t discarded_interval_count = 0;
    std::uint32_t entry_overflow_count = 0;
    AotFfTargetTimingEntry entries[kAotFfTargetTimingEntryCapacity] = {};
};

bool AotFfTargetTimingEnabled();

void ClearAotFfTargetTimingCandidate(AotFfTargetTimingProfile* profile);

void SetAotFfTargetTimingCandidate(
    AotFfTargetTimingProfile* profile,
    std::uint32_t source_guest_eip,
    std::uint32_t target_guest_eip,
    bool index_value_valid,
    std::uint8_t index_register,
    std::uint32_t index_value);

bool BeginAotFfTargetTimingIfMatched(
    AotFfTargetTimingProfile* profile,
    std::uint32_t target_guest_eip,
    std::uint32_t cache_target,
    std::uint64_t start_cycles);

bool CompleteAotFfTargetTiming(
    AotFfTargetTimingProfile* profile,
    std::uint64_t end_cycles);

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_AOT_FF_TARGET_TIMING_H_
