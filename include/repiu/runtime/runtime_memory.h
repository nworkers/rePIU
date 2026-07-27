#ifndef REPIU_RUNTIME_RUNTIME_MEMORY_H_
#define REPIU_RUNTIME_RUNTIME_MEMORY_H_

#include "repiu/exe/dos4gw_loader.h"

#include <cstdint>
#include <vector>

namespace repiu::runtime
{

struct RuntimeObjectRegion
{
    std::uint32_t object_index = 0;
    std::uint32_t base_address = 0;
    std::uint32_t virtual_size = 0;
    std::uint32_t copied_bytes = 0;
    std::uint32_t flags = 0;
};

struct RuntimeMemoryPlan
{
    bool valid = false;
    std::vector<RuntimeObjectRegion> object_regions;
    std::uint32_t entry_linear_address = 0;
    std::uint32_t stack_top_linear_address = 0;
    std::uint32_t hle_reserve_base = 0;
    std::uint64_t total_object_virtual_bytes = 0;
    bool entry_valid = false;
    bool stack_valid = false;
};

struct RelocatableRuntimeObjectRegion
{
    std::uint32_t object_index = 0;
    std::uint32_t original_base_address = 0;
    std::uint32_t relocated_base_address = 0;
    std::uint32_t virtual_size = 0;
    std::uint32_t copied_bytes = 0;
    std::uint32_t flags = 0;
};

struct RelocatableRuntimeRelocationDryRun
{
    bool valid = false;
    std::uint32_t applied_count = 0;
    std::uint32_t skipped_count = 0;
    std::uint32_t failed_count = 0;
    std::uint32_t unsupported_source_type_count = 0;
    std::uint32_t source_out_of_range_count = 0;
    std::uint32_t first_relocated_value = 0;
    std::uint32_t first_original_value = 0;
    std::uint32_t first_source_object = 0;
    std::uint32_t first_source_object_offset = 0;
    std::uint32_t first_target_object = 0;
    std::uint32_t first_target_offset = 0;
    bool has_first_applied = false;
};

struct RelocatableRuntimeImagePlan
{
    bool valid = false;
    std::uint32_t original_image_base = 0;
    std::uint32_t relocated_image_base = 0;
    std::uint32_t relocation_delta = 0;
    std::uint32_t relocated_hle_reserve_base = 0;
    std::uint32_t relocated_entry_linear_address = 0;
    std::uint32_t relocated_stack_top_linear_address = 0;
    std::uint64_t total_object_virtual_bytes = 0;
    bool entry_valid = false;
    bool stack_valid = false;
    std::vector<RelocatableRuntimeObjectRegion> object_regions;
    RelocatableRuntimeRelocationDryRun relocation_dry_run;
};

struct RelocatedRuntimeObject
{
    std::uint32_t object_index = 0;
    std::uint32_t relocated_base_address = 0;
    std::uint32_t virtual_size = 0;
    std::uint32_t flags = 0;
    std::vector<std::uint8_t> memory;

    // Task 329: an optional zero-copy view of guest bytes that already live in
    // this process. When `external_bytes` is set, readers use it instead of
    // `memory`, so the dynamic translator can read the live 133.8MB arena
    // without copying it. Objects that own their bytes leave both fields unset
    // and behave exactly as before.
    //
    // Lifetime and immutability contract for the external view:
    //  * The range must stay mapped and readable for as long as this object
    //    exists. Nothing may decommit it or make it PAGE_NOACCESS.
    //  * Nothing may write the range while a reader is running, because a view
    //    has none of the point-in-time fixity a copy gives.
    //
    // The second rule holds today only because the guest thread is blocked in
    // the synchronous translation rendezvous (Task 327) and no other thread
    // writes the arena (Task 329 audit). WARNING: making translation
    // asynchronous silently invalidates that guarantee, so such a change must
    // first restore a copy (design option 2) for any object read this way.
    const std::uint8_t* external_bytes = nullptr;
    std::uint32_t external_byte_count = 0;
};

// Readers go through these so an owning object and an external view are read
// the same way. Returns nullptr for an empty owning object, which callers
// already have to handle through the size.
inline const std::uint8_t* RelocatedRuntimeObjectBytes(
    const RelocatedRuntimeObject& object)
{
    return object.external_bytes != nullptr ? object.external_bytes
                                            : object.memory.data();
}

inline std::uint64_t RelocatedRuntimeObjectByteCount(
    const RelocatedRuntimeObject& object)
{
    return object.external_bytes != nullptr
        ? static_cast<std::uint64_t>(object.external_byte_count)
        : static_cast<std::uint64_t>(object.memory.size());
}

struct RelocatedSelectorBinding
{
    std::uint16_t selector = 0;
    std::uint32_t target_object = 0;
    std::uint32_t relocated_base_address = 0;
    std::uint32_t limit = 0;
};

struct RelocatedRuntimeImage
{
    bool valid = false;
    std::uint32_t relocated_image_base = 0;
    std::uint32_t relocated_hle_reserve_base = 0;
    std::uint32_t relocated_entry_linear_address = 0;
    std::uint32_t relocated_stack_top_linear_address = 0;
    std::vector<RelocatedRuntimeObject> objects;
    std::vector<RelocatedSelectorBinding> selector_bindings;
    std::uint32_t selector_binding_record_count = 0;
    std::uint32_t selector_binding_conflict_count = 0;
    RelocatableRuntimeRelocationDryRun relocation_result;
};

bool BuildRuntimeMemoryPlan(const exe::Dos4gwLoadResult& load_result,
                            RuntimeMemoryPlan* plan,
                            exe::ParseError* error);

bool BuildRelocatableRuntimeImagePlan(
    const exe::Dos4gwLoadResult& load_result,
    std::uint32_t relocated_image_base,
    RelocatableRuntimeImagePlan* plan,
    exe::ParseError* error);

bool BuildRelocatedRuntimeImage(
    const exe::Dos4gwLoadResult& load_result,
    const RelocatableRuntimeImagePlan& plan,
    RelocatedRuntimeImage* image,
    exe::ParseError* error);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_RUNTIME_MEMORY_H_
