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
            bytes[site.cache_offset] = 0xCCU;
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

std::uint32_t PatchAotGuardedSegmentLoadSites(
    std::uint8_t* const bytes,
    const std::vector<AotGuardedSegmentLoadSite>& sites,
    const AotSegmentTable& table,
    const std::uint32_t success_counter_address,
    const std::uint32_t fallback_counter_address,
    AotGuardedSegmentLoadPatchStats* const stats)
{
    if (bytes == nullptr)
    {
        return 0U;
    }
    std::uint32_t processed = 0U;
    for (const AotGuardedSegmentLoadSite& site : sites)
    {
        ++processed;
        const bool counters_ready = !site.has_counter_operands ||
            (success_counter_address != 0U &&
             fallback_counter_address != 0U);
        if (site.segment_register >= 6U ||
            table.segments[site.segment_register].shadow_address == 0U ||
            site.guard_prologue_size == 0U || !counters_ready)
        {
            bytes[site.cache_offset] = 0xCCU;
            if (stats != nullptr)
            {
                ++stats->unresolved_site_count;
            }
            continue;
        }

        std::memcpy(bytes + site.cache_offset, site.guard_prologue,
                    site.guard_prologue_size);
        const AotSegmentResolution& resolution =
            table.segments[site.segment_register];
        const std::uint32_t shadow_address = resolution.shadow_address;
        std::memcpy(bytes + site.shadow_address_offset, &shadow_address,
                    sizeof(shadow_address));
        // Task i018. Slots emitted with the accepted-pair form take the pair
        // word addresses; without a shadow block the pair compares point at
        // the shadow word itself, so only no-op reloads pass, which is the
        // old guard's acceptance.
        if (site.pair0_address_offset != 0U)
        {
            const std::uint32_t pair0_address =
                resolution.pair0_address != 0U
                    ? resolution.pair0_address : shadow_address;
            const std::uint32_t pair1_address =
                resolution.pair1_address != 0U
                    ? resolution.pair1_address : shadow_address;
            std::memcpy(bytes + site.pair0_address_offset, &pair0_address,
                        sizeof(pair0_address));
            std::memcpy(bytes + site.pair1_address_offset, &pair1_address,
                        sizeof(pair1_address));
            std::memcpy(bytes + site.shadow_store_offset, &shadow_address,
                        sizeof(shadow_address));
        }
        if (site.has_counter_operands)
        {
            std::memcpy(bytes + site.success_counter_address_offset,
                        &success_counter_address,
                        sizeof(success_counter_address));
            std::memcpy(bytes + site.fallback_counter_address_offset,
                        &fallback_counter_address,
                        sizeof(fallback_counter_address));
        }
        if (stats != nullptr)
        {
            ++stats->native_site_count;
        }
    }
    return processed;
}

