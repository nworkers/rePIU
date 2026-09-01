#include "repiu/runtime/aot_segment_patch.h"

#include <cstring>

namespace repiu::runtime
{

std::uint32_t PatchAotSegmentOverrideSites(
    std::uint8_t* const bytes,
    const std::vector<AotSegmentOverrideSite>& sites,
    const AotSegmentTable& table,
    AotSegmentOverridePatchStats* const stats)
{
    if (bytes == nullptr)
    {
        return 0U;
    }
    std::uint32_t processed = 0U;
    for (const AotSegmentOverrideSite& site : sites)
    {
        const std::uint8_t seg = site.segment_register;
        if (seg >= 6U)
        {
            continue;
        }
        const AotSegmentResolution& resolution = table.segments[seg];
        ++processed;
        if (resolution.shadow_address == 0U)
        {
            bytes[site.cache_offset] = 0xCCU;
            if (stats != nullptr)
            {
                ++stats->unresolved_site_count;
            }
            continue;
        }
        if (resolution.policy == AotSegmentAccessPolicy::kHleLowMemory)
        {
            if (site.dispatch_cache_offset == 0U)
            {
                bytes[site.cache_offset] = 0xCCU;
            }
            else
            {
                bytes[site.cache_offset] = 0xE9U;
                const std::int32_t relative = static_cast<std::int32_t>(
                    site.dispatch_cache_offset - (site.cache_offset + 5U));
                std::memcpy(bytes + site.cache_offset + 1U,
                            &relative, sizeof(relative));
            }
            if (stats != nullptr)
            {
                ++stats->hle_site_count;
            }
            continue;
        }
        if (resolution.policy != AotSegmentAccessPolicy::kNativeFolded)
        {
            bytes[site.cache_offset] = 0xCCU;
            if (stats != nullptr)
            {
                ++stats->unresolved_site_count;
            }
            continue;
        }
        // Restore the slot's own opening bytes, because HLE routing overwrites
        // the first five with JMP rel32.
        //
        // Task 568. These come from the site, not from a constant here. i386
        // opens a slot `9C 66 81 3D` and long mode opens it with a lowered
        // `pushfd`; a constant would fit one host and silently corrupt the
        // other. Restoring precedes the operand patches below, so a prologue
        // that overlaps the abs32 field loses to the address written after it.
        std::memcpy(bytes + site.cache_offset, site.guard_prologue,
                    site.guard_prologue_size);
        std::memcpy(bytes + site.guard_address_offset,
                    &resolution.shadow_address, sizeof(std::uint32_t));
        std::memcpy(bytes + site.guard_selector_offset,
                    &resolution.selector, sizeof(std::uint16_t));
        const std::uint32_t displacement =
            static_cast<std::uint32_t>(site.original_displacement) +
            resolution.base;
        std::memcpy(bytes + site.displacement_offset, &displacement,
                    sizeof(displacement));
        if (stats != nullptr)
        {
            ++stats->native_site_count;
        }
    }
    return processed;
}

}  // namespace repiu::runtime
