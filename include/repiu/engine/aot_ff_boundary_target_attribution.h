#pragma once

#include "repiu/engine/aot_ff_boundary_attribution.h"
#include "repiu/platform/guest_cpu_context.h"

#include <cstddef>
#include <cstdint>

namespace repiu::engine
{

struct ThreadContext;

enum class AotFfTargetStatus : std::uint8_t
{
    kNotFf4 = 0,
    kResolved,
    kInstructionTruncated,
    kUnsupportedAddressSize,
    kUnsupportedSegment,
    kMissingContext,
    kMemoryUnreadable,
    kCount,
};

struct AotFfBoundaryTargetDecoded
{
    AotFfTargetStatus status = AotFfTargetStatus::kNotFf4;
    bool register_form = false;
    bool has_base = false;
    bool has_index = false;
    bool has_displacement = false;
    bool has_segment_override = false;
    std::uint8_t target_register = 0;
    std::uint8_t base_register = 0;
    std::uint8_t index_register = 0;
    std::uint8_t scale = 1;
    std::uint8_t segment_override = 0;
    std::uint32_t displacement = 0;
    std::uint32_t instruction_size = 0;
};

struct AotFfBoundaryTargetResult
{
    AotFfTargetStatus status = AotFfTargetStatus::kNotFf4;
    bool pointer_address_valid = false;
    std::uint32_t pointer_address = 0;
    bool index_value_valid = false;
    std::uint8_t index_register = 0;
    std::uint32_t index_value = 0;
    bool base_value_valid = false;
    std::uint8_t base_register = 0;
    std::uint32_t base_value = 0;
    bool target_valid = false;
    std::uint32_t target = 0;
};

AotFfTargetStatus DecodeAotFfBoundaryTarget(
    const std::uint8_t* bytes,
    std::size_t length,
    AotFfBoundaryTargetDecoded* decoded);

AotFfTargetStatus ResolveAotFfBoundaryTarget(
    const AotFfBoundaryTargetDecoded& decoded,
    ThreadContext* context,
    const repiu::platform::GuestCpuContext* win32_context,
    AotFfBoundaryTargetResult* result);

void RecordAotFfBoundaryTargetSample(
    AotFfBoundaryAttribution* attribution,
    ThreadContext* context,
    const repiu::platform::GuestCpuContext* win32_context,
    std::uint32_t guest_eip,
    const std::uint8_t* boundary_bytes,
    std::size_t boundary_length);

const char* AotFfTargetStatusName(AotFfTargetStatus status);

}  // namespace repiu::engine
