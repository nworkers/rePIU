#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace repiu::engine
{

// Task 725. Opt-in byte-difference evidence for guest writes through lfbPtr.
struct GlideLfbWriteFootprintProfile
{
    bool enabled = false;
    bool baseline_active = false;
    std::uint32_t baseline_width = 0;
    std::uint32_t baseline_height = 0;
    std::vector<std::uint8_t> baseline;
    std::uint32_t write_lock_count = 0;
    std::uint32_t compared_lock_count = 0;
    std::uint32_t unchanged_lock_count = 0;
    std::uint32_t partial_extent_lock_count = 0;
    std::uint32_t full_extent_lock_count = 0;
    std::uint32_t all_pixels_changed_lock_count = 0;
    std::uint32_t malformed_baseline_count = 0;
    std::uint64_t changed_pixel_count = 0;
    std::uint64_t max_changed_pixel_count = 0;
    std::uint64_t max_bounding_box_pixel_count = 0;
};

struct GlideLfbWriteFootprintSnapshot
{
    bool enabled = false;
    std::uint32_t write_lock_count = 0;
    std::uint32_t compared_lock_count = 0;
    std::uint32_t unchanged_lock_count = 0;
    std::uint32_t partial_extent_lock_count = 0;
    std::uint32_t full_extent_lock_count = 0;
    std::uint32_t all_pixels_changed_lock_count = 0;
    std::uint32_t malformed_baseline_count = 0;
    std::uint64_t changed_pixel_count = 0;
    std::uint64_t max_changed_pixel_count = 0;
    std::uint64_t max_bounding_box_pixel_count = 0;
};

bool ResolveGlideLfbWriteFootprintEnabled(std::string_view setting);
bool GlideLfbWriteFootprintEnabled();
void BeginGlideLfbWriteFootprint(GlideLfbWriteFootprintProfile* profile,
                                 const std::uint8_t* pixels,
                                 std::size_t byte_count,
                                 std::uint32_t width,
                                 std::uint32_t height);
void CompareGlideLfbWriteFootprint(GlideLfbWriteFootprintProfile* profile,
                                   const std::uint8_t* pixels,
                                   std::size_t byte_count,
                                   std::uint32_t width,
                                   std::uint32_t height);
GlideLfbWriteFootprintSnapshot SnapshotGlideLfbWriteFootprint(
    const GlideLfbWriteFootprintProfile& profile);

}  // namespace repiu::engine
