#include "repiu/engine/glide_lfb_lock_interval_census.h"

#include <cstdlib>

namespace repiu::engine
{
namespace
{

using Interval = GlideLfbLockInterval;

void ResetAccumulator(GlideLfbLockIntervalCensus* const census)
{
    census->swap_count = 0U;
    census->clear_count = 0U;
    census->draw_count = 0U;
    census->region_count = 0U;
    census->other_count = 0U;
}

}  // namespace

bool ResolveGlideLfbLockIntervalCensusSetting(const std::string_view setting)
{
    return setting == "1" || setting == "on" || setting == "true";
}

bool GlideLfbLockIntervalCensusEnabled()
{
    static const bool enabled = [] {
        const char* const value =
            std::getenv("REPIU_GLIDE_LFB_LOCK_INTERVAL_CENSUS");
        return value != nullptr &&
            ResolveGlideLfbLockIntervalCensusSetting(value);
    }();
    return enabled;
}

void NoteGlideLfbLockIntervalGate(GlideLfbLockIntervalCensus* const census,
                                  const repiu::hle::GlideGateId gate_id)
{
    if (census == nullptr || !census->interval_open)
    {
        return;
    }
    using Gate = repiu::hle::GlideGateId;
    switch (gate_id)
    {
        case Gate::kGrBufferSwap:
            ++census->swap_count;
            return;
        case Gate::kGrBufferClear:
            ++census->clear_count;
            return;
        case Gate::kGrDrawLine:
        case Gate::kGrDrawPoint:
        case Gate::kGrDrawTriangle:
        case Gate::kGrDrawPlanarPolygon:
        case Gate::kGrDrawPlanarPolygonVertexList:
        case Gate::kGrDrawPolygon:
        case Gate::kGrDrawPolygonVertexList:
        case Gate::kGrAADrawPoint:
        case Gate::kGrAADrawLine:
        case Gate::kGrAADrawTriangle:
        case Gate::kGrAADrawPolygon:
        case Gate::kGrAADrawPolygonVertexList:
            ++census->draw_count;
            return;
        case Gate::kGrLfbWriteRegion:
        case Gate::kGrLfbReadRegion:
            ++census->region_count;
            return;
        // Locks and unlocks bound the interval rather than fill it, and the
        // state setters between them are exactly what this census is trying to
        // look past.
        case Gate::kGrLfbLock:
        case Gate::kGrLfbUnlock:
            return;
        default:
            ++census->other_count;
            return;
    }
}

void OpenGlideLfbLockInterval(GlideLfbLockIntervalCensus* const census)
{
    if (census == nullptr)
    {
        return;
    }
    ResetAccumulator(census);
    census->interval_open = true;
}

GlideLfbLockInterval ClassifyGlideLfbLockInterval(
    GlideLfbLockIntervalCensus* const census)
{
    if (census == nullptr)
    {
        return Interval::kFirstLock;
    }
    // Ranked rather than combined: a window holding both a clear and a draw is
    // reported as cleared, because the clear is the stronger statement about
    // whether previous pixels survived.
    Interval interval = Interval::kFirstLock;
    if (census->interval_open)
    {
        if (census->clear_count != 0U)
        {
            interval = Interval::kCleared;
        }
        else if (census->draw_count != 0U)
        {
            interval = Interval::kDrawn;
        }
        else if (census->region_count != 0U)
        {
            interval = Interval::kRegion;
        }
        else
        {
            interval = Interval::kClean;
        }

        census->swap_total += census->swap_count;
        census->clear_total += census->clear_count;
        census->draw_total += census->draw_count;
        if (census->swap_count < census->swap_minimum)
        {
            census->swap_minimum = census->swap_count;
        }
        if (census->swap_count > census->swap_maximum)
        {
            census->swap_maximum = census->swap_count;
        }
        if (census->swap_count == 0U)
        {
            ++census->zero_swap_interval_count;
        }
        else if (census->swap_count == 1U)
        {
            ++census->single_swap_interval_count;
        }
        else
        {
            ++census->multi_swap_interval_count;
        }
    }
    ++census->interval_counts[static_cast<std::size_t>(interval)];
    ResetAccumulator(census);
    // The interval stays closed until the next unlock opens one, so the gates
    // of the lock itself are not counted into the next window.
    census->interval_open = false;
    return interval;
}

GlideLfbLockIntervalCensusSnapshot SnapshotGlideLfbLockIntervalCensus(
    const GlideLfbLockIntervalCensus& census)
{
    GlideLfbLockIntervalCensusSnapshot snapshot;
    snapshot.enabled = GlideLfbLockIntervalCensusEnabled();
    for (std::size_t index = 0U; index < kGlideLfbLockIntervalKindCount;
         ++index)
    {
        snapshot.interval_counts[index] = census.interval_counts[index];
    }
    snapshot.swap_total = census.swap_total;
    snapshot.swap_minimum =
        census.swap_minimum == 0xFFFFFFFFU ? 0U : census.swap_minimum;
    snapshot.swap_maximum = census.swap_maximum;
    snapshot.zero_swap_interval_count = census.zero_swap_interval_count;
    snapshot.single_swap_interval_count = census.single_swap_interval_count;
    snapshot.multi_swap_interval_count = census.multi_swap_interval_count;
    snapshot.clear_total = census.clear_total;
    snapshot.draw_total = census.draw_total;
    return snapshot;
}

const char* GlideLfbLockIntervalName(const GlideLfbLockInterval interval)
{
    switch (interval)
    {
        case Interval::kClean:
            return "clean";
        case Interval::kCleared:
            return "cleared";
        case Interval::kDrawn:
            return "drawn";
        case Interval::kRegion:
            return "region";
        case Interval::kFirstLock:
            return "first-lock";
        case Interval::kCount:
            break;
    }
    return "unknown";
}

}  // namespace repiu::engine
