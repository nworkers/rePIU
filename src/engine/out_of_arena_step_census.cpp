#include "repiu/engine/out_of_arena_step_census.h"

namespace repiu::engine
{

void RecordOutOfArenaStep(OutOfArenaStepCensus* census,
                          std::uint32_t eip,
                          OutOfArenaStepLocation location,
                          bool trace_enabled,
                          bool reentry_pending)
{
    if (census == nullptr)
    {
        return;
    }
    if (census->total_count == 0U)
    {
        census->first_eip = eip;
    }
    ++census->total_count;
    census->last_eip = eip;

    const std::uint32_t index = static_cast<std::uint32_t>(location);
    if (index < kOutOfArenaStepLocationCount)
    {
        ++census->location_counts[index];
    }
    if (trace_enabled)
    {
        ++census->trace_enabled_count;
    }
    if (reentry_pending)
    {
        ++census->reentry_pending_count;
    }

    // Linear scan over a dozen slots. The table is small enough that this stays
    // cheaper than a hash on a path that must not allocate.
    for (auto& slot : census->addresses)
    {
        if (slot.count != 0U && slot.eip == eip)
        {
            ++slot.count;
            return;
        }
    }
    for (auto& slot : census->addresses)
    {
        if (slot.count == 0U)
        {
            slot.eip = eip;
            slot.count = 1U;
            return;
        }
    }
    // Full. Counting the overflow keeps "one site dominates" from being read off
    // a table that silently dropped the rest.
    ++census->address_overflow_count;
}

void RecordSingleStepTraceDisposition(OutOfArenaStepCensus* census,
                                      bool trace_enabled)
{
    if (census == nullptr)
    {
        return;
    }
    if (trace_enabled)
    {
        ++census->trace_enabled_handled_count;
        return;
    }
    ++census->trace_disabled_fallthrough_count;
}

OutOfArenaStepCensusSnapshot SnapshotOutOfArenaStepCensus(
    const OutOfArenaStepCensus& census)
{
    OutOfArenaStepCensusSnapshot snapshot;
    snapshot.total_count = census.total_count;
    snapshot.location_counts = census.location_counts;
    snapshot.trace_enabled_count = census.trace_enabled_count;
    snapshot.reentry_pending_count = census.reentry_pending_count;
    snapshot.first_eip = census.first_eip;
    snapshot.last_eip = census.last_eip;
    snapshot.address_overflow_count = census.address_overflow_count;
    snapshot.trace_disabled_fallthrough_count =
        census.trace_disabled_fallthrough_count;
    snapshot.trace_enabled_handled_count = census.trace_enabled_handled_count;
    snapshot.addresses = census.addresses;
    return snapshot;
}

}  // namespace repiu::engine
