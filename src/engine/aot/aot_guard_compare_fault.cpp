#include "aot_guard_compare_fault.h"

#include <cstring>

namespace repiu::engine
{
namespace
{

// The abs32 operand the slot was patched with, read back out of the placed
// cache. This is the number the diagnosis turns on: a guard that faults is a
// guard whose shadow address is wrong, and the wrong address is the evidence.
std::uint32_t ReadPatchedShadowAddress(const AotCodeCachePlacement& placement,
                                       const std::uint32_t operand_offset)
{
    if (operand_offset + sizeof(std::uint32_t) > placement.size)
    {
        return 0U;
    }
    const auto* const bytes = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(placement.base_address));
    std::uint32_t value = 0;
    std::memcpy(&value, bytes + operand_offset, sizeof(value));
    return value;
}

// The compare spans `(cache_offset, operand_offset + operand_bytes)`. The open
// lower bound is deliberate: `cache_offset` itself is the slot's first byte,
// which is either the start of `flags_save` or the INT3 that HLE routing wrote
// over it. A fault there is a boundary, not a broken guard.
bool InCompareWindow(const std::uint32_t offset,
                     const std::uint32_t cache_offset,
                     const std::uint32_t operand_offset,
                     const std::uint32_t operand_bytes)
{
    return offset > cache_offset && offset < operand_offset + operand_bytes;
}

}  // namespace

const char* AotGuardSlotKindName(const AotGuardSlotKind kind)
{
    switch (kind)
    {
        case AotGuardSlotKind::kSegmentOverride: return "segment-override";
        case AotGuardSlotKind::kGuardedLoad: return "guarded-load";
        case AotGuardSlotKind::kGuardedPop: return "guarded-pop";
        case AotGuardSlotKind::kGuardedRead: return "guarded-read";
    }
    return "unknown";
}

bool FindAotGuardCompareFault(const AotCodeCachePlacement& placement,
                              const std::uint32_t cache_address,
                              AotGuardCompareFault* const fault)
{
    if (fault == nullptr || !placement.placed ||
        cache_address < placement.base_address ||
        cache_address - placement.base_address >= placement.size)
    {
        return false;
    }
    const std::uint32_t offset = cache_address - placement.base_address;

    for (const runtime::AotSegmentOverrideSite& site :
         placement.segment_override_sites)
    {
        // The selector immediate follows the abs32, so its two bytes are the
        // end of the compare.
        if (InCompareWindow(offset, site.cache_offset,
                            site.guard_selector_offset, 2U))
        {
            fault->kind = AotGuardSlotKind::kSegmentOverride;
            fault->guest_source = site.guest_source;
            fault->cache_offset = site.cache_offset;
            fault->segment_register = site.segment_register;
            fault->shadow_address =
                ReadPatchedShadowAddress(placement, site.guard_address_offset);
            return true;
        }
    }
    for (const runtime::AotGuardedSegmentLoadSite& site :
         placement.guarded_segment_load_sites)
    {
        if (InCompareWindow(offset, site.cache_offset,
                            site.shadow_address_offset, 4U))
        {
            fault->kind = AotGuardSlotKind::kGuardedLoad;
            fault->guest_source = site.guest_source;
            fault->cache_offset = site.cache_offset;
            fault->segment_register = site.segment_register;
            fault->shadow_address =
                ReadPatchedShadowAddress(placement, site.shadow_address_offset);
            return true;
        }
    }
    for (const runtime::AotGuardedSegmentPopSite& site :
         placement.guarded_segment_pop_sites)
    {
        if (InCompareWindow(offset, site.cache_offset,
                            site.shadow_address_offset, 4U))
        {
            fault->kind = AotGuardSlotKind::kGuardedPop;
            fault->guest_source = site.guest_source;
            fault->cache_offset = site.cache_offset;
            fault->segment_register = site.segment_register;
            fault->shadow_address =
                ReadPatchedShadowAddress(placement, site.shadow_address_offset);
            return true;
        }
    }
    for (const runtime::AotGuardedSegmentReadSite& site :
         placement.guarded_segment_read_sites)
    {
        if (InCompareWindow(offset, site.cache_offset,
                            site.shadow_address_offset, 4U))
        {
            fault->kind = AotGuardSlotKind::kGuardedRead;
            fault->guest_source = site.guest_source;
            fault->cache_offset = site.cache_offset;
            fault->segment_register = site.segment_register;
            fault->shadow_address =
                ReadPatchedShadowAddress(placement, site.shadow_address_offset);
            return true;
        }
    }
    return false;
}

}  // namespace repiu::engine
