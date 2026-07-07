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
};

struct RelocatedRuntimeImage
{
    bool valid = false;
    std::uint32_t relocated_image_base = 0;
    std::uint32_t relocated_entry_linear_address = 0;
    std::uint32_t relocated_stack_top_linear_address = 0;
    std::vector<RelocatedRuntimeObject> objects;
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
