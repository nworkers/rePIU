#pragma once

#include "repiu/engine/aot_ff_target_timing.h"

#include <cstddef>
#include <cstdint>

namespace repiu::engine
{

constexpr std::size_t kAotFfBoundarySiteCapacity = 32U;
constexpr std::size_t kAotFfBoundarySiteHotspotCapacity = 8U;
constexpr std::size_t kAotFfBoundaryIndexObservationCapacity = 8U;
constexpr std::size_t kAotFfBoundaryIndexTransitionCapacity = 32U;

enum class AotFfAddressingMode : std::uint8_t
{
    kRegister = 0,
    kAbsolute,
    kBase,
    kSib,
    kAddress16,
    kUnknown,
    kCount,
};

constexpr std::size_t kAotFfAddressingModeCount =
    static_cast<std::size_t>(AotFfAddressingMode::kCount);

struct AotFfBoundaryDecoded
{
    bool effective_ff = false;
    bool modrm_present = false;
    std::uint8_t modrm = 0;
    AotFfAddressingMode addressing_mode = AotFfAddressingMode::kUnknown;
    std::uint32_t packed_bytes = 0;
};

struct AotFfBoundaryIndexObservation
{
    bool valid = false;
    std::uint8_t register_number = 0;
    std::uint32_t value = 0;
    std::uint32_t count = 0;
};

struct AotFfBoundaryIndexTransition
{
    bool valid = false;
    std::uint8_t from_register = 0;
    std::uint32_t from_value = 0;
    std::uint8_t to_register = 0;
    std::uint32_t to_value = 0;
};

struct AotFfBoundarySite
{
    std::uint32_t guest_eip = 0;
    std::uint32_t count = 0;
    std::uint32_t last_packed_bytes = 0;
    std::uint8_t last_modrm = 0;
    AotFfAddressingMode addressing_mode = AotFfAddressingMode::kUnknown;
    std::uint32_t byte_change_count = 0;
    std::uint32_t mode_change_count = 0;
    std::uint32_t last_displacement = 0;
    std::uint32_t last_pointer_address = 0;
    std::uint32_t last_target = 0;
    std::uint32_t target_read_count = 0;
    std::uint32_t target_failure_count = 0;
    std::uint32_t pointer_change_count = 0;
    std::uint32_t displacement_change_count = 0;
    std::uint32_t target_change_count = 0;
    std::uint32_t target_change_with_same_pointer_count = 0;
    std::uint32_t target_change_with_pointer_change_count = 0;
    bool last_pointer_address_valid = false;
    std::uint8_t last_index_register = 0;
    std::uint32_t last_index_value = 0;
    std::uint32_t index_value_change_count = 0;
    bool last_index_value_valid = false;
    std::uint8_t last_base_register = 0;
    std::uint32_t last_base_value = 0;
    std::uint32_t base_value_change_count = 0;
    bool last_base_value_valid = false;
    std::uint32_t index_value_observation_sample_count = 0;
    std::uint32_t index_value_observation_slot_count = 0;
    std::uint32_t index_value_observation_overflow_count = 0;
    AotFfBoundaryIndexObservation index_value_observations[
        kAotFfBoundaryIndexObservationCapacity] = {};
    std::uint32_t index_transition_count = 0;
    std::uint32_t index_transition_slot_count = 0;
    std::uint32_t index_transition_overflow_count = 0;
    AotFfBoundaryIndexTransition index_transitions[
        kAotFfBoundaryIndexTransitionCapacity] = {};
    bool last_target_valid = false;
};

struct AotFfBoundaryAttribution
{
    std::uint32_t sample_count = 0;
    std::uint32_t modrm_truncated_count = 0;
    std::uint32_t addressing_mode_counts[kAotFfAddressingModeCount] = {};
    std::uint32_t site_count = 0;
    std::uint32_t site_overflow_count = 0;
    std::uint32_t target_resolved_count = 0;
    std::uint32_t target_unresolved_count = 0;
    std::uint32_t target_instruction_truncated_count = 0;
    std::uint32_t target_unsupported_count = 0;
    std::uint32_t target_memory_unreadable_count = 0;
    AotFfTargetTimingProfile target_timing;
    AotFfBoundarySite sites[kAotFfBoundarySiteCapacity] = {};
};

struct AotFfBoundarySiteHotspot
{
    std::uint32_t guest_eip = 0;
    std::uint32_t count = 0;
    std::uint32_t last_packed_bytes = 0;
    std::uint8_t last_modrm = 0;
    AotFfAddressingMode addressing_mode = AotFfAddressingMode::kUnknown;
    std::uint32_t byte_change_count = 0;
    std::uint32_t mode_change_count = 0;
    std::uint32_t last_displacement = 0;
    std::uint32_t last_pointer_address = 0;
    std::uint32_t last_target = 0;
    std::uint32_t target_read_count = 0;
    std::uint32_t target_failure_count = 0;
    std::uint32_t pointer_change_count = 0;
    std::uint32_t displacement_change_count = 0;
    std::uint32_t target_change_count = 0;
    std::uint32_t target_change_with_same_pointer_count = 0;
    std::uint32_t target_change_with_pointer_change_count = 0;
    bool last_pointer_address_valid = false;
    std::uint8_t last_index_register = 0;
    std::uint32_t last_index_value = 0;
    std::uint32_t index_value_change_count = 0;
    bool last_index_value_valid = false;
    std::uint8_t last_base_register = 0;
    std::uint32_t last_base_value = 0;
    std::uint32_t base_value_change_count = 0;
    bool last_base_value_valid = false;
    std::uint32_t index_value_observation_sample_count = 0;
    std::uint32_t index_value_observation_slot_count = 0;
    std::uint32_t index_value_observation_overflow_count = 0;
    AotFfBoundaryIndexObservation index_value_observations[
        kAotFfBoundaryIndexObservationCapacity] = {};
    std::uint32_t index_transition_count = 0;
    std::uint32_t index_transition_slot_count = 0;
    std::uint32_t index_transition_overflow_count = 0;
    AotFfBoundaryIndexTransition index_transitions[
        kAotFfBoundaryIndexTransitionCapacity] = {};
    bool last_target_valid = false;
};

bool DecodeAotFfBoundary(const std::uint8_t* bytes,
                         std::size_t length,
                         AotFfBoundaryDecoded* decoded);

AotFfBoundarySite* FindAotFfBoundarySite(
    AotFfBoundaryAttribution* attribution,
    std::uint32_t guest_eip);

void RecordAotFfBoundarySample(AotFfBoundaryAttribution* attribution,
                               std::uint32_t guest_eip,
                               const std::uint8_t* bytes,
                               std::size_t length);

void RankAotFfBoundarySites(const AotFfBoundaryAttribution& attribution,
                            AotFfBoundarySiteHotspot* hotspots,
                            std::size_t capacity);

const char* AotFfAddressingModeName(AotFfAddressingMode mode);

}  // namespace repiu::engine
