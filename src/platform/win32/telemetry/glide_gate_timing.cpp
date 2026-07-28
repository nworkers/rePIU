#include "repiu/platform/win32/glide_gate_timing.h"

#include <algorithm>
#include <chrono>

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#endif

namespace repiu::platform::win32
{

std::uint64_t ReadGlideGateTimingCycles()
{
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    return __rdtsc();
#else
    return static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

std::uint64_t GlideGateTimingDelta(Win32GlideGateTimingProfile* profile,
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

void RecordGlideGatePublish(Win32GlideGateTimingProfile* profile,
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

void RecordGlideGateHostCommand(Win32GlideGateTimingProfile* profile,
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

void RecordGlideGateResume(Win32GlideGateTimingProfile* profile,
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

void RecordGlideGateDirectCommand(Win32GlideGateTimingProfile* profile,
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

Win32GlideGateTimingSnapshot SnapshotGlideGateTiming(
    const Win32GlideGateTimingProfile& profile)
{
    Win32GlideGateTimingSnapshot snapshot;
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

}  // namespace repiu::platform::win32
