#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace repiu::runtime
{

struct AotDbtReturnDispatchSite;

}  // namespace repiu::runtime

namespace repiu::engine
{

struct Win32AotReturnPatchPolicy;

// Task 482: attribution inside the return handler bucket.
//
// After Task 481 the return handler still holds 16.5-17.5% of `guest-run`, but
// the bucket mixes site validation, the guest stack read and call/return
// bookkeeping, target classification, the megamorphic patch policy, and guest
// continuation. This profile splits that one bucket into five mutually
// exclusive stages plus the residual so the next implementation is chosen from
// measurement rather than from the shape of the remaining bucket.
//
// This is attribution only. It changes no guest register, no guest memory, no
// cache layout, no resolved target, and no patch decision, and it adds no map,
// allocation, lock, or formatted logging to the hot path.
//
// See docs/design/20260814-482-post-return-bottleneck-attribution.md.
enum class AotReturnStage : std::uint32_t
{
    // Entry accounting, dispatch-site lookup, and the RET opcode check.
    kEntryValidation = 0,
    // Guest stack target read plus call/return bookkeeping: expected-frame
    // matching, the return trace ring, and shared live telemetry.
    kTargetRead,
    // Target classification and ResolveAotTransferTarget.
    kTargetResolution,
    // Inline-cache miss test, Task 481 megamorphic policy, and optional patch.
    kPatchPolicy,
    // Guest continuation (ESP/EIP/EFLAGS), counters, and frame writeback.
    kContinuation,
    kCount,
};

constexpr std::uint32_t kAotReturnStageCount =
    static_cast<std::uint32_t>(AotReturnStage::kCount);

// Reported next to the stage totals so the top of the distribution is readable
// without a formatted dump of every site.
constexpr std::size_t kAotReturnStageSiteReportCapacity = 16U;

struct Win32AotReturnStageProfile
{
    bool enabled = false;
    std::array<std::uint64_t, kAotReturnStageCount> cycles = {};
    std::array<std::uint32_t, kAotReturnStageCount> counts = {};
    std::array<std::uint64_t, kAotReturnStageCount> max_cycles = {};
    // Running sum of the array above. The outer scope reads it on entry and on
    // exit so one return's residual is (outer - stages of that same return),
    // which stays correct even though stages are recorded by inner scopes.
    std::uint64_t stage_total_cycles = 0;
    std::uint64_t outer_cycles = 0;
    std::uint32_t outer_count = 0;
    std::uint64_t max_outer_cycles = 0;
    std::uint64_t residual_cycles = 0;
    // A sample whose stage sum exceeded its own outer window. Counted rather
    // than allowed to underflow the residual.
    std::uint32_t residual_clamp_count = 0;
    // A sample whose end timestamp preceded its start, which a migrated thread
    // can produce. Counted and dropped.
    std::uint32_t clamped_sample_count = 0;
    // The VEH path and the DBT adapter both reach the same resolver, so only
    // the outermost frame attributes an outer window.
    std::uint32_t outer_depth = 0;
};

struct Win32AotReturnStageSnapshot
{
    bool enabled = false;
    std::array<std::uint64_t, kAotReturnStageCount> cycles = {};
    std::array<std::uint32_t, kAotReturnStageCount> counts = {};
    std::array<std::uint64_t, kAotReturnStageCount> max_cycles = {};
    std::uint64_t stage_total_cycles = 0;
    std::uint64_t outer_cycles = 0;
    std::uint32_t outer_count = 0;
    std::uint64_t max_outer_cycles = 0;
    std::uint64_t residual_cycles = 0;
    std::uint32_t residual_clamp_count = 0;
    std::uint32_t clamped_sample_count = 0;
};

// One return-dispatch site of the Task 481 policy, resolved against the plan so
// the report names guest addresses rather than indices alone.
struct Win32AotReturnStageSiteObservation
{
    std::uint32_t site_index = 0;
    std::uint32_t guest_source = 0;
    std::uint32_t miss_cache_offset = 0;
    std::uint32_t observation_count = 0;
    std::uint32_t distinct_target_count = 0;
    std::uint32_t bypass_count = 0;
    bool megamorphic = false;
};

// Opt-in, matching REPIU_AOT_RESIDENCY_SAMPLE and REPIU_GLIDE_ORDINAL_TIME_
// PROFILE: unset and empty mean OFF, and only `1|on|true` enables it. The
// Glide-ordinal pass and this pass are run separately so neither instrument
// biases the other's outer bucket.
bool ResolveAotReturnStageProfileEnabled(const char* setting);
bool AotReturnStageProfileEnabled();

void RecordAotReturnStageSample(Win32AotReturnStageProfile* profile,
                                AotReturnStage stage,
                                std::uint64_t start_cycles,
                                std::uint64_t end_cycles);

void RecordAotReturnOuterSample(Win32AotReturnStageProfile* profile,
                                std::uint64_t start_cycles,
                                std::uint64_t end_cycles,
                                std::uint64_t stage_total_at_entry);

Win32AotReturnStageSnapshot SnapshotAotReturnStageProfile(
    const Win32AotReturnStageProfile& profile);

// Sum of the five stages, for coverage against the outer window.
std::uint64_t AotReturnStageCoveredCycles(
    const Win32AotReturnStageSnapshot& snapshot);

std::uint64_t ReadAotReturnStageCycles();

// Ranks the Task 481 policy sites that actually missed, most observations
// first, and keeps at most kAotReturnStageSiteReportCapacity of them. Called
// once at teardown, never on the return path.
void RankAotReturnStageSites(
    const Win32AotReturnPatchPolicy& policy,
    const std::vector<runtime::AotDbtReturnDispatchSite>& sites,
    std::vector<Win32AotReturnStageSiteObservation>* observations);

// Times one stage. Reads the clock only while the profile is enabled, so a
// disabled run pays one predictable branch at each stage boundary.
class AotReturnStageScope
{
public:
    AotReturnStageScope(Win32AotReturnStageProfile* profile,
                        AotReturnStage stage);
    ~AotReturnStageScope();

    AotReturnStageScope(const AotReturnStageScope&) = delete;
    AotReturnStageScope& operator=(const AotReturnStageScope&) = delete;

    // Ends the stage before the scope leaves, so a stage that must stop at a
    // branch does not bleed into the next one.
    void Close();

private:
    Win32AotReturnStageProfile* profile_ = nullptr;
    AotReturnStage stage_ = AotReturnStage::kEntryValidation;
    std::uint64_t start_cycles_ = 0;
};

// Times one whole return, and derives that return's residual from the stages
// recorded inside it.
class AotReturnOuterScope
{
public:
    explicit AotReturnOuterScope(Win32AotReturnStageProfile* profile);
    ~AotReturnOuterScope();

    AotReturnOuterScope(const AotReturnOuterScope&) = delete;
    AotReturnOuterScope& operator=(const AotReturnOuterScope&) = delete;

private:
    Win32AotReturnStageProfile* profile_ = nullptr;
    std::uint64_t start_cycles_ = 0;
    std::uint64_t stage_total_at_entry_ = 0;
    bool owns_depth_ = false;
};

}  // namespace repiu::engine
