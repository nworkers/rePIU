#include "glide_lfb_lock_interval_probe.h"

#include "repiu/engine/glide_lfb_lock_interval_census.h"

#include <iostream>
#include <string_view>

namespace repiu::tools
{
namespace
{

using engine::GlideLfbLockInterval;
using engine::GlideLfbLockIntervalCensus;
using Gate = repiu::hle::GlideGateId;

std::uint32_t KindCount(const GlideLfbLockIntervalCensus& census,
                        const GlideLfbLockInterval interval)
{
    return census.interval_counts[static_cast<std::size_t>(interval)];
}

void Note(GlideLfbLockIntervalCensus* const census, const Gate gate)
{
    engine::NoteGlideLfbLockIntervalGate(census, gate);
}

}  // namespace

bool RunGlideLfbLockIntervalProbe()
{
    const bool policy =
        !engine::ResolveGlideLfbLockIntervalCensusSetting("") &&
        !engine::ResolveGlideLfbLockIntervalCensusSetting("0") &&
        !engine::ResolveGlideLfbLockIntervalCensusSetting("yes") &&
        engine::ResolveGlideLfbLockIntervalCensusSetting("1") &&
        engine::ResolveGlideLfbLockIntervalCensusSetting("on") &&
        engine::ResolveGlideLfbLockIntervalCensusSetting("true");

    // A run's first lock has no preceding unlock, so nothing was watched and
    // the interval must not be reported as clean.
    GlideLfbLockIntervalCensus first;
    Note(&first, Gate::kGrBufferSwap);
    Note(&first, Gate::kGrDrawTriangle);
    const bool first_is_first_lock =
        engine::ClassifyGlideLfbLockInterval(&first) ==
            GlideLfbLockInterval::kFirstLock &&
        KindCount(first, GlideLfbLockInterval::kFirstLock) == 1U &&
        KindCount(first, GlideLfbLockInterval::kClean) == 0U &&
        first.swap_total == 0U;

    // Swaps alone are the window a shadow pair could survive.
    GlideLfbLockIntervalCensus clean;
    engine::OpenGlideLfbLockInterval(&clean);
    Note(&clean, Gate::kGrBufferSwap);
    Note(&clean, Gate::kGrTexSource);
    Note(&clean, Gate::kGrColorCombine);
    const bool clean_interval =
        engine::ClassifyGlideLfbLockInterval(&clean) ==
            GlideLfbLockInterval::kClean &&
        KindCount(clean, GlideLfbLockInterval::kClean) == 1U &&
        clean.swap_total == 1U && clean.single_swap_interval_count == 1U &&
        clean.clear_total == 0U && clean.draw_total == 0U;

    // Ranking: a window holding both a clear and a draw reports as cleared,
    // because the clear is the stronger statement about previous pixels.
    GlideLfbLockIntervalCensus ranked;
    engine::OpenGlideLfbLockInterval(&ranked);
    Note(&ranked, Gate::kGrDrawTriangle);
    Note(&ranked, Gate::kGrBufferClear);
    Note(&ranked, Gate::kGrLfbWriteRegion);
    Note(&ranked, Gate::kGrBufferSwap);
    Note(&ranked, Gate::kGrBufferSwap);
    const bool ranking =
        engine::ClassifyGlideLfbLockInterval(&ranked) ==
            GlideLfbLockInterval::kCleared &&
        ranked.swap_total == 2U && ranked.multi_swap_interval_count == 1U &&
        ranked.clear_total == 1U && ranked.draw_total == 1U;

    GlideLfbLockIntervalCensus drawn;
    engine::OpenGlideLfbLockInterval(&drawn);
    Note(&drawn, Gate::kGrAADrawTriangle);
    const bool drawn_interval =
        engine::ClassifyGlideLfbLockInterval(&drawn) ==
            GlideLfbLockInterval::kDrawn &&
        drawn.zero_swap_interval_count == 1U;

    GlideLfbLockIntervalCensus region;
    engine::OpenGlideLfbLockInterval(&region);
    Note(&region, Gate::kGrLfbReadRegion);
    const bool region_interval =
        engine::ClassifyGlideLfbLockInterval(&region) ==
            GlideLfbLockInterval::kRegion;

    // The interval closes at the lock, so gates between that lock and the next
    // unlock belong to no window.
    GlideLfbLockIntervalCensus closed;
    engine::OpenGlideLfbLockInterval(&closed);
    Note(&closed, Gate::kGrBufferSwap);
    engine::ClassifyGlideLfbLockInterval(&closed);
    Note(&closed, Gate::kGrBufferClear);
    Note(&closed, Gate::kGrDrawTriangle);
    const bool closes_after_lock =
        engine::ClassifyGlideLfbLockInterval(&closed) ==
            GlideLfbLockInterval::kFirstLock &&
        closed.clear_total == 0U && closed.draw_total == 0U;

    // Several intervals in a row, as a run produces them.
    GlideLfbLockIntervalCensus series;
    for (int index = 0; index < 3; ++index)
    {
        engine::OpenGlideLfbLockInterval(&series);
        Note(&series, Gate::kGrBufferSwap);
        if (index == 1)
        {
            Note(&series, Gate::kGrBufferClear);
            Note(&series, Gate::kGrBufferSwap);
        }
        engine::ClassifyGlideLfbLockInterval(&series);
    }
    const auto series_snapshot =
        engine::SnapshotGlideLfbLockIntervalCensus(series);
    const bool series_totals =
        series_snapshot.interval_counts[static_cast<std::size_t>(
            GlideLfbLockInterval::kClean)] == 2U &&
        series_snapshot.interval_counts[static_cast<std::size_t>(
            GlideLfbLockInterval::kCleared)] == 1U &&
        series_snapshot.swap_total == 4U &&
        series_snapshot.swap_minimum == 1U &&
        series_snapshot.swap_maximum == 2U &&
        series_snapshot.single_swap_interval_count == 2U &&
        series_snapshot.multi_swap_interval_count == 1U;

    // An untouched census reports a zero minimum rather than the sentinel.
    const auto empty_snapshot = engine::SnapshotGlideLfbLockIntervalCensus(
        GlideLfbLockIntervalCensus{});
    bool names = empty_snapshot.swap_minimum == 0U;
    for (std::size_t index = 0U;
         index < engine::kGlideLfbLockIntervalKindCount; ++index)
    {
        const char* const name = engine::GlideLfbLockIntervalName(
            static_cast<GlideLfbLockInterval>(index));
        names = names && name != nullptr &&
            std::string_view(name) != "unknown";
    }
    engine::NoteGlideLfbLockIntervalGate(nullptr, Gate::kGrBufferSwap);
    engine::OpenGlideLfbLockInterval(nullptr);
    names = names && engine::ClassifyGlideLfbLockInterval(nullptr) ==
        GlideLfbLockInterval::kFirstLock;

    const bool all = policy && first_is_first_lock && clean_interval &&
        ranking && drawn_interval && region_interval && closes_after_lock &&
        series_totals && names;
    std::cout << "glide_lfb_lock_interval_policy="
              << (policy ? "true" : "false")
              << "\nglide_lfb_lock_interval_first_lock="
              << (first_is_first_lock ? "true" : "false")
              << "\nglide_lfb_lock_interval_clean="
              << (clean_interval ? "true" : "false")
              << "\nglide_lfb_lock_interval_ranking="
              << (ranking ? "true" : "false")
              << "\nglide_lfb_lock_interval_drawn="
              << (drawn_interval ? "true" : "false")
              << "\nglide_lfb_lock_interval_region="
              << (region_interval ? "true" : "false")
              << "\nglide_lfb_lock_interval_closes_after_lock="
              << (closes_after_lock ? "true" : "false")
              << "\nglide_lfb_lock_interval_series="
              << (series_totals ? "true" : "false")
              << "\nglide_lfb_lock_interval_names="
              << (names ? "true" : "false")
              << "\nglide_lfb_lock_interval_all="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
