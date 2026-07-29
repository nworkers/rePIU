#include "repiu/platform/win32/glide_buffer_swap_timing.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#endif

namespace repiu::platform::win32
{
namespace
{

bool ReadGlideBufferSwapTimingProfileSetting()
{
    const char* value = std::getenv("REPIU_GLIDE_SWAP_TIME_PROFILE");
    return value != nullptr &&
        ResolveGlideBufferSwapTimingProfileEnabled(value);
}

std::uint64_t CounterDelta(
    Win32GlideBufferSwapTimingProfile* profile,
    std::uint64_t before,
    std::uint64_t after)
{
    if (after >= before)
    {
        return after - before;
    }
    ++profile->clamped_sample_count;
    return 0;
}

}  // namespace

bool ResolveGlideBufferSwapTimingProfileEnabled(std::string_view setting)
{
    return setting == "1" || setting == "on" || setting == "true";
}

bool GlideBufferSwapTimingProfileEnabled()
{
    static const bool enabled = ReadGlideBufferSwapTimingProfileSetting();
    return enabled;
}

std::uint64_t ReadGlideBufferSwapTimingCycles()
{
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    return __rdtsc();
#else
    return static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

void RecordGlideBufferSwapTiming(
    Win32GlideBufferSwapTimingProfile* profile,
    std::uint32_t requested_interval,
    bool succeeded,
    std::uint64_t entry_cycles,
    std::uint64_t present_start_cycles,
    std::uint64_t present_end_cycles,
    std::uint64_t accounting_end_cycles,
    std::uint64_t finish_cycles)
{
    if (profile == nullptr)
    {
        return;
    }

    profile->enabled = true;
    ++profile->call_count;
    if (succeeded)
    {
        ++profile->success_count;
    }
    else
    {
        ++profile->failure_count;
    }

    if (requested_interval == 0U)
    {
        ++profile->requested_zero_count;
    }
    else if (requested_interval == 1U)
    {
        ++profile->requested_one_count;
    }
    else
    {
        ++profile->requested_other_count;
    }
    profile->requested_minimum =
        std::min(profile->requested_minimum, requested_interval);
    profile->requested_maximum =
        std::max(profile->requested_maximum, requested_interval);
    profile->requested_last = requested_interval;

    const std::uint64_t setup = CounterDelta(
        profile, entry_cycles, present_start_cycles);
    const std::uint64_t present = CounterDelta(
        profile, present_start_cycles, present_end_cycles);
    const std::uint64_t accounting = CounterDelta(
        profile, present_end_cycles, accounting_end_cycles);
    const std::uint64_t finalize = CounterDelta(
        profile, accounting_end_cycles, finish_cycles);
    const std::uint64_t total = CounterDelta(
        profile, entry_cycles, finish_cycles);
    profile->setup_cycles += setup;
    profile->present_cycles += present;
    profile->accounting_cycles += accounting;
    profile->finalize_cycles += finalize;
    profile->total_cycles += total;
    profile->max_present_cycles =
        std::max(profile->max_present_cycles, present);
}

void RecordGlideBufferSwapSdlInterval(
    Win32GlideBufferSwapTimingProfile* profile,
    bool succeeded,
    std::int32_t interval)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->enabled = true;
    ++profile->sdl_interval_query_count;
    if (succeeded)
    {
        ++profile->sdl_interval_query_success_count;
        profile->observed_sdl_interval = interval;
    }
    else
    {
        ++profile->sdl_interval_query_failure_count;
    }
}

Win32GlideBufferSwapTimingSnapshot SnapshotGlideBufferSwapTiming(
    const Win32GlideBufferSwapTimingProfile& profile)
{
    Win32GlideBufferSwapTimingSnapshot snapshot;
    snapshot.enabled = profile.enabled;
    snapshot.call_count = profile.call_count;
    snapshot.success_count = profile.success_count;
    snapshot.failure_count = profile.failure_count;
    snapshot.clamped_sample_count = profile.clamped_sample_count;
    snapshot.setup_cycles = profile.setup_cycles;
    snapshot.present_cycles = profile.present_cycles;
    snapshot.accounting_cycles = profile.accounting_cycles;
    snapshot.finalize_cycles = profile.finalize_cycles;
    snapshot.total_cycles = profile.total_cycles;
    snapshot.max_present_cycles = profile.max_present_cycles;
    snapshot.requested_zero_count = profile.requested_zero_count;
    snapshot.requested_one_count = profile.requested_one_count;
    snapshot.requested_other_count = profile.requested_other_count;
    snapshot.requested_minimum =
        profile.call_count == 0U ? 0U : profile.requested_minimum;
    snapshot.requested_maximum = profile.requested_maximum;
    snapshot.requested_last = profile.requested_last;
    snapshot.sdl_interval_query_count = profile.sdl_interval_query_count;
    snapshot.sdl_interval_query_success_count =
        profile.sdl_interval_query_success_count;
    snapshot.sdl_interval_query_failure_count =
        profile.sdl_interval_query_failure_count;
    snapshot.observed_sdl_interval = profile.observed_sdl_interval;
    return snapshot;
}

}  // namespace repiu::platform::win32
