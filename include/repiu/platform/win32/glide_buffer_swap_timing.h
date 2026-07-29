#pragma once

#include <cstdint>
#include <string_view>

namespace repiu::platform::win32
{

struct Win32GlideBufferSwapTimingProfile
{
    bool enabled = false;
    std::uint32_t call_count = 0;
    std::uint32_t success_count = 0;
    std::uint32_t failure_count = 0;
    std::uint32_t clamped_sample_count = 0;
    std::uint64_t setup_cycles = 0;
    std::uint64_t present_cycles = 0;
    std::uint64_t accounting_cycles = 0;
    std::uint64_t finalize_cycles = 0;
    std::uint64_t total_cycles = 0;
    std::uint64_t max_present_cycles = 0;
    std::uint32_t requested_zero_count = 0;
    std::uint32_t requested_one_count = 0;
    std::uint32_t requested_other_count = 0;
    std::uint32_t requested_minimum = UINT32_MAX;
    std::uint32_t requested_maximum = 0;
    std::uint32_t requested_last = 0;
    std::uint32_t sdl_interval_query_count = 0;
    std::uint32_t sdl_interval_query_success_count = 0;
    std::uint32_t sdl_interval_query_failure_count = 0;
    std::int32_t observed_sdl_interval = 0;
};

struct Win32GlideBufferSwapTimingSnapshot
{
    bool enabled = false;
    std::uint32_t call_count = 0;
    std::uint32_t success_count = 0;
    std::uint32_t failure_count = 0;
    std::uint32_t clamped_sample_count = 0;
    std::uint64_t setup_cycles = 0;
    std::uint64_t present_cycles = 0;
    std::uint64_t accounting_cycles = 0;
    std::uint64_t finalize_cycles = 0;
    std::uint64_t total_cycles = 0;
    std::uint64_t max_present_cycles = 0;
    std::uint32_t requested_zero_count = 0;
    std::uint32_t requested_one_count = 0;
    std::uint32_t requested_other_count = 0;
    std::uint32_t requested_minimum = 0;
    std::uint32_t requested_maximum = 0;
    std::uint32_t requested_last = 0;
    std::uint32_t sdl_interval_query_count = 0;
    std::uint32_t sdl_interval_query_success_count = 0;
    std::uint32_t sdl_interval_query_failure_count = 0;
    std::int32_t observed_sdl_interval = 0;
};

bool ResolveGlideBufferSwapTimingProfileEnabled(std::string_view setting);
bool GlideBufferSwapTimingProfileEnabled();

std::uint64_t ReadGlideBufferSwapTimingCycles();

void RecordGlideBufferSwapTiming(
    Win32GlideBufferSwapTimingProfile* profile,
    std::uint32_t requested_interval,
    bool succeeded,
    std::uint64_t entry_cycles,
    std::uint64_t present_start_cycles,
    std::uint64_t present_end_cycles,
    std::uint64_t accounting_end_cycles,
    std::uint64_t finish_cycles);

void RecordGlideBufferSwapSdlInterval(
    Win32GlideBufferSwapTimingProfile* profile,
    bool succeeded,
    std::int32_t interval);

Win32GlideBufferSwapTimingSnapshot SnapshotGlideBufferSwapTiming(
    const Win32GlideBufferSwapTimingProfile& profile);

}  // namespace repiu::platform::win32
