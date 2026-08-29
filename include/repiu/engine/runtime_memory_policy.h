#ifndef REPIU_ENGINE_RUNTIME_MEMORY_POLICY_H_
#define REPIU_ENGINE_RUNTIME_MEMORY_POLICY_H_

#include "repiu/runtime/runtime_memory.h"

#include <cstdint>
#include <string>

namespace repiu::engine
{

struct RuntimeMemoryPolicy
{
    bool valid = false;
    std::uint32_t host_pointer_bits = 0;
    bool direct_x86_execution_supported = false;
    std::uint32_t preferred_allocation_base = 0;
    std::uint32_t required_reserve_size = 0;
    std::uint32_t hle_reserve_base = 0;
    std::string message;
};

struct AddressRangeProbe
{
    bool valid = false;
    bool range_available = false;
    std::uint32_t checked_base = 0;
    std::uint32_t checked_size = 0;
    std::uint32_t first_block_base = 0;
    std::uint32_t first_block_size = 0;
    std::string first_block_state;
    std::string message;
};

struct AddressRangeReservation
{
    bool valid = false;
    bool reserved = false;
    std::uint32_t requested_base = 0;
    std::uint32_t requested_size = 0;
    std::uint32_t reserved_base = 0;
    std::uint32_t reserved_size = 0;
    std::uint32_t windows_error = 0;
    std::string message;
};

struct RelocatedImagePlacement
{
    bool valid = false;
    bool placed = false;
    std::uint32_t requested_base = 0;
    std::uint32_t requested_size = 0;
    std::uint32_t placed_base = 0;
    std::uint32_t placed_size = 0;
    std::uint32_t hle_reserve_base = 0;
    std::uint32_t arena_end_address = 0;
    std::uint32_t copied_object_count = 0;
    std::uint32_t protected_object_count = 0;
    std::vector<runtime::RelocatedSelectorBinding> selector_bindings;
    std::uint32_t windows_error = 0;
    std::string message;
};

bool BuildRuntimeMemoryPolicy(
    const runtime::RuntimeMemoryPlan& memory_plan,
    RuntimeMemoryPolicy* policy);

bool BuildRuntimeMemoryPolicyFromFixedRange(
    std::uint32_t preferred_allocation_base,
    std::uint32_t required_reserve_size,
    RuntimeMemoryPolicy* policy);

bool ProbeRuntimeAddressRange(
    const RuntimeMemoryPolicy& policy,
    AddressRangeProbe* probe);

bool ReserveRuntimeAddressRange(
    const RuntimeMemoryPolicy& policy,
    AddressRangeReservation* reservation);

bool ReserveAndCommitRuntimeAddressRange(
    const RuntimeMemoryPolicy& policy,
    AddressRangeReservation* reservation);

bool ReleaseRuntimeAddressRange(
    const AddressRangeReservation& reservation);

bool PlaceRelocatedImage(
    const runtime::RelocatedRuntimeImage& image,
    RelocatedImagePlacement* placement);

bool PlaceRelocatedImage(
    const runtime::RelocatedRuntimeImage& image,
    std::uint32_t minimum_reserve_size,
    RelocatedImagePlacement* placement);

bool PlaceRelocatedImageInReservedRange(
    const runtime::RelocatedRuntimeImage& image,
    const AddressRangeReservation& reservation,
    RelocatedImagePlacement* placement);

bool ReleaseRelocatedImage(
    const RelocatedImagePlacement& placement);

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_RUNTIME_MEMORY_POLICY_H_
