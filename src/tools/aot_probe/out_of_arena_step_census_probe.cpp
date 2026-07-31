#include "out_of_arena_step_census_probe.h"

#include "repiu/platform/win32/out_of_arena_step_census.h"

#include <iostream>

namespace repiu::tools
{

bool RunOutOfArenaStepCensusProbe()
{
    using platform::win32::kOutOfArenaStepAddressCapacity;
    using platform::win32::OutOfArenaStepLocation;
    using platform::win32::RecordOutOfArenaStep;
    using platform::win32::SnapshotOutOfArenaStepCensus;
    using platform::win32::Win32OutOfArenaStepCensus;

    Win32OutOfArenaStepCensus census;
    RecordOutOfArenaStep(&census, 0x0C000010U,
                         OutOfArenaStepLocation::kAotCodeCache, true, true);
    RecordOutOfArenaStep(&census, 0x0C000010U,
                         OutOfArenaStepLocation::kAotCodeCache, true, false);
    RecordOutOfArenaStep(&census, 0x7FFF0000U, OutOfArenaStepLocation::kOther,
                         false, false);

    const auto snapshot = SnapshotOutOfArenaStepCensus(census);
    const bool classified = snapshot.total_count == 3U &&
        snapshot.location_counts[0] == 2U &&
        snapshot.location_counts[1] == 1U &&
        snapshot.trace_enabled_count == 2U &&
        snapshot.reentry_pending_count == 1U;

    // First and last must bracket the run: the first address is what armed the
    // very first discarded step, and the last is where the run ended.
    const bool endpoints = snapshot.first_eip == 0x0C000010U &&
        snapshot.last_eip == 0x7FFF0000U;

    // A repeated address aggregates rather than consuming a second slot, which
    // is what makes "one site dominates" readable off the table.
    const bool aggregated = snapshot.addresses[0].eip == 0x0C000010U &&
        snapshot.addresses[0].count == 2U &&
        snapshot.addresses[1].eip == 0x7FFF0000U &&
        snapshot.addresses[1].count == 1U;

    // Overflow must be counted, not silently dropped, or a full table would read
    // as a dominant site when it is merely a truncated one.
    Win32OutOfArenaStepCensus full;
    for (std::uint32_t index = 0;
         index < kOutOfArenaStepAddressCapacity + 5U; ++index)
    {
        RecordOutOfArenaStep(&full, 0x1000U + index,
                             OutOfArenaStepLocation::kOther, false, false);
    }
    const auto full_snapshot = SnapshotOutOfArenaStepCensus(full);
    const bool overflow_counted =
        full_snapshot.address_overflow_count == 5U &&
        full_snapshot.total_count == kOutOfArenaStepAddressCapacity + 5U;

    Win32OutOfArenaStepCensus untouched;
    RecordOutOfArenaStep(nullptr, 0x1U, OutOfArenaStepLocation::kOther, true,
                         true);
    const bool inert =
        SnapshotOutOfArenaStepCensus(untouched).total_count == 0U;

    const bool all = classified && endpoints && aggregated &&
        overflow_counted && inert;
    std::cout << "out_of_arena_step_classified="
              << (classified ? "true" : "false")
              << "\nout_of_arena_step_endpoints="
              << (endpoints ? "true" : "false")
              << "\nout_of_arena_step_aggregated="
              << (aggregated ? "true" : "false")
              << "\nout_of_arena_step_overflow_counted="
              << (overflow_counted ? "true" : "false")
              << "\nout_of_arena_step_inert=" << (inert ? "true" : "false")
              << "\nout_of_arena_step_all=" << (all ? "true" : "false")
              << std::endl;
    return all;
}

}  // namespace repiu::tools
