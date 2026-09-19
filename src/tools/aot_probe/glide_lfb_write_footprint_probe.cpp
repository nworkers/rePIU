#include "glide_lfb_write_footprint_probe.h"

#include "repiu/engine/glide_lfb_write_footprint.h"

#include <array>
#include <iostream>

namespace repiu::tools
{

bool RunGlideLfbWriteFootprintProbe()
{
    using engine::BeginGlideLfbWriteFootprint;
    using engine::CompareGlideLfbWriteFootprint;
    using engine::GlideLfbWriteFootprintProfile;
    using engine::SnapshotGlideLfbWriteFootprint;

    const bool policy =
        !engine::ResolveGlideLfbWriteFootprintEnabled("") &&
        !engine::ResolveGlideLfbWriteFootprintEnabled("0") &&
        engine::ResolveGlideLfbWriteFootprintEnabled("1") &&
        engine::ResolveGlideLfbWriteFootprintEnabled("on") &&
        engine::ResolveGlideLfbWriteFootprintEnabled("true");

    GlideLfbWriteFootprintProfile profile;
    std::array<std::uint8_t, 16> pixels{};
    BeginGlideLfbWriteFootprint(&profile, pixels.data(), pixels.size(), 4U, 2U);
    CompareGlideLfbWriteFootprint(&profile, pixels.data(), pixels.size(), 4U, 2U);
    pixels[2U] = 1U;
    BeginGlideLfbWriteFootprint(&profile, pixels.data(), pixels.size(), 4U, 2U);
    pixels[2U] = 0U;
    pixels[3U] = 2U;
    CompareGlideLfbWriteFootprint(&profile, pixels.data(), pixels.size(), 4U, 2U);
    BeginGlideLfbWriteFootprint(&profile, pixels.data(), pixels.size(), 4U, 2U);
    for (std::size_t index = 0; index < pixels.size(); index += 2U)
    {
        pixels[index] ^= 1U;
    }
    CompareGlideLfbWriteFootprint(&profile, pixels.data(), pixels.size(), 4U, 2U);
    const auto snapshot = SnapshotGlideLfbWriteFootprint(profile);
    const bool aggregation =
        snapshot.enabled &&
        snapshot.write_lock_count == 3U &&
        snapshot.compared_lock_count == 3U &&
        snapshot.unchanged_lock_count == 1U &&
        snapshot.partial_extent_lock_count == 1U &&
        snapshot.full_extent_lock_count == 1U &&
        snapshot.all_pixels_changed_lock_count == 1U &&
        snapshot.malformed_baseline_count == 0U &&
        snapshot.changed_pixel_count == 9U &&
        snapshot.max_changed_pixel_count == 8U &&
        snapshot.max_bounding_box_pixel_count == 8U;

    GlideLfbWriteFootprintProfile malformed_profile;
    BeginGlideLfbWriteFootprint(&malformed_profile, nullptr, 0U, 4U, 2U);
    const bool malformed =
        SnapshotGlideLfbWriteFootprint(malformed_profile).malformed_baseline_count == 1U;

    const bool all = policy && aggregation && malformed;
    std::cout << "glide_lfb_write_footprint_policy=" << (policy ? "true" : "false")
              << "\nglide_lfb_write_footprint_aggregation="
              << (aggregation ? "true" : "false")
              << "\nglide_lfb_write_footprint_malformed="
              << (malformed ? "true" : "false")
              << "\nglide_lfb_write_footprint_all=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
