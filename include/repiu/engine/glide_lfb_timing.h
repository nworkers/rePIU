#pragma once

#include <cstdint>
#include <string_view>

namespace repiu::engine
{

// Task 724. Splits the staging seed that grLfbLock performs to preserve pixels
// outside a guest partial write. It is observation-only and off by default.
struct GlideLfbTimingProfile
{
    bool enabled = false;
    std::uint32_t lock_count = 0;
    std::uint32_t readback_success_count = 0;
    std::uint32_t readback_failure_count = 0;
    std::uint32_t encode_success_count = 0;
    std::uint32_t encode_failure_count = 0;
    std::uint64_t readback_cycles = 0;
    std::uint64_t encode_cycles = 0;
    std::uint64_t total_cycles = 0;
    std::uint64_t max_readback_cycles = 0;
    std::uint64_t max_encode_cycles = 0;
    std::uint64_t max_total_cycles = 0;
    std::uint32_t clamped_sample_count = 0;
};

struct GlideLfbTimingSnapshot
{
    bool enabled = false;
    std::uint32_t lock_count = 0;
    std::uint32_t readback_success_count = 0;
    std::uint32_t readback_failure_count = 0;
    std::uint32_t encode_success_count = 0;
    std::uint32_t encode_failure_count = 0;
    std::uint64_t readback_cycles = 0;
    std::uint64_t encode_cycles = 0;
    std::uint64_t total_cycles = 0;
    std::uint64_t max_readback_cycles = 0;
    std::uint64_t max_encode_cycles = 0;
    std::uint64_t max_total_cycles = 0;
    std::uint32_t clamped_sample_count = 0;
};

bool ResolveGlideLfbTimingProfileEnabled(std::string_view setting);
bool GlideLfbTimingProfileEnabled();
std::uint64_t ReadGlideLfbTimingCycles();

void RecordGlideLfbTiming(GlideLfbTimingProfile* profile,
                          bool readback_succeeded,
                          bool encode_attempted,
                          bool encode_succeeded,
                          std::uint64_t entry_cycles,
                          std::uint64_t readback_end_cycles,
                          std::uint64_t encode_end_cycles);

GlideLfbTimingSnapshot SnapshotGlideLfbTiming(
    const GlideLfbTimingProfile& profile);

}  // namespace repiu::engine
