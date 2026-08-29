#pragma once

#include "repiu/engine/glide_setter_state_model.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace repiu::engine
{

// Task 364: the census answers "how many setter calls exactly repeat the state
// that was last applied successfully" without changing any dispatch result. It is
// observation only. Task 365's cache acts on the answer, and both take their rules
// from `glide_setter_state_model.h` so they cannot disagree.
constexpr std::size_t kGlideSetterCensusCapacity = 256U;

// Distinct-value tracking is a bounded linear set: state setters in the target
// list use a handful of values, and a saturating counter is enough to tell "two
// alternating values" from "a wide distribution".
constexpr std::size_t kGlideSetterCensusDistinctCapacity = 8U;

struct GlideSetterCensusEntry
{
    std::uint32_t call_count = 0;
    // No successfully applied state was on record for this ordinal yet.
    std::uint32_t first_count = 0;
    std::uint32_t same_count = 0;
    std::uint32_t changed_count = 0;
    // The gate declined, so the host state is unknown and the record is void.
    std::uint32_t failure_count = 0;
    // The gate was handled under the retain policy but the backend did not express
    // the argument, so it is not a successfully applied state either.
    std::uint32_t unsupported_count = 0;
    std::uint32_t key_overflow_count = 0;
    std::uint32_t current_repeat_run = 0;
    std::uint32_t max_repeat_run = 0;
    std::uint32_t frame_call_count = 0;
    std::uint32_t frame_change_count = 0;
    std::uint32_t max_frame_call_count = 0;
    std::uint32_t max_frame_change_count = 0;
    std::uint32_t distinct_key_count = 0;
    std::uint32_t distinct_overflow_count = 0;
    bool applied_valid = false;
    GlideSetterStateKey applied_key;
    std::array<GlideSetterStateKey,
               kGlideSetterCensusDistinctCapacity> distinct_keys = {};
};

struct GlideSetterCensusProfile
{
    bool enabled = false;
    std::array<GlideSetterCensusEntry,
               kGlideSetterCensusCapacity> entries = {};
    std::uint32_t ordinal_overflow_count = 0;
    std::uint32_t invalidation_count = 0;
    std::uint32_t frame_count = 0;
    std::uint32_t texture_generation = 0;
};

// Aggregates only. The per-ordinal entries stay in the profile and are read
// directly by the reporting path: an entry is a few hundred bytes and the array is
// 256 wide, so duplicating it into a by-value snapshot would put a hundred
// kilobytes on the stack at every copy.
struct GlideSetterCensusSnapshot
{
    bool enabled = false;
    std::uint32_t active_entry_count = 0;
    std::uint32_t ordinal_overflow_count = 0;
    std::uint32_t invalidation_count = 0;
    std::uint32_t frame_count = 0;
    std::uint32_t texture_generation = 0;
    std::uint32_t call_count = 0;
    std::uint32_t first_count = 0;
    std::uint32_t same_count = 0;
    std::uint32_t changed_count = 0;
    std::uint32_t failure_count = 0;
    std::uint32_t unsupported_count = 0;
    std::uint32_t key_overflow_count = 0;
    std::uint32_t distinct_overflow_count = 0;
};

// The outcome the boundary observed after the unmodified dispatch ran.
enum class GlideSetterCensusOutcome : std::uint8_t
{
    kApplied = 0,
    kUnsupported,
    kFailed,
};

bool ResolveGlideSetterCensusEnabled(std::string_view setting);
bool GlideSetterCensusEnabled();

void RecordGlideSetterCensusCall(
    GlideSetterCensusProfile* profile,
    std::uint16_t ordinal,
    const GlideSetterStateKey& key,
    GlideSetterCensusOutcome outcome);

void RecordGlideSetterCensusKeyOverflow(
    GlideSetterCensusProfile* profile,
    std::uint16_t ordinal);

void RecordGlideSetterCensusInvalidation(
    GlideSetterCensusProfile* profile);

void RecordGlideSetterCensusTextureGeneration(
    GlideSetterCensusProfile* profile);

// `grBufferSwap` is the frame boundary: it rolls the per-frame maxima and resets
// the per-frame counters.
void RecordGlideSetterCensusFrameBoundary(
    GlideSetterCensusProfile* profile);

GlideSetterCensusSnapshot SnapshotGlideSetterCensus(
    const GlideSetterCensusProfile& profile);

}  // namespace repiu::engine
