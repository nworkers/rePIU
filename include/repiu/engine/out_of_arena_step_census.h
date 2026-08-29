#pragma once

#include <array>
#include <cstdint>

namespace repiu::engine
{

// Task 376: single-step exceptions taken outside the guest arena were discarded
// without a name -- the trap flag cleared and execution continued. Both existing
// instruments (`single_step_trace_count` and the HLE re-entry funnel) gate on
// `IsGuestInstructionPointer`, so this population never reached either, and it is
// 70.1% of all single steps: 236 per frame and 6.5% of wall in kernel round trips
// alone.
//
// Nothing here changes behaviour. It names what was already happening.
// Two buckets, not three. Whether the address sits in the host image could only
// be decided here by a heuristic or a module query, and neither belongs on the
// exception path; the top-address table characterises "other" without guessing.
enum class OutOfArenaStepLocation : std::uint32_t
{
    kAotCodeCache = 0,
    kOther,
    kCount,
};

constexpr std::uint32_t kOutOfArenaStepLocationCount =
    static_cast<std::uint32_t>(OutOfArenaStepLocation::kCount);

// Small on purpose: the question is whether one site dominates, and a handful of
// addresses answers that. An overflow counter keeps the answer honest when it
// does not.
constexpr std::uint32_t kOutOfArenaStepAddressCapacity = 12U;

struct OutOfArenaStepAddress
{
    std::uint32_t eip = 0;
    std::uint32_t count = 0;
};

struct OutOfArenaStepCensus
{
    std::uint32_t total_count = 0;
    std::array<std::uint32_t, kOutOfArenaStepLocationCount> location_counts = {};
    // The pair that separates the hypotheses. If these arrive with trace mode
    // off, the arming subject is not the one-step bridge.
    std::uint32_t trace_enabled_count = 0;
    std::uint32_t reentry_pending_count = 0;
    std::uint32_t first_eip = 0;
    std::uint32_t last_eip = 0;
    std::uint32_t address_overflow_count = 0;
    // Task 376 stage two: the discard site measured zero, so the population that
    // never reaches `single_step_trace_count` is not being thrown away -- it is
    // taking another branch. These name which one.
    std::uint32_t trace_disabled_fallthrough_count = 0;
    std::uint32_t trace_enabled_handled_count = 0;
    std::array<OutOfArenaStepAddress, kOutOfArenaStepAddressCapacity>
        addresses = {};
};

struct OutOfArenaStepCensusSnapshot
{
    std::uint32_t total_count = 0;
    std::array<std::uint32_t, kOutOfArenaStepLocationCount> location_counts = {};
    std::uint32_t trace_enabled_count = 0;
    std::uint32_t reentry_pending_count = 0;
    std::uint32_t first_eip = 0;
    std::uint32_t last_eip = 0;
    std::uint32_t address_overflow_count = 0;
    std::uint32_t trace_disabled_fallthrough_count = 0;
    std::uint32_t trace_enabled_handled_count = 0;
    std::array<OutOfArenaStepAddress, kOutOfArenaStepAddressCapacity>
        addresses = {};
};

// Counter increments only -- this runs on the exception path, so it takes no
// clock reading and allocates nothing.
void RecordOutOfArenaStep(OutOfArenaStepCensus* census,
                          std::uint32_t eip,
                          OutOfArenaStepLocation location,
                          bool trace_enabled,
                          bool reentry_pending);

// A single step arriving with trace mode off does not reach
// RecordSingleStepDiagnostics, so it never appears in `single_step_trace_count`.
// It is handled further down the chain rather than discarded.
void RecordSingleStepTraceDisposition(OutOfArenaStepCensus* census,
                                      bool trace_enabled);

OutOfArenaStepCensusSnapshot SnapshotOutOfArenaStepCensus(
    const OutOfArenaStepCensus& census);

}  // namespace repiu::engine