std::uint32_t PatchAotGuardedSegmentPopSites(
    std::uint8_t* const bytes,
    const std::vector<AotGuardedSegmentPopSite>& sites,
    const AotSegmentTable& table,
    const std::uint32_t success_counter_address,
    const std::uint32_t fallback_counter_address,
    AotGuardedSegmentPopPatchStats* const stats)
{
    if (bytes == nullptr)
    {
        return 0U;
    }
    std::uint32_t processed = 0U;
    for (const AotGuardedSegmentPopSite& site : sites)
    {
        ++processed;
        const bool counters_ready = !site.has_counter_operands ||
            (success_counter_address != 0U &&
             fallback_counter_address != 0U);
        if (site.segment_register >= 6U ||
            table.segments[site.segment_register].shadow_address == 0U ||
            site.guard_prologue_size == 0U || !counters_ready)
        {
            bytes[site.cache_offset] = 0xCCU;
            if (stats != nullptr)
            {
                ++stats->unresolved_site_count;
            }
            continue;
        }

        std::memcpy(bytes + site.cache_offset, site.guard_prologue,
                    site.guard_prologue_size);
        const std::uint32_t shadow_address =
            table.segments[site.segment_register].shadow_address;
        std::memcpy(bytes + site.shadow_address_offset, &shadow_address,
                    sizeof(shadow_address));
        // Task i018. The accepted-pair pop slot takes the pair word addresses
        // on the load patcher's rule: without a shadow block the pair compares
        // point at the shadow word itself, so only no-op reloads pass.
        if (site.pair0_address_offset != 0U)
        {
            const AotSegmentResolution& resolution =
                table.segments[site.segment_register];
            const std::uint32_t pair0_address =
                resolution.pair0_address != 0U
                    ? resolution.pair0_address : shadow_address;
            const std::uint32_t pair1_address =
                resolution.pair1_address != 0U
                    ? resolution.pair1_address : shadow_address;
            std::memcpy(bytes + site.pair0_address_offset, &pair0_address,
                        sizeof(pair0_address));
            std::memcpy(bytes + site.pair1_address_offset, &pair1_address,
                        sizeof(pair1_address));
            std::memcpy(bytes + site.shadow_store_offset, &shadow_address,
                        sizeof(shadow_address));
        }
        if (site.has_counter_operands)
        {
            std::memcpy(bytes + site.success_counter_address_offset,
                        &success_counter_address,
                        sizeof(success_counter_address));
            std::memcpy(bytes + site.fallback_counter_address_offset,
                        &fallback_counter_address,
                        sizeof(fallback_counter_address));
        }
        if (stats != nullptr)
        {
            ++stats->native_site_count;
        }
    }
    return processed;
}

std::uint32_t PatchAotGuardedSegmentReadSites(
    std::uint8_t* const bytes,
    const std::vector<AotGuardedSegmentReadSite>& sites,
    const AotSegmentTable& table,
    AotGuardedSegmentReadPatchStats* const stats)
{
    if (bytes == nullptr)
    {
        return 0U;
    }
    std::uint32_t processed = 0U;
    for (const AotGuardedSegmentReadSite& site : sites)
    {
        ++processed;
        if (site.segment_register >= 6U ||
            table.segments[site.segment_register].shadow_address == 0U ||
            site.guard_prologue_size == 0U)
        {
            bytes[site.cache_offset] = 0xCCU;
            if (stats != nullptr)
            {
                ++stats->unresolved_site_count;
            }
            continue;
        }

        std::memcpy(bytes + site.cache_offset, site.guard_prologue,
                    site.guard_prologue_size);
        const std::uint32_t shadow_address =
            table.segments[site.segment_register].shadow_address;
        std::memcpy(bytes + site.shadow_address_offset, &shadow_address,
                    sizeof(shadow_address));
        std::memcpy(bytes + site.load_shadow_address_offset, &shadow_address,
                    sizeof(shadow_address));
        if (stats != nullptr)
        {
            ++stats->native_site_count;
        }
    }
    return processed;
}

void ApplyFlatStackSegmentFold(const std::uint16_t flat_stack_selector,
                               AotSegmentResolution* const ss_resolution)
{
    if (ss_resolution == nullptr || flat_stack_selector == 0U ||
        ss_resolution->selector != flat_stack_selector ||
        ss_resolution->policy != AotSegmentAccessPolicy::kNativeFolded)
    {
        return;
    }
    // Only the base the fold adds changes. The limit is not used by the fold,
    // and the descriptor in the selector table is left as it is because the DOS
    // allocator and the mode16 paths read that base for their own purposes.
    ss_resolution->base = 0U;
}

void ApplyFlatSegmentFolds(const std::uint16_t flat_stack_selector,
                           const std::uint16_t flat_data_selector,
                           AotSegmentTable* const table)
{
    if (table == nullptr)
    {
        return;
    }
    for (std::uint8_t seg = 0U; seg < 6U; ++seg)
    {
        if (seg == 1U)
        {
            continue;  // CS has no shadow
        }
        ApplyFlatStackSegmentFold(flat_stack_selector, &table->segments[seg]);
        ApplyFlatStackSegmentFold(flat_data_selector, &table->segments[seg]);
    }
}

}  // namespace repiu::runtime

