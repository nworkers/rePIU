#include "repiu/engine/aot_ff_boundary_attribution.h"

#include "repiu/engine/aot_boundary_opcode_census.h"

namespace repiu::engine
{
namespace
{

constexpr std::uint8_t kFfOpcode = 0xFFU;

std::uint32_t PackBytes(const std::uint8_t* bytes, std::size_t length)
{
    if (bytes == nullptr)
    {
        return 0U;
    }
    std::uint32_t packed = 0U;
    for (std::size_t index = 0; index < 4U && index < length; ++index)
    {
        packed |= static_cast<std::uint32_t>(bytes[index]) << (index * 8U);
    }
    return packed;
}

bool HasAddressSizePrefix(const std::uint8_t* bytes, std::size_t length)
{
    std::size_t index = 0U;
    std::uint32_t prefix_count = 0U;
    while (index < length && IsX86LegacyPrefix(bytes[index]))
    {
        if (prefix_count >= kAotMaxLegacyPrefixes)
        {
            break;
        }
        if (bytes[index] == 0x67U)
        {
            return true;
        }
        ++prefix_count;
        ++index;
    }
    return false;
}

AotFfAddressingMode ClassifyAddressingMode(std::uint8_t modrm,
                                            bool address_size_16)
{
    const std::uint8_t mod = (modrm >> 6U) & 0x03U;
    const std::uint8_t rm = modrm & 0x07U;
    if (mod == 3U)
    {
        return AotFfAddressingMode::kRegister;
    }
    if (address_size_16)
    {
        return AotFfAddressingMode::kAddress16;
    }
    if (mod == 0U && rm == 5U)
    {
        return AotFfAddressingMode::kAbsolute;
    }
    if (rm == 4U)
    {
        return AotFfAddressingMode::kSib;
    }
    return AotFfAddressingMode::kBase;
}

std::size_t ModeIndex(AotFfAddressingMode mode)
{
    const std::size_t index = static_cast<std::size_t>(mode);
    return index < kAotFfAddressingModeCount
        ? index
        : static_cast<std::size_t>(AotFfAddressingMode::kUnknown);
}

bool IsBetter(const AotFfBoundarySite& candidate,
              const AotFfBoundarySiteHotspot& current)
{
    return candidate.count > current.count ||
        (candidate.count == current.count && candidate.guest_eip != 0U &&
         (current.guest_eip == 0U || candidate.guest_eip < current.guest_eip));
}

void CopySite(const AotFfBoundarySite& site,
              AotFfBoundarySiteHotspot* hotspot)
{
    hotspot->guest_eip = site.guest_eip;
    hotspot->count = site.count;
    hotspot->last_packed_bytes = site.last_packed_bytes;
    hotspot->last_modrm = site.last_modrm;
    hotspot->addressing_mode = site.addressing_mode;
    hotspot->byte_change_count = site.byte_change_count;
    hotspot->mode_change_count = site.mode_change_count;
    hotspot->last_displacement = site.last_displacement;
    hotspot->last_pointer_address = site.last_pointer_address;
    hotspot->last_target = site.last_target;
    hotspot->target_read_count = site.target_read_count;
    hotspot->target_failure_count = site.target_failure_count;
    hotspot->pointer_change_count = site.pointer_change_count;
    hotspot->displacement_change_count = site.displacement_change_count;
    hotspot->target_change_count = site.target_change_count;
    hotspot->target_change_with_same_pointer_count =
        site.target_change_with_same_pointer_count;
    hotspot->target_change_with_pointer_change_count =
        site.target_change_with_pointer_change_count;
    hotspot->last_pointer_address_valid = site.last_pointer_address_valid;
    hotspot->last_index_register = site.last_index_register;
    hotspot->last_index_value = site.last_index_value;
    hotspot->index_value_change_count = site.index_value_change_count;
    hotspot->last_index_value_valid = site.last_index_value_valid;
    hotspot->last_base_register = site.last_base_register;
    hotspot->last_base_value = site.last_base_value;
    hotspot->base_value_change_count = site.base_value_change_count;
    hotspot->last_base_value_valid = site.last_base_value_valid;
    hotspot->index_value_observation_sample_count =
        site.index_value_observation_sample_count;
    hotspot->index_value_observation_slot_count =
        site.index_value_observation_slot_count;
    hotspot->index_value_observation_overflow_count =
        site.index_value_observation_overflow_count;
    for (std::size_t slot = 0U;
         slot < kAotFfBoundaryIndexObservationCapacity;
         ++slot)
    {
        hotspot->index_value_observations[slot] =
            site.index_value_observations[slot];
    }
    hotspot->index_transition_count = site.index_transition_count;
    hotspot->index_transition_slot_count =
        site.index_transition_slot_count;
    hotspot->index_transition_overflow_count =
        site.index_transition_overflow_count;
    for (std::size_t slot = 0U;
         slot < kAotFfBoundaryIndexTransitionCapacity;
         ++slot)
    {
        hotspot->index_transitions[slot] = site.index_transitions[slot];
    }
    hotspot->last_target_valid = site.last_target_valid;
}

}  // namespace

bool DecodeAotFfBoundary(const std::uint8_t* bytes,
                         std::size_t length,
                         AotFfBoundaryDecoded* decoded)
{
    if (decoded == nullptr)
    {
        return false;
    }
    *decoded = AotFfBoundaryDecoded{};
    if (bytes == nullptr || length == 0U)
    {
        return false;
    }

    decoded->packed_bytes = PackBytes(bytes, length);
    std::size_t index = 0U;
    std::uint32_t prefix_count = 0U;
    while (index < length && IsX86LegacyPrefix(bytes[index]))
    {
        if (prefix_count >= kAotMaxLegacyPrefixes)
        {
            break;
        }
        ++prefix_count;
        ++index;
    }
    if (index >= length || bytes[index] != kFfOpcode)
    {
        return false;
    }

    decoded->effective_ff = true;
    if (index + 1U >= length)
    {
        return true;
    }
    decoded->modrm_present = true;
    decoded->modrm = bytes[index + 1U];
    const std::uint8_t group = (decoded->modrm >> 3U) & 0x07U;
    if (group == 4U)
    {
        decoded->addressing_mode = ClassifyAddressingMode(
            decoded->modrm, HasAddressSizePrefix(bytes, length));
    }
    return true;
}

AotFfBoundarySite* FindAotFfBoundarySite(
    AotFfBoundaryAttribution* attribution,
    std::uint32_t guest_eip)
{
    if (attribution == nullptr)
    {
        return nullptr;
    }
    for (std::size_t index = 0U; index < attribution->site_count; ++index)
    {
        if (attribution->sites[index].guest_eip == guest_eip)
        {
            return &attribution->sites[index];
        }
    }
    return nullptr;
}

void RecordAotFfBoundarySample(AotFfBoundaryAttribution* attribution,
                               std::uint32_t guest_eip,
                               const std::uint8_t* bytes,
                               std::size_t length)
{
    if (attribution == nullptr)
    {
        return;
    }
    AotFfBoundaryDecoded decoded;
    if (!DecodeAotFfBoundary(bytes, length, &decoded))
    {
        return;
    }
    if (!decoded.modrm_present)
    {
        ++attribution->modrm_truncated_count;
        return;
    }
    const std::uint8_t group = (decoded.modrm >> 3U) & 0x07U;
    if (group != 4U)
    {
        return;
    }

    ++attribution->sample_count;
    ++attribution->addressing_mode_counts[
        ModeIndex(decoded.addressing_mode)];

    AotFfBoundarySite* site =
        FindAotFfBoundarySite(attribution, guest_eip);
    if (site == nullptr)
    {
        if (attribution->site_count >= kAotFfBoundarySiteCapacity)
        {
            ++attribution->site_overflow_count;
            return;
        }
        site = &attribution->sites[attribution->site_count++];
        site->guest_eip = guest_eip;
        site->last_packed_bytes = decoded.packed_bytes;
        site->last_modrm = decoded.modrm;
        site->addressing_mode = decoded.addressing_mode;
    }

    ++site->count;
    if (site->count > 1U && site->last_packed_bytes != decoded.packed_bytes)
    {
        ++site->byte_change_count;
    }
    if (site->count > 1U && site->addressing_mode != decoded.addressing_mode)
    {
        ++site->mode_change_count;
    }
    site->last_packed_bytes = decoded.packed_bytes;
    site->last_modrm = decoded.modrm;
    site->addressing_mode = decoded.addressing_mode;
}

void RankAotFfBoundarySites(const AotFfBoundaryAttribution& attribution,
                            AotFfBoundarySiteHotspot* hotspots,
                            std::size_t capacity)
{
    if (hotspots == nullptr || capacity == 0U)
    {
        return;
    }
    for (std::size_t slot = 0U; slot < capacity; ++slot)
    {
        hotspots[slot] = AotFfBoundarySiteHotspot{};
    }
    for (std::size_t index = 0U; index < attribution.site_count; ++index)
    {
        const AotFfBoundarySite& candidate = attribution.sites[index];
        for (std::size_t slot = 0U; slot < capacity; ++slot)
        {
            if (!IsBetter(candidate, hotspots[slot]))
            {
                continue;
            }
            for (std::size_t shift = capacity - 1U; shift > slot; --shift)
            {
                hotspots[shift] = hotspots[shift - 1U];
            }
            CopySite(candidate, &hotspots[slot]);
            break;
        }
    }
}

const char* AotFfAddressingModeName(AotFfAddressingMode mode)
{
    switch (mode)
    {
        case AotFfAddressingMode::kRegister:
            return "reg";
        case AotFfAddressingMode::kAbsolute:
            return "abs";
        case AotFfAddressingMode::kBase:
            return "base";
        case AotFfAddressingMode::kSib:
            return "sib";
        case AotFfAddressingMode::kAddress16:
            return "addr16";
        case AotFfAddressingMode::kUnknown:
        case AotFfAddressingMode::kCount:
            return "unknown";
    }
    return "unknown";
}

}  // namespace repiu::engine
