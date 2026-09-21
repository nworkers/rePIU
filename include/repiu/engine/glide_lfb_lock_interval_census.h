#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "repiu/hle/glide_hle.h"

namespace repiu::engine
{

// Task 729. What happens between one write `grLfbUnlock` and the next write
// `grLfbLock`.
//
// Task 728 showed that the staging shadow is always broken by `grBufferSwap`,
// but it could not show what follows that swap, because it counted only the
// first gate to break a live shadow. The follow-on design -- one shadow per
// buffer, exchanged at the swap -- only works if nothing touches the back
// buffer in that window. This census answers that before anything is built on
// the assumption.
enum class GlideLfbLockInterval : std::uint32_t
{
    // Swaps only. The window a per-buffer shadow pair could survive.
    kClean = 0,
    kCleared,
    kDrawn,
    kRegion,
    // The first lock of a run, which has no preceding unlock.
    kFirstLock,
    kCount
};

inline constexpr std::size_t kGlideLfbLockIntervalKindCount =
    static_cast<std::size_t>(GlideLfbLockInterval::kCount);

struct GlideLfbLockIntervalCensus
{
    // The accumulator for the interval currently open. `interval_open` is false
    // before the first unlock, which is what separates "nothing happened" from
    // "nothing was watched".
    bool interval_open = false;
    std::uint32_t swap_count = 0;
    std::uint32_t clear_count = 0;
    std::uint32_t draw_count = 0;
    std::uint32_t region_count = 0;
    std::uint32_t other_count = 0;

    std::uint32_t interval_counts[kGlideLfbLockIntervalKindCount] = {};

    // Swap distribution across classified intervals. Parity decides which
    // buffer a lock sees under the page-flip model, so one swap and two swaps
    // are different cases rather than degrees of the same one.
    std::uint64_t swap_total = 0;
    std::uint32_t swap_minimum = 0xFFFFFFFFU;
    std::uint32_t swap_maximum = 0;
    std::uint32_t zero_swap_interval_count = 0;
    std::uint32_t single_swap_interval_count = 0;
    std::uint32_t multi_swap_interval_count = 0;

    // Totals over classified intervals, for reading the averages the aggregate
    // ordinal profile can only guess at.
    std::uint64_t clear_total = 0;
    std::uint64_t draw_total = 0;
};

struct GlideLfbLockIntervalCensusSnapshot
{
    bool enabled = false;
    std::uint32_t interval_counts[kGlideLfbLockIntervalKindCount] = {};
    std::uint64_t swap_total = 0;
    std::uint32_t swap_minimum = 0;
    std::uint32_t swap_maximum = 0;
    std::uint32_t zero_swap_interval_count = 0;
    std::uint32_t single_swap_interval_count = 0;
    std::uint32_t multi_swap_interval_count = 0;
    std::uint64_t clear_total = 0;
    std::uint64_t draw_total = 0;
};

bool ResolveGlideLfbLockIntervalCensusSetting(std::string_view setting);
bool GlideLfbLockIntervalCensusEnabled();

// Counts one dispatched gate into the open interval. Gates arriving before the
// first unlock are ignored rather than attributed to an interval that was never
// opened.
void NoteGlideLfbLockIntervalGate(GlideLfbLockIntervalCensus* census,
                                  repiu::hle::GlideGateId gate_id);

// Opens a new interval at a write unlock, discarding whatever the previous one
// accumulated after it was classified.
void OpenGlideLfbLockInterval(GlideLfbLockIntervalCensus* census);

// Classifies the open interval at a write lock and resets the accumulator.
// Returns what the interval was, so a caller can act on it.
GlideLfbLockInterval ClassifyGlideLfbLockInterval(
    GlideLfbLockIntervalCensus* census);

GlideLfbLockIntervalCensusSnapshot SnapshotGlideLfbLockIntervalCensus(
    const GlideLfbLockIntervalCensus& census);

const char* GlideLfbLockIntervalName(GlideLfbLockInterval interval);

}  // namespace repiu::engine
