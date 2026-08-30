#include "repiu/engine/aot_ff_boundary_target_attribution.h"

#include "../cpu_emul/guest_memory_access.h"
#include "../cpu_emul/instruction_emulation.h"
#include "repiu/engine/aot_boundary_opcode_census.h"

#include <cstddef>
#include <cstdint>

namespace repiu::engine
{
namespace
{

constexpr std::uint8_t kFfOpcode = 0xFFU;
constexpr std::uint8_t kCsSegmentOverride = 0x2EU;
constexpr std::size_t kMaxObservedInstructionBytes = 15U;

bool ReadUInt32(const std::uint8_t* bytes,
                std::size_t length,
                std::size_t offset,
                std::uint32_t* value)
{
    if (value == nullptr || offset > length || length - offset < 4U)
    {
        return false;
    }
    *value = static_cast<std::uint32_t>(bytes[offset]) |
        (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U) |
        (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U) |
        (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
    return true;
}

AotFfTargetStatus Truncated(AotFfBoundaryTargetDecoded* decoded)
{
    decoded->status = AotFfTargetStatus::kInstructionTruncated;
    return decoded->status;
}

void RecordFailure(AotFfBoundaryAttribution* attribution,
                   AotFfBoundarySite* site,
                   AotFfTargetStatus status)
{
    ++attribution->target_unresolved_count;
    ++site->target_failure_count;
    switch (status)
    {
        case AotFfTargetStatus::kInstructionTruncated:
            ++attribution->target_instruction_truncated_count;
            break;
        case AotFfTargetStatus::kUnsupportedAddressSize:
        case AotFfTargetStatus::kUnsupportedSegment:
        case AotFfTargetStatus::kMissingContext:
        case AotFfTargetStatus::kNotFf4:
            ++attribution->target_unsupported_count;
            break;
        case AotFfTargetStatus::kMemoryUnreadable:
            ++attribution->target_memory_unreadable_count;
            break;
        case AotFfTargetStatus::kResolved:
        case AotFfTargetStatus::kCount:
            ++attribution->target_unsupported_count;
            break;
    }
}

void RecordIndexValueObservation(
    AotFfBoundarySite* site,
    const AotFfBoundaryTargetResult& result)
{
    if (site == nullptr || !result.index_value_valid)
    {
        return;
    }
    ++site->index_value_observation_sample_count;
    for (std::size_t slot = 0U;
         slot < kAotFfBoundaryIndexObservationCapacity;
         ++slot)
    {
        AotFfBoundaryIndexObservation& observation =
            site->index_value_observations[slot];
        if (observation.valid &&
            observation.register_number == result.index_register &&
            observation.value == result.index_value)
        {
            ++observation.count;
            return;
        }
    }
    for (std::size_t slot = 0U;
         slot < kAotFfBoundaryIndexObservationCapacity;
         ++slot)
    {
        AotFfBoundaryIndexObservation& observation =
            site->index_value_observations[slot];
        if (!observation.valid)
        {
            observation.valid = true;
            observation.register_number = result.index_register;
            observation.value = result.index_value;
            observation.count = 1U;
            ++site->index_value_observation_slot_count;
            return;
        }
    }
    ++site->index_value_observation_overflow_count;
}

void RecordIndexTransition(
    AotFfBoundarySite* site,
    const AotFfBoundaryTargetResult& result)
{
    if (site == nullptr || !result.index_value_valid ||
        !site->last_index_value_valid)
    {
        return;
    }
    ++site->index_transition_count;
    for (std::size_t slot = 0U;
         slot < kAotFfBoundaryIndexTransitionCapacity;
         ++slot)
    {
        AotFfBoundaryIndexTransition& transition =
            site->index_transitions[slot];
        if (!transition.valid)
        {
            transition.valid = true;
            transition.from_register = site->last_index_register;
            transition.from_value = site->last_index_value;
            transition.to_register = result.index_register;
            transition.to_value = result.index_value;
            ++site->index_transition_slot_count;
            return;
        }
    }
    ++site->index_transition_overflow_count;
}

}  // namespace

AotFfTargetStatus DecodeAotFfBoundaryTarget(
    const std::uint8_t* bytes,
    std::size_t length,
    AotFfBoundaryTargetDecoded* decoded)
{
    if (decoded == nullptr)
    {
        return AotFfTargetStatus::kNotFf4;
    }
    *decoded = AotFfBoundaryTargetDecoded{};
    if (bytes == nullptr || length == 0U)
    {
        return decoded->status;
    }

    std::size_t index = 0U;
    std::uint32_t prefix_count = 0U;
    while (index < length && IsX86LegacyPrefix(bytes[index]))
    {
        if (prefix_count >= kAotMaxLegacyPrefixes)
        {
            break;
        }
        switch (bytes[index])
        {
            case 0x2EU:
            case 0x26U:
            case 0x36U:
            case 0x3EU:
            case 0x64U:
            case 0x65U:
                decoded->has_segment_override = true;
                decoded->segment_override = bytes[index];
                break;
            case 0x67U:
                decoded->status =
                    AotFfTargetStatus::kUnsupportedAddressSize;
                break;
            default:
                break;
        }
        ++prefix_count;
        ++index;
    }
    if (index >= length || bytes[index] != kFfOpcode)
    {
        decoded->status = AotFfTargetStatus::kNotFf4;
        return decoded->status;
    }
    if (index + 1U >= length)
    {
        return Truncated(decoded);
    }

    const std::uint8_t modrm = bytes[index + 1U];
    const std::uint8_t group = (modrm >> 3U) & 0x07U;
    if (group != 4U)
    {
        decoded->status = AotFfTargetStatus::kNotFf4;
        return decoded->status;
    }
    if (decoded->status == AotFfTargetStatus::kUnsupportedAddressSize)
    {
        return decoded->status;
    }
    if (decoded->has_segment_override &&
        decoded->segment_override != kCsSegmentOverride)
    {
        decoded->status = AotFfTargetStatus::kUnsupportedSegment;
        return decoded->status;
    }

    const std::uint8_t mod = (modrm >> 6U) & 0x03U;
    const std::uint8_t rm = modrm & 0x07U;
    decoded->target_register = rm;
    if (mod == 3U)
    {
        decoded->register_form = true;
        decoded->instruction_size =
            static_cast<std::uint32_t>(index + 2U);
        decoded->status = AotFfTargetStatus::kResolved;
        return decoded->status;
    }

    std::size_t cursor = index + 2U;
    if (rm == 4U)
    {
        if (cursor >= length)
        {
            return Truncated(decoded);
        }
        const std::uint8_t sib = bytes[cursor++];
        const std::uint8_t index_register = (sib >> 3U) & 0x07U;
        const std::uint8_t base_register = sib & 0x07U;
        decoded->scale = static_cast<std::uint8_t>(
            1U << ((sib >> 6U) & 0x03U));
        decoded->has_index = index_register != 4U;
        decoded->index_register = index_register;
        decoded->has_base = !(mod == 0U && base_register == 5U);
        decoded->base_register = base_register;
    }
    else
    {
        decoded->has_base = !(mod == 0U && rm == 5U);
        decoded->base_register = rm;
    }

    if (mod == 0U)
    {
        const bool absolute =
            (rm == 5U) || (rm == 4U && !decoded->has_base);
        if (absolute)
        {
            if (!ReadUInt32(bytes, length, cursor,
                            &decoded->displacement))
            {
                return Truncated(decoded);
            }
            decoded->has_displacement = true;
            cursor += 4U;
        }
    }
    else if (mod == 1U)
    {
        if (cursor >= length)
        {
            return Truncated(decoded);
        }
        const auto signed_displacement =
            static_cast<std::int32_t>(static_cast<std::int8_t>(
                bytes[cursor++]));
        decoded->displacement =
            static_cast<std::uint32_t>(signed_displacement);
        decoded->has_displacement = true;
    }
    else
    {
        if (!ReadUInt32(bytes, length, cursor,
                        &decoded->displacement))
        {
            return Truncated(decoded);
        }
        decoded->has_displacement = true;
        cursor += 4U;
    }

    decoded->instruction_size = static_cast<std::uint32_t>(cursor);
    decoded->status = AotFfTargetStatus::kResolved;
    return decoded->status;
}

AotFfTargetStatus ResolveAotFfBoundaryTarget(
    const AotFfBoundaryTargetDecoded& decoded,
    ThreadContext* context,
    const repiu::platform::GuestCpuContext* win32_context,
    AotFfBoundaryTargetResult* result)
{
    if (result == nullptr)
    {
        return AotFfTargetStatus::kMissingContext;
    }
    *result = AotFfBoundaryTargetResult{};
    result->status = decoded.status;
    if (decoded.status != AotFfTargetStatus::kResolved)
    {
        return result->status;
    }
    if (context == nullptr || win32_context == nullptr)
    {
        result->status = AotFfTargetStatus::kMissingContext;
        return result->status;
    }

    if (decoded.register_form)
    {
        result->target = ReadGeneralRegister32(
            win32_context, decoded.target_register);
        result->target_valid = true;
        result->status = AotFfTargetStatus::kResolved;
        return result->status;
    }

    std::uint32_t pointer_address = decoded.displacement;
    if (decoded.has_base)
    {
        result->base_value_valid = true;
        result->base_register = decoded.base_register;
        result->base_value = ReadGeneralRegister32(
            win32_context, decoded.base_register);
        pointer_address += result->base_value;
    }
    if (decoded.has_index)
    {
        result->index_value_valid = true;
        result->index_register = decoded.index_register;
        result->index_value = ReadGeneralRegister32(
            win32_context, decoded.index_register);
        pointer_address += result->index_value * decoded.scale;
    }
    result->pointer_address = pointer_address;
    result->pointer_address_valid = true;
    if (!ReadGuestUInt32(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(pointer_address)),
            &result->target))
    {
        result->status = AotFfTargetStatus::kMemoryUnreadable;
        return result->status;
    }
    result->target_valid = true;
    result->status = AotFfTargetStatus::kResolved;
    return result->status;
}

void RecordAotFfBoundaryTargetSample(
    AotFfBoundaryAttribution* attribution,
    ThreadContext* context,
    const repiu::platform::GuestCpuContext* win32_context,
    std::uint32_t guest_eip,
    const std::uint8_t* boundary_bytes,
    std::size_t boundary_length)
{
    if (attribution != nullptr)
    {
        ClearAotFfTargetTimingCandidate(&attribution->target_timing);
    }
    AotFfBoundaryDecoded boundary_decoded;
    if (!DecodeAotFfBoundary(
            boundary_bytes, boundary_length, &boundary_decoded) ||
        !boundary_decoded.modrm_present ||
        ((boundary_decoded.modrm >> 3U) & 0x07U) != 4U)
    {
        return;
    }
    AotFfBoundarySite* site =
        FindAotFfBoundarySite(attribution, guest_eip);
    if (attribution == nullptr || site == nullptr)
    {
        return;
    }

    AotFfBoundaryTargetDecoded decoded;
    AotFfBoundaryTargetResult result;
    AotFfTargetStatus status = AotFfTargetStatus::kMissingContext;
    if (context != nullptr && win32_context != nullptr)
    {
        const auto* instruction = reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(guest_eip));
        std::size_t readable = 0U;
        for (std::size_t candidate = kMaxObservedInstructionBytes;
             candidate > 0U; --candidate)
        {
            if (IsGuestRangeReadable(
                    context, instruction,
                    static_cast<std::uint32_t>(candidate)))
            {
                readable = candidate;
                break;
            }
        }
        status = DecodeAotFfBoundaryTarget(
            instruction, readable, &decoded);
        status = ResolveAotFfBoundaryTarget(
            decoded, context, win32_context, &result);
    }

