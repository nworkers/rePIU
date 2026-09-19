#include "repiu/engine/glide_lfb_timing.h"

#include "repiu/platform/host_time.h"

#include <algorithm>
#include <cstdlib>

namespace repiu::engine
{
namespace
{

std::uint64_t CounterDelta(GlideLfbTimingProfile* const profile,
                           const std::uint64_t before,
                           const std::uint64_t after)
{
    if (after >= before)
    {
        return after - before;
    }
    ++profile->clamped_sample_count;
    return 0U;
}

}  // namespace

bool ResolveGlideLfbTimingProfileEnabled(const std::string_view setting)
{
    return setting == "1" || setting == "on" || setting == "true";
}

bool GlideLfbTimingProfileEnabled()
{
    static const bool enabled = [] {
        const char* const value = std::getenv("REPIU_GLIDE_LFB_TIME_PROFILE");
        return value != nullptr && ResolveGlideLfbTimingProfileEnabled(value);
    }();
    return enabled;
}

std::uint64_t ReadGlideLfbTimingCycles()
{
    return repiu::platform::ReadCycleCounter();
}

void RecordGlideLfbTiming(GlideLfbTimingProfile* const profile,
                          const bool readback_succeeded,
                          const bool encode_attempted,
                          const bool encode_succeeded,
                          const std::uint64_t entry_cycles,
                          const std::uint64_t readback_end_cycles,
                          const std::uint64_t encode_end_cycles)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->enabled = true;
    ++profile->lock_count;
    readback_succeeded ? ++profile->readback_success_count
                       : ++profile->readback_failure_count;
    if (encode_attempted)
    {
        encode_succeeded ? ++profile->encode_success_count
                         : ++profile->encode_failure_count;
    }
    const std::uint64_t readback =
        CounterDelta(profile, entry_cycles, readback_end_cycles);
    const std::uint64_t encode =
        CounterDelta(profile, readback_end_cycles, encode_end_cycles);
    const std::uint64_t total =
        CounterDelta(profile, entry_cycles, encode_end_cycles);
    profile->readback_cycles += readback;
    profile->encode_cycles += encode;
    profile->total_cycles += total;
    profile->max_readback_cycles = std::max(profile->max_readback_cycles, readback);
    profile->max_encode_cycles = std::max(profile->max_encode_cycles, encode);
    profile->max_total_cycles = std::max(profile->max_total_cycles, total);
}

GlideLfbTimingSnapshot SnapshotGlideLfbTiming(
    const GlideLfbTimingProfile& profile)
{
    GlideLfbTimingSnapshot snapshot;
    snapshot.enabled = profile.enabled;
    snapshot.lock_count = profile.lock_count;
    snapshot.readback_success_count = profile.readback_success_count;
    snapshot.readback_failure_count = profile.readback_failure_count;
    snapshot.encode_success_count = profile.encode_success_count;
    snapshot.encode_failure_count = profile.encode_failure_count;
    snapshot.readback_cycles = profile.readback_cycles;
    snapshot.encode_cycles = profile.encode_cycles;
    snapshot.total_cycles = profile.total_cycles;
    snapshot.max_readback_cycles = profile.max_readback_cycles;
    snapshot.max_encode_cycles = profile.max_encode_cycles;
    snapshot.max_total_cycles = profile.max_total_cycles;
    snapshot.clamped_sample_count = profile.clamped_sample_count;
    return snapshot;
}

}  // namespace repiu::engine
