#include "repiu/engine/execution_time_profile.h"

#include <chrono>
#include <cstdlib>
#include "repiu/platform/host_time.h"


namespace repiu::engine
{
namespace
{

std::uint64_t ReadExecutionTimeCycles()
{
    return repiu::platform::ReadCycleCounter();
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

void RecordExecutionTimeBucket(ExecutionTimeProfile* profile,
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

void RecordVehExceptionGap(ExecutionTimeProfile* profile,
                           VehGapClass gap_class)
{
    if (profile == nullptr || profile->veh_gap_pending_cycles == 0U)
    {
        return;
    }
    const std::uint32_t index = static_cast<std::uint32_t>(gap_class);
    if (index >= kVehGapClassCount)
    {
        return;
    }
    profile->veh_gap_cycles[index] += profile->veh_gap_pending_cycles;
    ++profile->veh_gap_counts[index];
    // The gap was banked as unclassified when the scope opened, because the
    // exception code was not readable yet. Move it out now so the three classes
    // plus the residual always sum to what was banked.
    profile->veh_gap_unclassified_cycles =
        profile->veh_gap_unclassified_cycles >= profile->veh_gap_pending_cycles
            ? profile->veh_gap_unclassified_cycles -
                profile->veh_gap_pending_cycles
            : 0U;
    // Consumed: a handler that runs the census twice for one exception must not
    // attribute the same interval to two classes.
    profile->veh_gap_pending_cycles = 0;
}

ExecutionTimeProfileSnapshot SnapshotExecutionTimeProfile(
    const ExecutionTimeProfile& profile)
{
    ExecutionTimeProfileSnapshot snapshot;
    snapshot.enabled = profile.enabled;
    snapshot.veh_gap_cycles = profile.veh_gap_cycles;
    snapshot.veh_gap_counts = profile.veh_gap_counts;
    snapshot.veh_gap_min_cycles = profile.veh_gap_min_cycles;
    snapshot.veh_gap_max_cycles = profile.veh_gap_max_cycles;
    snapshot.veh_gap_unclassified_cycles = profile.veh_gap_unclassified_cycles;
    snapshot.veh_gap_clamped_count = profile.veh_gap_clamped_count;
    snapshot.cycles = profile.cycles;
    snapshot.counts = profile.counts;
    snapshot.inside_veh_cycles = profile.inside_veh_cycles;
    snapshot.inside_veh_counts = profile.inside_veh_counts;
    snapshot.glide_gate_prologue_cycles = profile.glide_gate_prologue_cycles;
    snapshot.glide_gate_prologue_count = profile.glide_gate_prologue_count;
    snapshot.glide_gate_prologue_clamped_count =
        profile.glide_gate_prologue_clamped_count;
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

ExecutionTimeShares ComputeExecutionTimeShares(
    const ExecutionTimeProfileSnapshot& snapshot)
{
    const auto bucket = [&snapshot](ExecutionTimeBucket id) {
        return snapshot.cycles[static_cast<std::uint32_t>(id)];
    };
    const auto inside = [&snapshot](ExecutionTimeBucket id) {
        return snapshot.inside_veh_cycles[static_cast<std::uint32_t>(id)];
    };

    ExecutionTimeShares shares;
    shares.total = bucket(ExecutionTimeBucket::kGuestRunTotal);
    shares.veh = bucket(ExecutionTimeBucket::kVehTotal);
    shares.glide_gate = bucket(ExecutionTimeBucket::kGlideGate);
    shares.port_io = bucket(ExecutionTimeBucket::kPortIoDevice);
    shares.dos_service = bucket(ExecutionTimeBucket::kDosService);

    // The buckets are not mutually exclusive: a service is reachable both from
    // inside the handler and from outside it, so each one carries the portion
    // entered while the handler was on the stack. That is what makes these two
    // subtractions the only way to state exclusivity.
    const std::uint64_t service_inside =
        inside(ExecutionTimeBucket::kGlideGate) +
        inside(ExecutionTimeBucket::kPortIoDevice) +
        inside(ExecutionTimeBucket::kDosService);
    const std::uint64_t service_outside =
        (shares.glide_gate - inside(ExecutionTimeBucket::kGlideGate)) +
        (shares.port_io - inside(ExecutionTimeBucket::kPortIoDevice)) +
        (shares.dos_service - inside(ExecutionTimeBucket::kDosService));

    shares.veh_exclusive =
        shares.veh > service_inside ? shares.veh - service_inside : 0U;
    const std::uint64_t accounted = shares.veh + service_outside;
    shares.unaccounted =
        shares.total > accounted ? shares.total - accounted : 0U;
    return shares;
}

ExecutionTimeScope::ExecutionTimeScope(ExecutionTimeProfile* profile,
                                       ExecutionTimeBucket bucket,
                                       std::uint64_t* completed_cycles)
    : profile_(profile),
      bucket_(bucket),
      completed_cycles_(completed_cycles)
{
    if (completed_cycles_ != nullptr)
    {
        *completed_cycles_ = 0U;
    }
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
    // Task 368 stage one: capture the outermost VEH entry, then, when the Glide
    // gate scope opens inside it, bank the interval between the two. That
    // interval is the kernel transition plus everything the handler did to
    // reach the gate -- what exception-free dispatch would remove -- and it
    // reuses both existing timestamps rather than reading the clock again.
    if (bucket_ == ExecutionTimeBucket::kVehTotal)
    {
        if (owns_veh_depth_)
        {
            profile_->veh_entry_cycles = start_cycles_;
            // Task 372: the interval since the previous handler returned --
            // kernel return, guest execution, kernel delivery. Only the outermost
            // frame banks it, so a fault raised inside a handler does not count
            // an interval that never contained guest execution.
            if (profile_->veh_last_exit_cycles != 0U)
            {
                if (start_cycles_ >= profile_->veh_last_exit_cycles)
                {
                    const std::uint64_t gap =
                        start_cycles_ - profile_->veh_last_exit_cycles;
                    profile_->veh_gap_pending_cycles = gap;
                    profile_->veh_gap_unclassified_cycles += gap;
                    if (profile_->veh_gap_min_cycles == 0U ||
                        gap < profile_->veh_gap_min_cycles)
                    {
                        profile_->veh_gap_min_cycles = gap;
                    }
                    if (gap > profile_->veh_gap_max_cycles)
                    {
                        profile_->veh_gap_max_cycles = gap;
                    }
                }
                else
                {
                    ++profile_->veh_gap_clamped_count;
                }
            }
        }
    }
    else if (bucket_ == ExecutionTimeBucket::kGlideGate && inside_veh_ &&
             profile_->veh_entry_cycles != 0U)
    {
        if (start_cycles_ >= profile_->veh_entry_cycles)
        {
            profile_->glide_gate_prologue_cycles +=
                start_cycles_ - profile_->veh_entry_cycles;
            ++profile_->glide_gate_prologue_count;
        }
        else
        {
            ++profile_->glide_gate_prologue_clamped_count;
        }
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
        // Task 372: the far end of the gap the next handler entry will measure.
        profile_->veh_last_exit_cycles = end_cycles;
        // An unclassified pending gap stays in the residual rather than leaking
        // into the next exception's measurement.
        profile_->veh_gap_pending_cycles = 0;
    }
    const std::uint64_t cycles = end_cycles - start_cycles_;
    if (completed_cycles_ != nullptr)
    {
        *completed_cycles_ = cycles;
    }
    RecordExecutionTimeBucket(profile_, bucket_, cycles, inside_veh_);
}

}  // namespace repiu::engine