    if (status != AotFfTargetStatus::kResolved || !result.target_valid)
    {
        RecordFailure(attribution, site, status);
        return;
    }

    ++attribution->target_resolved_count;
    const bool pointer_valid = result.pointer_address_valid;
    const bool pointer_changed = pointer_valid &&
        site->last_pointer_address_valid &&
        site->last_pointer_address != result.pointer_address;
    if (pointer_changed)
    {
        ++site->pointer_change_count;
    }
    const bool index_changed = result.index_value_valid &&
        site->last_index_value_valid &&
        (site->last_index_register != result.index_register ||
         site->last_index_value != result.index_value);
    if (index_changed)
    {
        ++site->index_value_change_count;
        RecordIndexTransition(site, result);
    }
    const bool base_changed = result.base_value_valid &&
        site->last_base_value_valid &&
        (site->last_base_register != result.base_register ||
         site->last_base_value != result.base_value);
    if (base_changed)
    {
        ++site->base_value_change_count;
    }
    RecordIndexValueObservation(site, result);
    if (site->target_read_count > 0U &&
        site->last_displacement != decoded.displacement)
    {
        ++site->displacement_change_count;
    }
    if (site->last_target_valid && site->last_target != result.target)
    {
        ++site->target_change_count;
        if (pointer_valid && site->last_pointer_address_valid)
        {
            if (pointer_changed)
            {
                ++site->target_change_with_pointer_change_count;
            }
            else
            {
                ++site->target_change_with_same_pointer_count;
            }
        }
    }
    ++site->target_read_count;
    site->last_displacement = decoded.displacement;
    site->last_pointer_address = pointer_valid
        ? result.pointer_address
        : 0U;
    site->last_pointer_address_valid = pointer_valid;
    site->last_index_register = result.index_value_valid
        ? result.index_register
        : 0U;
    site->last_index_value = result.index_value_valid
        ? result.index_value
        : 0U;
    site->last_index_value_valid = result.index_value_valid;
    site->last_base_register = result.base_value_valid
        ? result.base_register
        : 0U;
    site->last_base_value = result.base_value_valid
        ? result.base_value
        : 0U;
    site->last_base_value_valid = result.base_value_valid;
    site->last_target = result.target;
    site->last_target_valid = true;
    if (AotFfTargetTimingEnabled())
    {
        SetAotFfTargetTimingCandidate(
            &attribution->target_timing,
            guest_eip,
            result.target,
            result.index_value_valid,
            result.index_register,
            result.index_value);
    }
}

const char* AotFfTargetStatusName(AotFfTargetStatus status)
{
    switch (status)
    {
        case AotFfTargetStatus::kNotFf4:
            return "not-ff4";
        case AotFfTargetStatus::kResolved:
            return "resolved";
        case AotFfTargetStatus::kInstructionTruncated:
            return "instruction-truncated";
        case AotFfTargetStatus::kUnsupportedAddressSize:
            return "unsupported-address-size";
        case AotFfTargetStatus::kUnsupportedSegment:
            return "unsupported-segment";
        case AotFfTargetStatus::kMissingContext:
            return "missing-context";
        case AotFfTargetStatus::kMemoryUnreadable:
            return "memory-unreadable";
        case AotFfTargetStatus::kCount:
            return "unknown";
    }
    return "unknown";
}

}  // namespace repiu::engine
