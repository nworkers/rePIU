#include "repiu/platform/win32/execution_time_profile.h"

#include <chrono>
#include <cstdlib>

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#endif

namespace repiu::platform::win32
{
namespace
{

std::uint64_t ReadExecutionTimeCycles()
{
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    return __rdtsc();
#else
    return static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

bool ReadExecutionTimeProfileSetting()
{
    const char* value = std::getenv("REPIU_EXECUTION_TIME_PROFILE");
    return value != nullptr && ResolveExecutionTimeProfileEnabled(value);
}

}  // namespace

bool ResolveExecutionTimeProfileEnabled(std::string_view setting)
{
    return setting == "1" || setting == "on" || setting == "true";
}

bool ExecutionTimeProfileEnabled()
{
    static const bool enabled = ReadExecutionTimeProfileSetting();
    return enabled;
}

void RecordExecutionTimeBucket(Win32ExecutionTimeProfile* profile,
                               ExecutionTimeBucket bucket,
                               std::uint64_t cycles,
                               bool inside_veh)
{
    if (profile == nullptr)
    {
        return;
    }
    const std::uint32_t index = static_cast<std::uint32_t>(bucket);
    if (index >= kExecutionTimeBucketCount)
    {
        return;
    }
    profile->enabled = true;
    profile->cycles[index] += cycles;
    ++profile->counts[index];
    if (inside_veh)
    {
        profile->inside_veh_cycles[index] += cycles;
        ++profile->inside_veh_counts[index];
    }
}

Win32ExecutionTimeProfileSnapshot SnapshotExecutionTimeProfile(
    const Win32ExecutionTimeProfile& profile)
{
    Win32ExecutionTimeProfileSnapshot snapshot;
    snapshot.enabled = profile.enabled;
    snapshot.cycles = profile.cycles;
    snapshot.counts = profile.counts;
    snapshot.inside_veh_cycles = profile.inside_veh_cycles;
    snapshot.inside_veh_counts = profile.inside_veh_counts;
    // Close the still-open guest run so a timed-out run still reports a
    // denominator. The interval is measured against the same invariant TSC even
    // though the snapshot runs on the host thread.
    if (profile.guest_run_open)
    {
        const std::uint32_t index =
            static_cast<std::uint32_t>(ExecutionTimeBucket::kGuestRunTotal);
        const std::uint64_t now = ReadExecutionTimeCycles();
        if (now > profile.guest_run_start_cycles)
        {
            snapshot.cycles[index] += now - profile.guest_run_start_cycles;
            ++snapshot.counts[index];
        }
    }
    return snapshot;
}

ExecutionTimeScope::ExecutionTimeScope(Win32ExecutionTimeProfile* profile,
                                       ExecutionTimeBucket bucket)
    : profile_(profile),
      bucket_(bucket)
{
    if (profile_ == nullptr)
    {
        return;
    }
    // A service bucket is "inside the VEH" when a VEH frame is already open.
    // The VEH bucket itself is never marked inside its own frame; it tracks
    // depth instead so a nested fault does not double count.
    if (bucket_ == ExecutionTimeBucket::kVehTotal)
    {
        owns_veh_depth_ = profile_->veh_depth == 0U;
        ++profile_->veh_depth;
    }
    else
    {
        inside_veh_ = profile_->veh_depth != 0U;
    }
    start_cycles_ = ReadExecutionTimeCycles();
    if (bucket_ == ExecutionTimeBucket::kGuestRunTotal)
    {
        profile_->enabled = true;
        profile_->guest_run_start_cycles = start_cycles_;
        profile_->guest_run_open = true;
    }
}

ExecutionTimeScope::~ExecutionTimeScope()
{
    if (profile_ == nullptr)
    {
        return;
    }
    const std::uint64_t end_cycles = ReadExecutionTimeCycles();
    if (bucket_ == ExecutionTimeBucket::kGuestRunTotal)
    {
        profile_->guest_run_open = false;
    }
    if (bucket_ == ExecutionTimeBucket::kVehTotal)
    {
        if (profile_->veh_depth != 0U)
        {
            --profile_->veh_depth;
        }
        if (!owns_veh_depth_)
        {
            return;
        }
    }
    RecordExecutionTimeBucket(
        profile_, bucket_, end_cycles - start_cycles_, inside_veh_);
}

}  // namespace repiu::platform::win32
