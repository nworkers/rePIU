#pragma once

#include <array>
#include <cstdint>
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
    std::array<Win32SingleStepHotspotEntry,
               kWin32SingleStepHotspotCapacity> entries = {};
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
    std::uint32_t count_hotspot_count = 0;
    std::uint32_t cycle_hotspot_count = 0;
    std::uint32_t top_count_coverage_count = 0;
    std::uint64_t top_cycle_coverage_cycles = 0;
    std::array<Win32SingleStepHotspotSample,
               kWin32SingleStepHotspotReportCapacity> count_hotspots = {};
    std::array<Win32SingleStepHotspotSample,
               kWin32SingleStepHotspotReportCapacity> cycle_hotspots = {};
};

bool ResolveSingleStepHotspotProfileEnabled(std::string_view setting);
bool SingleStepHotspotProfileEnabled();

void RecordSingleStepHotspot(
    Win32SingleStepHotspotProfile* profile,
    std::uint32_t guest_address,
    std::uint64_t cycles,
    SingleStepProfileOutcome outcome);

Win32SingleStepHotspotProfileSnapshot SnapshotSingleStepHotspotProfile(
    const Win32SingleStepHotspotProfile& profile);

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

private:
    Win32SingleStepHotspotProfile* profile_ = nullptr;
    std::uint32_t guest_address_ = 0;
    std::uint64_t start_cycles_ = 0;
    SingleStepProfileOutcome outcome_ =
        SingleStepProfileOutcome::kTrapFlagRearm;
};

}  // namespace repiu::platform::win32
