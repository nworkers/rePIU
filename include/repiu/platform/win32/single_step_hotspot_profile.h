#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace repiu::platform::win32
{

constexpr std::uint32_t kWin32SingleStepHotspotCapacity = 8192U;
constexpr std::uint32_t kWin32SingleStepHotspotReportCapacity = 32U;

enum class SingleStepProfileOutcome : std::uint32_t
{
    kHandledHle = 0,
    kTimerInterrupt,
    kNativeExecution,
    kTrapFlagRearm,
    kCount,
};

constexpr std::uint32_t kSingleStepProfileOutcomeCount =
    static_cast<std::uint32_t>(SingleStepProfileOutcome::kCount);

// Sequential (never nested) regions of HandleSingleStepTrace, in execution
// order. Task 322 measures these to settle whether the HLE tick share found in
// Task 309 belongs to the emulation body (kHleDispatch) or to translation cache
// work on the AOT re-entry path (kAotResume). The stage axis is orthogonal to
// SingleStepProfileOutcome; every sample is recorded on both.
// See docs/design/20260727-322-single-step-handler-stage-attribution.md.
enum class SingleStepProfileStage : std::uint32_t
{
    kPrologueTrace = 0,
    kHleDispatch,
    kAotResume,
    kInterruptInjection,
    kNativeEntry,
    // Task 323 sub-stages of kAotResume, in the execution order of
    // TryResumeAotAfterHandledHle. They open only inside kAotResume, so
    // sum(sub-stage) <= kAotResume holds. Appended after the original five so
    // existing array indices and log field order stay stable.
    kSegmentWriteProbe,
    kQuarantineCheck,
    kCacheLookup,
    kSpanSafety,
    kCount,
};

constexpr std::uint32_t kSingleStepProfileFirstAotResumeSubStage =
    static_cast<std::uint32_t>(SingleStepProfileStage::kSegmentWriteProbe);

constexpr std::uint32_t kSingleStepProfileStageCount =
    static_cast<std::uint32_t>(SingleStepProfileStage::kCount);

struct Win32SingleStepHotspotEntry
{
    std::uint32_t guest_address = 0;
    std::uint32_t sample_count = 0;
    std::uint64_t total_cycles = 0;
    std::uint64_t max_cycles = 0;
    std::array<std::uint32_t, kSingleStepProfileOutcomeCount>
        outcome_counts = {};
    std::array<std::uint64_t, kSingleStepProfileOutcomeCount>
        outcome_cycles = {};
    std::array<std::uint32_t, kSingleStepProfileStageCount>
        stage_counts = {};
    std::array<std::uint64_t, kSingleStepProfileStageCount>
        stage_cycles = {};
    bool occupied = false;
};

struct Win32SingleStepHotspotProfile
{
    bool enabled = false;
    std::uint32_t total_sample_count = 0;
    std::uint32_t distinct_guest_count = 0;
    std::uint32_t overflow_count = 0;
    std::uint64_t total_cycles = 0;
    std::uint64_t max_cycles = 0;
    std::array<std::uint32_t, kSingleStepProfileOutcomeCount>
        outcome_counts = {};
    std::array<std::uint64_t, kSingleStepProfileOutcomeCount>
        outcome_cycles = {};
    std::array<std::uint32_t, kSingleStepProfileStageCount>
        stage_counts = {};
    std::array<std::uint64_t, kSingleStepProfileStageCount>
        stage_cycles = {};
    std::array<Win32SingleStepHotspotEntry,
               kWin32SingleStepHotspotCapacity> entries = {};
    // Task 401: teardown can hang after the guest thread stops, so the dump is
    // written as early as teardown allows and reported again later. These keep
    // the second call from rewriting the file and let it report the same
    // numbers.
    bool dump_written = false;
    std::uint32_t dump_entry_count = 0;
};

struct Win32SingleStepHotspotSample
{
    std::uint32_t guest_address = 0;
    std::uint32_t sample_count = 0;
    std::uint64_t total_cycles = 0;
    std::uint64_t max_cycles = 0;
    std::array<std::uint32_t, kSingleStepProfileOutcomeCount>
        outcome_counts = {};
    std::array<std::uint64_t, kSingleStepProfileOutcomeCount>
        outcome_cycles = {};
    std::array<std::uint32_t, kSingleStepProfileStageCount>
        stage_counts = {};
    std::array<std::uint64_t, kSingleStepProfileStageCount>
        stage_cycles = {};
};

struct Win32SingleStepHotspotProfileSnapshot
{
    bool enabled = false;
    std::uint32_t total_sample_count = 0;
    std::uint32_t distinct_guest_count = 0;
    std::uint32_t overflow_count = 0;
    std::uint64_t total_cycles = 0;
    std::uint64_t max_cycles = 0;
    std::array<std::uint32_t, kSingleStepProfileOutcomeCount>
        outcome_counts = {};
    std::array<std::uint64_t, kSingleStepProfileOutcomeCount>
        outcome_cycles = {};
    std::array<std::uint32_t, kSingleStepProfileStageCount>
        stage_counts = {};
    std::array<std::uint64_t, kSingleStepProfileStageCount>
        stage_cycles = {};
    std::uint32_t count_hotspot_count = 0;
    std::uint32_t cycle_hotspot_count = 0;
    std::uint32_t top_count_coverage_count = 0;
    std::uint64_t top_cycle_coverage_cycles = 0;
    std::array<Win32SingleStepHotspotSample,
               kWin32SingleStepHotspotReportCapacity> count_hotspots = {};
    std::array<Win32SingleStepHotspotSample,
               kWin32SingleStepHotspotReportCapacity> cycle_hotspots = {};
    // Task 400: the top-32 lists answer "where is time spent", not "what else
    // ran at all". A stall diagnosis needs the second question, and a routine
    // executing two orders of magnitude less often than the hot loop cannot
    // reach a 32-entry list. The full-table dump carries every occupied entry.
    bool dump_written = false;
    std::uint32_t dump_entry_count = 0;
    std::string dump_path;
};

// Per-sample stage totals collected by one SingleStepHotspotCycleScope before
// they are folded into the profile.
struct Win32SingleStepStageTally
{
    std::array<std::uint32_t, kSingleStepProfileStageCount> counts = {};
    std::array<std::uint64_t, kSingleStepProfileStageCount> cycles = {};
};

bool ResolveSingleStepHotspotProfileEnabled(std::string_view setting);
bool SingleStepHotspotProfileEnabled();

void RecordSingleStepHotspot(
    Win32SingleStepHotspotProfile* profile,
    std::uint32_t guest_address,
    std::uint64_t cycles,
    SingleStepProfileOutcome outcome,
    const Win32SingleStepStageTally* stages = nullptr);

Win32SingleStepHotspotProfileSnapshot SnapshotSingleStepHotspotProfile(
    const Win32SingleStepHotspotProfile& profile);

// `REPIU_SINGLE_STEP_HOTSPOT_DUMP`: unset or empty disables the dump, "1"
// selects build/single_step_hotspot.txt, anything else is used as the path.
std::filesystem::path ResolveSingleStepHotspotDumpPath(
    std::string_view setting);

std::filesystem::path SingleStepHotspotDumpPath();

// Writes every occupied table entry, ordered by sample count, so a stalled run
// can be read as a complete execution census rather than a top-N ranking.
bool WriteSingleStepHotspotDump(
    const std::filesystem::path& path,
    Win32SingleStepHotspotProfile* profile,
    std::uint32_t* written_entry_count);

// Resolves the configured path and writes once. Safe to call from several
// teardown points; only the first call touches the file.
bool WriteSingleStepHotspotDumpIfEnabled(
    Win32SingleStepHotspotProfile* profile,
    std::uint32_t* written_entry_count,
    std::string* resolved_path);

class SingleStepHotspotCycleScope
{
public:
    SingleStepHotspotCycleScope(
        Win32SingleStepHotspotProfile* profile,
        std::uint32_t guest_address);
    ~SingleStepHotspotCycleScope();

    SingleStepHotspotCycleScope(const SingleStepHotspotCycleScope&) = delete;
    SingleStepHotspotCycleScope& operator=(
        const SingleStepHotspotCycleScope&) = delete;

    void SetOutcome(SingleStepProfileOutcome outcome);

    // False when the profile is disabled, so stage scopes can skip __rdtsc
    // entirely and cost one branch on the normal path.
    bool active() const { return profile_ != nullptr; }

    void AddStageCycles(SingleStepProfileStage stage, std::uint64_t cycles);

private:
    Win32SingleStepHotspotProfile* profile_ = nullptr;
    std::uint32_t guest_address_ = 0;
    std::uint64_t start_cycles_ = 0;
    SingleStepProfileOutcome outcome_ =
        SingleStepProfileOutcome::kTrapFlagRearm;
    Win32SingleStepStageTally stages_ = {};
};

// Measures one sequential region of HandleSingleStepTrace. Stage scopes must
// never nest inside one another, otherwise the enclosing interval would be
// counted twice against the same sample.
class SingleStepHotspotStageScope
{
public:
    SingleStepHotspotStageScope(SingleStepHotspotCycleScope& parent,
                                SingleStepProfileStage stage);
    // Null-tolerant form for call sites that reach a measured region through
    // ThreadContext::active_hotspot_scope rather than owning the parent scope
    // (Task 323). A null parent makes the scope inert.
    SingleStepHotspotStageScope(SingleStepHotspotCycleScope* parent,
                                SingleStepProfileStage stage);
    ~SingleStepHotspotStageScope();

    SingleStepHotspotStageScope(const SingleStepHotspotStageScope&) = delete;
    SingleStepHotspotStageScope& operator=(
        const SingleStepHotspotStageScope&) = delete;

private:
    SingleStepHotspotCycleScope* parent_ = nullptr;
    SingleStepProfileStage stage_ = SingleStepProfileStage::kPrologueTrace;
    std::uint64_t start_cycles_ = 0;
};

}  // namespace repiu::platform::win32
