#pragma once

#include <cstdint>
#include <string_view>

namespace repiu::engine
{

// Task 726. Observation-only aggregate for explicit AOT stores while a write
// lock exposes the guest-addressable LFB staging range.
struct GlideLfbNativeStoreCensusProfile
{
    bool enabled = false;
    bool range_active = false;
    std::uint32_t range_base = 0;
    std::uint32_t range_byte_count = 0;
    std::uint32_t write_lock_count = 0;
    std::uint32_t completed_write_lock_count = 0;
    std::uint32_t malformed_range_count = 0;
    std::uint64_t observer_call_count = 0;
    std::uint64_t decoded_store_count = 0;
    std::uint64_t active_decoded_store_count = 0;
    std::uint64_t overlapping_store_count = 0;
    std::uint64_t overlapping_byte_count = 0;
    std::uint32_t max_overlapping_byte_count = 0;
};

struct GlideLfbNativeStoreCensusSnapshot
{
    bool enabled = false;
    std::uint32_t write_lock_count = 0;
    std::uint32_t completed_write_lock_count = 0;
    std::uint32_t malformed_range_count = 0;
    std::uint64_t observer_call_count = 0;
    std::uint64_t decoded_store_count = 0;
    std::uint64_t active_decoded_store_count = 0;
    std::uint64_t overlapping_store_count = 0;
    std::uint64_t overlapping_byte_count = 0;
    std::uint32_t max_overlapping_byte_count = 0;
};

bool ResolveGlideLfbNativeStoreCensusEnabled(std::string_view setting);
bool GlideLfbNativeStoreCensusEnabled();
[[nodiscard]] std::uintptr_t GlideLfbNativeStoreCensusActiveFlagAddress();
void BeginGlideLfbNativeStoreCensus(GlideLfbNativeStoreCensusProfile* profile,
                                    std::uint32_t range_base,
                                    std::uint32_t range_byte_count);
void EndGlideLfbNativeStoreCensus(GlideLfbNativeStoreCensusProfile* profile);
void RecordGlideLfbNativeStoreCensus(
    GlideLfbNativeStoreCensusProfile* profile,
    bool decoded,
    std::uint32_t destination,
    std::uint32_t byte_count);
GlideLfbNativeStoreCensusSnapshot SnapshotGlideLfbNativeStoreCensus(
    const GlideLfbNativeStoreCensusProfile& profile);

}  // namespace repiu::engine
