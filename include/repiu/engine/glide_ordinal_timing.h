#pragma once

#include "repiu/engine/glide_gate_timing.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace repiu::engine
{

constexpr std::size_t kGlideOrdinalTimingCapacity = 256U;

struct GlideOrdinalTimingEntry
{
    std::uint32_t count = 0;
    std::uint64_t gate_cycles = 0;
    std::uint64_t max_gate_cycles = 0;
    std::uint32_t rendezvous_count = 0;
    std::uint64_t queue_cycles = 0;
    std::uint64_t wake_cycles = 0;
    std::uint64_t work_cycles = 0;
    std::uint64_t complete_cycles = 0;
    std::uint64_t residual_cycles = 0;
    std::uint64_t backend_total_cycles = 0;
    std::uint32_t direct_count = 0;
    std::uint64_t direct_work_cycles = 0;
};

struct GlideOrdinalTimingProfile
{
    bool enabled = false;
    std::array<GlideOrdinalTimingEntry,
               kGlideOrdinalTimingCapacity> entries = {};
    std::uint32_t overflow_count = 0;
    std::uint32_t clamped_sample_count = 0;
};

struct GlideOrdinalTimingSnapshot
{
    bool enabled = false;
    std::array<GlideOrdinalTimingEntry,
               kGlideOrdinalTimingCapacity> entries = {};
    std::uint32_t active_entry_count = 0;
    std::uint32_t completed_gate_count = 0;
    std::uint32_t overflow_count = 0;
    std::uint32_t clamped_sample_count = 0;
    std::uint64_t gate_cycles = 0;
    std::uint32_t rendezvous_count = 0;
    std::uint64_t queue_cycles = 0;
    std::uint64_t wake_cycles = 0;
    std::uint64_t work_cycles = 0;
    std::uint64_t complete_cycles = 0;
    std::uint64_t residual_cycles = 0;
    std::uint64_t backend_total_cycles = 0;
    std::uint32_t direct_count = 0;
    std::uint64_t direct_work_cycles = 0;
};

bool ResolveGlideOrdinalTimingProfileEnabled(std::string_view setting);
bool GlideOrdinalTimingProfileEnabled();

void RecordGlideOrdinalGateTime(
    GlideOrdinalTimingProfile* profile,
    std::uint16_t ordinal,
    std::uint64_t gate_cycles);

// Records one completed synchronous backend rendezvous using the timestamps
// already captured by Task 333. This avoids copying the whole cumulative
// backend profile twice per gate.
void RecordGlideOrdinalRendezvous(
    GlideOrdinalTimingProfile* profile,
    std::uint16_t ordinal,
    std::uint64_t enter_cycles,
    std::uint64_t publish_cycles,
    std::uint64_t host_start_cycles,
    std::uint64_t host_finish_cycles,
    std::uint64_t resume_cycles);

void RecordGlideOrdinalDirectWork(
    GlideOrdinalTimingProfile* profile,
    std::uint16_t ordinal,
    std::uint64_t cycles);

GlideOrdinalTimingSnapshot SnapshotGlideOrdinalTiming(
    const GlideOrdinalTimingProfile& profile);

}  // namespace repiu::engine
