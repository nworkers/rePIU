#include "repiu/engine/glide_gate_timing.h"

#include <algorithm>
#include <chrono>
#include "repiu/platform/host_time.h"


namespace repiu::engine
{

std::uint64_t ReadGlideGateTimingCycles()
{
    return repiu::platform::ReadCycleCounter();
}

std::uint64_t GlideGateTimingDelta(GlideGateTimingProfile* profile,
                                   std::uint64_t start,
                                   std::uint64_t end)
{
    if (end >= start)
    {
        return end - start;
    }
    if (profile != nullptr)
    {
        ++profile->clamped_sample_count;
    }
    return 0U;
}

void RecordGlideGatePublish(GlideGateTimingProfile* profile,
                            std::uint64_t enter_cycles,
                            std::uint64_t publish_cycles)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->enabled = true;
    profile->queue_cycles +=
        GlideGateTimingDelta(profile, enter_cycles, publish_cycles);
    profile->publish_cycles = publish_cycles;
    // Cleared so a command whose host side never ran cannot be credited with a
    // stale finish timestamp from the previous rendezvous.
    profile->host_start_cycles = 0;
    profile->host_finish_cycles = 0;
}

void RecordGlideGateHostCommand(GlideGateTimingProfile* profile,
                                std::uint64_t start_cycles,
                                std::uint64_t finish_cycles)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->enabled = true;
    const std::uint64_t wake =
        GlideGateTimingDelta(profile, profile->publish_cycles, start_cycles);
    const std::uint64_t work =
        GlideGateTimingDelta(profile, start_cycles, finish_cycles);
    profile->wake_cycles += wake;
    profile->work_cycles += work;
    profile->max_wake_cycles = std::max(profile->max_wake_cycles, wake);
    profile->max_work_cycles = std::max(profile->max_work_cycles, work);
    profile->host_start_cycles = start_cycles;
    profile->host_finish_cycles = finish_cycles;
}

void RecordGlideGateResume(GlideGateTimingProfile* profile,
                           std::uint64_t enter_cycles,
                           std::uint64_t resume_cycles)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->enabled = true;
    ++profile->rendezvous_count;
    profile->complete_cycles += GlideGateTimingDelta(
        profile, profile->host_finish_cycles, resume_cycles);
    const std::uint64_t total =
        GlideGateTimingDelta(profile, enter_cycles, resume_cycles);
    profile->total_cycles += total;
    profile->max_total_cycles = std::max(profile->max_total_cycles, total);
}

void RecordGlideGateDirectCommand(GlideGateTimingProfile* profile,
                                  std::uint64_t cycles)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->enabled = true;
    ++profile->direct_count;
    profile->direct_work_cycles += cycles;
}

GlideGateTimingSnapshot SnapshotGlideGateTiming(
    const GlideGateTimingProfile& profile)
{
    GlideGateTimingSnapshot snapshot;
    snapshot.enabled = profile.enabled;
    snapshot.rendezvous_count = profile.rendezvous_count;
    snapshot.queue_cycles = profile.queue_cycles;
    snapshot.wake_cycles = profile.wake_cycles;
    snapshot.work_cycles = profile.work_cycles;
    snapshot.complete_cycles = profile.complete_cycles;
    snapshot.total_cycles = profile.total_cycles;
    snapshot.direct_count = profile.direct_count;
    snapshot.direct_work_cycles = profile.direct_work_cycles;
    snapshot.max_wake_cycles = profile.max_wake_cycles;
    snapshot.max_work_cycles = profile.max_work_cycles;
    snapshot.max_total_cycles = profile.max_total_cycles;
    snapshot.clamped_sample_count = profile.clamped_sample_count;
    const std::uint64_t named = profile.queue_cycles + profile.wake_cycles +
        profile.work_cycles + profile.complete_cycles;
    snapshot.residual_cycles =
        profile.total_cycles > named ? profile.total_cycles - named : 0U;
    return snapshot;
}

}  // namespace repiu::engine
