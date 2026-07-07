#include "repiu/runtime/runtime_memory.h"

#include <algorithm>
#include <limits>

namespace repiu::runtime
{
namespace
{

constexpr std::uint32_t kRuntimePageSize = 4096;

std::uint32_t AlignUp(std::uint32_t value, std::uint32_t alignment)
{
    const std::uint32_t remainder = value % alignment;
    if (remainder == 0)
    {
        return value;
    }

    return value + (alignment - remainder);
}

void SetError(const char* message, exe::ParseError* error)
{
    if (error != nullptr)
    {
        error->file_offset = 0;
        error->message = message;
    }
}

}  // namespace

bool BuildRuntimeMemoryPlan(const exe::Dos4gwLoadResult& load_result,
                            RuntimeMemoryPlan* plan,
                            exe::ParseError* error)
{
    if (plan == nullptr)
    {
        SetError("runtime memory plan output is null", error);
        return false;
    }

    *plan = RuntimeMemoryPlan{};

    if (!load_result.image.valid)
    {
        SetError("LE image is not valid", error);
        return false;
    }

    plan->object_regions.reserve(load_result.image.mapped_objects.size());
    std::uint64_t max_region_end = 0;
    for (std::uint32_t index = 0;
         index < load_result.image.mapped_objects.size();
         ++index)
    {
        const exe::LeMappedObject& mapped_object =
            load_result.image.mapped_objects[index];
        const std::uint64_t region_end =
            static_cast<std::uint64_t>(
                mapped_object.record.relocation_base_address) +
            mapped_object.record.virtual_size;
        max_region_end = std::max(max_region_end, region_end);

        RuntimeObjectRegion region;
        region.object_index = index + 1;
        region.base_address = mapped_object.record.relocation_base_address;
        region.virtual_size = mapped_object.record.virtual_size;
        region.copied_bytes = mapped_object.copied_bytes;
        region.flags = mapped_object.record.flags;
        plan->object_regions.push_back(region);
        plan->total_object_virtual_bytes += mapped_object.record.virtual_size;
    }

    if (max_region_end > std::numeric_limits<std::uint32_t>::max())
    {
        SetError("runtime object range exceeds 32-bit address space", error);
        return false;
    }

    if (load_result.le_header.entry_object > 0 &&
        load_result.le_header.entry_object <= plan->object_regions.size())
    {
        const RuntimeObjectRegion& entry_region =
            plan->object_regions[load_result.le_header.entry_object - 1];
        plan->entry_valid =
            load_result.le_header.entry_offset < entry_region.virtual_size;
        if (plan->entry_valid)
        {
            plan->entry_linear_address =
                entry_region.base_address + load_result.le_header.entry_offset;
        }
    }

    if (load_result.le_header.stack_object > 0 &&
        load_result.le_header.stack_object <= plan->object_regions.size())
    {
        const RuntimeObjectRegion& stack_region =
            plan->object_regions[load_result.le_header.stack_object - 1];
        plan->stack_valid =
            load_result.le_header.stack_offset <= stack_region.virtual_size;
        if (plan->stack_valid)
        {
            plan->stack_top_linear_address =
                stack_region.base_address + load_result.le_header.stack_offset;
        }
    }

    plan->hle_reserve_base =
        AlignUp(static_cast<std::uint32_t>(max_region_end), kRuntimePageSize);
    plan->valid = plan->entry_valid && plan->stack_valid;
    return true;
}

}  // namespace repiu::runtime
