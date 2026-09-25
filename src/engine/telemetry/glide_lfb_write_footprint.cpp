#include "repiu/engine/glide_lfb_write_footprint.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace repiu::engine
{
namespace
{

bool IsValidSurface(const std::uint8_t* const pixels,
                    const std::size_t byte_count,
                    const std::uint32_t width,
                    const std::uint32_t height)
{
    if (pixels == nullptr || width == 0U || height == 0U)
    {
        return false;
    }
    const std::uint64_t required = static_cast<std::uint64_t>(width) * height * 2U;
    return required <= byte_count &&
        required <= std::numeric_limits<std::size_t>::max();
}

void ClearBaseline(GlideLfbWriteFootprintProfile* const profile)
{
    profile->baseline.clear();
    profile->baseline_active = false;
    profile->baseline_width = 0U;
    profile->baseline_height = 0U;
}

}  // namespace

bool ResolveGlideLfbWriteFootprintEnabled(const std::string_view setting)
{
    return setting == "1" || setting == "on" || setting == "true";
}

bool GlideLfbWriteFootprintEnabled()
{
    static const bool enabled = [] {
        const char* const value = std::getenv("REPIU_GLIDE_LFB_WRITE_CENSUS");
        return value != nullptr && ResolveGlideLfbWriteFootprintEnabled(value);
    }();
    return enabled;
}

void BeginGlideLfbWriteFootprint(GlideLfbWriteFootprintProfile* const profile,
                                 const std::uint8_t* const pixels,
                                 const std::size_t byte_count,
                                 const std::uint32_t width,
                                 const std::uint32_t height)
{
    if (profile == nullptr)
    {
        return;
    }
    profile->enabled = true;
    ++profile->write_lock_count;
    ClearBaseline(profile);
    if (!IsValidSurface(pixels, byte_count, width, height))
    {
        ++profile->malformed_baseline_count;
        return;
    }
    const std::size_t required = static_cast<std::size_t>(width) * height * 2U;
    try
    {
        profile->baseline.assign(pixels, pixels + required);
        profile->baseline_width = width;
        profile->baseline_height = height;
        profile->baseline_active = true;
    }
    catch (...)
    {
        ++profile->malformed_baseline_count;
        ClearBaseline(profile);
    }
}

void CompareGlideLfbWriteFootprint(GlideLfbWriteFootprintProfile* const profile,
                                   const std::uint8_t* const pixels,
                                   const std::size_t byte_count,
                                   const std::uint32_t width,
                                   const std::uint32_t height)
{
    if (profile == nullptr || !profile->baseline_active)
    {
        return;
    }
    if (!IsValidSurface(pixels, byte_count, width, height) ||
        profile->baseline_width != width || profile->baseline_height != height)
    {
        ++profile->malformed_baseline_count;
        ClearBaseline(profile);
        return;
    }
    const std::size_t required = static_cast<std::size_t>(width) * height * 2U;
    if (profile->baseline.size() != required)
    {
        ++profile->malformed_baseline_count;
        ClearBaseline(profile);
        return;
    }

    std::uint64_t changed = 0U;
    std::uint32_t min_x = width;
    std::uint32_t min_y = height;
    std::uint32_t max_x = 0U;
    std::uint32_t max_y = 0U;
    for (std::uint32_t y = 0U; y < height; ++y)
    {
        for (std::uint32_t x = 0U; x < width; ++x)
        {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 2U;
            if (profile->baseline[offset] == pixels[offset] &&
                profile->baseline[offset + 1U] == pixels[offset + 1U])
            {
                continue;
            }
            ++changed;
            min_x = std::min(min_x, x);
            min_y = std::min(min_y, y);
            max_x = std::max(max_x, x);
            max_y = std::max(max_y, y);
        }
    }
    ++profile->compared_lock_count;
    profile->changed_pixel_count += changed;
    profile->max_changed_pixel_count =
        std::max(profile->max_changed_pixel_count, changed);
    if (changed == 0U)
    {
        ++profile->unchanged_lock_count;
        ClearBaseline(profile);
        return;
    }
    const std::uint64_t bounding_box_pixels =
        static_cast<std::uint64_t>(max_x - min_x + 1U) * (max_y - min_y + 1U);
    profile->max_bounding_box_pixel_count =
        std::max(profile->max_bounding_box_pixel_count, bounding_box_pixels);
    if (min_x == 0U && min_y == 0U && max_x + 1U == width && max_y + 1U == height)
    {
        ++profile->full_extent_lock_count;
    }
    else
    {
        ++profile->partial_extent_lock_count;
    }
    if (changed == static_cast<std::uint64_t>(width) * height)
    {
        ++profile->all_pixels_changed_lock_count;
    }
    ClearBaseline(profile);
}

GlideLfbWriteFootprintSnapshot SnapshotGlideLfbWriteFootprint(
    const GlideLfbWriteFootprintProfile& profile)
{
    GlideLfbWriteFootprintSnapshot snapshot;
    snapshot.enabled = profile.enabled;
    snapshot.write_lock_count = profile.write_lock_count;
    snapshot.compared_lock_count = profile.compared_lock_count;
    snapshot.unchanged_lock_count = profile.unchanged_lock_count;
    snapshot.partial_extent_lock_count = profile.partial_extent_lock_count;
    snapshot.full_extent_lock_count = profile.full_extent_lock_count;
    snapshot.all_pixels_changed_lock_count = profile.all_pixels_changed_lock_count;
    snapshot.malformed_baseline_count = profile.malformed_baseline_count;
    snapshot.changed_pixel_count = profile.changed_pixel_count;
    snapshot.max_changed_pixel_count = profile.max_changed_pixel_count;
    snapshot.max_bounding_box_pixel_count = profile.max_bounding_box_pixel_count;
    return snapshot;
}

}  // namespace repiu::engine
