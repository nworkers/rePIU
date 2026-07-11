#include "repiu/runtime/runtime_memory.h"

#include "repiu/runtime/selector_table.h"

#include <algorithm>
#include <limits>

namespace repiu::runtime
{
namespace
{

constexpr std::uint32_t kRuntimePageSize = 4096;
constexpr std::uint16_t kPiuDos4gwFirstObjectSelector = 0x1C;

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

const RelocatableRuntimeObjectRegion* FindRelocatedObject(
    const RelocatableRuntimeImagePlan& plan,
    std::uint32_t object_index)
{
    for (const RelocatableRuntimeObjectRegion& region :
         plan.object_regions)
    {
        if (region.object_index == object_index)
        {
            return &region;
        }
    }

    return nullptr;
}

const exe::LeMappedObject* FindSourceObjectForPage(
    const exe::LeImage& image,
    const exe::LeFixupRecord& record,
    std::uint32_t page_size,
    std::uint32_t* source_object_index,
    std::uint32_t* source_object_offset)
{
    for (std::uint32_t index = 0; index < image.mapped_objects.size();
         ++index)
    {
        const exe::LeMappedObject& candidate = image.mapped_objects[index];
        const std::uint32_t first_page =
            candidate.record.page_table_index - 1;
        const std::uint32_t page_count = candidate.record.page_count;
        if (record.page_index >= first_page &&
            record.page_index < first_page + page_count)
        {
            const std::uint32_t object_page_index =
                record.page_index - first_page;
            const std::uint64_t offset =
                static_cast<std::uint64_t>(object_page_index) * page_size +
                record.source_offset;
            if (offset > std::numeric_limits<std::uint32_t>::max())
            {
                return nullptr;
            }

            *source_object_index = index + 1;
            *source_object_offset = static_cast<std::uint32_t>(offset);
            return &candidate;
        }
    }

    return nullptr;
}

RelocatedRuntimeObject* FindRelocatedImageObject(
    RelocatedRuntimeImage* image,
    std::uint32_t object_index)
{
    if (image == nullptr)
    {
        return nullptr;
    }

    for (RelocatedRuntimeObject& object : image->objects)
    {
        if (object.object_index == object_index)
        {
            return &object;
        }
    }

    return nullptr;
}

std::uint32_t ReadLe32(const std::vector<std::uint8_t>& data,
                       std::uint32_t offset)
{
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

void WriteLe32(std::vector<std::uint8_t>* data,
               std::uint32_t offset,
               std::uint32_t value)
{
    (*data)[offset] = static_cast<std::uint8_t>(value & 0xff);
    (*data)[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
    (*data)[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xff);
    (*data)[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xff);
}

void WriteLe16(std::vector<std::uint8_t>* data,
               std::uint32_t offset,
               std::uint16_t value)
{
    (*data)[offset] = static_cast<std::uint8_t>(value & 0xff);
    (*data)[offset + 1] =
        static_cast<std::uint8_t>((value >> 8) & 0xff);
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
    if (!plan->stack_valid && load_result.le_header.stack_object == 0)
    {
        plan->stack_valid = true;
        plan->stack_top_linear_address = plan->hle_reserve_base;
    }
    plan->valid = plan->entry_valid && plan->stack_valid;
    return true;
}

bool BuildRelocatableRuntimeImagePlan(
    const exe::Dos4gwLoadResult& load_result,
    std::uint32_t relocated_image_base,
    RelocatableRuntimeImagePlan* plan,
    exe::ParseError* error)
{
    if (plan == nullptr)
    {
        SetError("relocatable runtime image plan output is null", error);
        return false;
    }

    *plan = RelocatableRuntimeImagePlan{};

    if (!load_result.image.valid ||
        !load_result.fixup_record_info.valid)
    {
        SetError("LE image or fixup records are invalid", error);
        return false;
    }

    if (load_result.image.mapped_objects.empty())
    {
        SetError("LE image has no mapped objects", error);
        return false;
    }

    std::uint32_t original_image_base =
        load_result.image.mapped_objects.front()
            .record.relocation_base_address;
    for (const exe::LeMappedObject& mapped_object :
         load_result.image.mapped_objects)
    {
        original_image_base = std::min(
            original_image_base,
            mapped_object.record.relocation_base_address);
    }

    if (relocated_image_base < original_image_base)
    {
        SetError("relocated image base is below original image base", error);
        return false;
    }

    const std::uint32_t relocation_delta =
        relocated_image_base - original_image_base;
    std::uint64_t max_region_end = 0;

    plan->original_image_base = original_image_base;
    plan->relocated_image_base = relocated_image_base;
    plan->relocation_delta = relocation_delta;
    plan->object_regions.reserve(load_result.image.mapped_objects.size());

    for (std::uint32_t index = 0;
         index < load_result.image.mapped_objects.size();
         ++index)
    {
        const exe::LeMappedObject& mapped_object =
            load_result.image.mapped_objects[index];
        const std::uint64_t relocated_base =
            static_cast<std::uint64_t>(
                mapped_object.record.relocation_base_address) +
            relocation_delta;
        const std::uint64_t relocated_end =
            relocated_base + mapped_object.record.virtual_size;
        if (relocated_end > std::numeric_limits<std::uint32_t>::max())
        {
            SetError("relocated runtime object exceeds 32-bit address space",
                     error);
            return false;
        }

        RelocatableRuntimeObjectRegion region;
        region.object_index = index + 1;
        region.original_base_address =
            mapped_object.record.relocation_base_address;
        region.relocated_base_address =
            static_cast<std::uint32_t>(relocated_base);
        region.virtual_size = mapped_object.record.virtual_size;
        region.copied_bytes = mapped_object.copied_bytes;
        region.flags = mapped_object.record.flags;
        plan->object_regions.push_back(region);
        plan->total_object_virtual_bytes += mapped_object.record.virtual_size;
        max_region_end = std::max(max_region_end, relocated_end);
    }

    if (load_result.le_header.entry_object > 0 &&
        load_result.le_header.entry_object <= plan->object_regions.size())
    {
        const RelocatableRuntimeObjectRegion& entry_region =
            plan->object_regions[load_result.le_header.entry_object - 1];
        plan->entry_valid =
            load_result.le_header.entry_offset < entry_region.virtual_size;
        if (plan->entry_valid)
        {
            plan->relocated_entry_linear_address =
                entry_region.relocated_base_address +
                load_result.le_header.entry_offset;
        }
    }

    if (load_result.le_header.stack_object > 0 &&
        load_result.le_header.stack_object <= plan->object_regions.size())
    {
        const RelocatableRuntimeObjectRegion& stack_region =
            plan->object_regions[load_result.le_header.stack_object - 1];
        plan->stack_valid =
            load_result.le_header.stack_offset <= stack_region.virtual_size;
        if (plan->stack_valid)
        {
            plan->relocated_stack_top_linear_address =
                stack_region.relocated_base_address +
                load_result.le_header.stack_offset;
        }
    }

    plan->relocated_hle_reserve_base =
        AlignUp(static_cast<std::uint32_t>(max_region_end),
                kRuntimePageSize);
    if (!plan->stack_valid && load_result.le_header.stack_object == 0)
    {
        plan->stack_valid = true;
        plan->relocated_stack_top_linear_address =
            plan->relocated_hle_reserve_base;
    }

    RelocatableRuntimeRelocationDryRun dry_run;
    for (const exe::LeFixupRecord& record :
         load_result.fixup_record_info.records)
    {
        const std::uint8_t source_kind = record.source_type & 0x0f;
        if (source_kind != 0x07)
        {
            ++dry_run.unsupported_source_type_count;
            ++dry_run.skipped_count;
            continue;
        }

        const RelocatableRuntimeObjectRegion* target_region =
            FindRelocatedObject(*plan, record.target_object);
        if (target_region == nullptr)
        {
            ++dry_run.failed_count;
            SetError("relocatable relocation target object is invalid",
                     error);
            return false;
        }

        std::uint32_t source_object_index = 0;
        std::uint32_t source_object_offset = 0;
        const exe::LeMappedObject* source_object = FindSourceObjectForPage(
            load_result.image,
            record,
            load_result.le_header.page_size,
            &source_object_index,
            &source_object_offset);
        if (source_object == nullptr)
        {
            ++dry_run.failed_count;
            SetError("relocatable relocation source page is invalid", error);
            return false;
        }

        if (source_object_offset > source_object->memory.size() ||
            source_object->memory.size() - source_object_offset < 4)
        {
            ++dry_run.source_out_of_range_count;
            ++dry_run.skipped_count;
            continue;
        }

        const std::uint32_t relocated_value =
            target_region->relocated_base_address + record.target_offset;
        const std::uint32_t original_value =
            target_region->original_base_address + record.target_offset;

        if (!dry_run.has_first_applied)
        {
            dry_run.first_relocated_value = relocated_value;
            dry_run.first_original_value = original_value;
            dry_run.first_source_object = source_object_index;
            dry_run.first_source_object_offset = source_object_offset;
            dry_run.first_target_object = record.target_object;
            dry_run.first_target_offset = record.target_offset;
            dry_run.has_first_applied = true;
        }

        ++dry_run.applied_count;
    }

    dry_run.valid = true;
    plan->relocation_dry_run = dry_run;
    plan->valid = plan->entry_valid && plan->stack_valid && dry_run.valid;
    return true;
}

bool BuildRelocatedRuntimeImage(
    const exe::Dos4gwLoadResult& load_result,
    const RelocatableRuntimeImagePlan& plan,
    RelocatedRuntimeImage* image,
    exe::ParseError* error)
{
    if (image == nullptr)
    {
        SetError("relocated runtime image output is null", error);
        return false;
    }

    *image = RelocatedRuntimeImage{};

    if (!load_result.image.valid ||
        !load_result.fixup_record_info.valid ||
        !plan.valid)
    {
        SetError("relocated runtime image input is invalid", error);
        return false;
    }

    image->relocated_image_base = plan.relocated_image_base;
    image->relocated_entry_linear_address =
        plan.relocated_entry_linear_address;
    image->relocated_stack_top_linear_address =
        plan.relocated_stack_top_linear_address;
    image->objects.reserve(load_result.image.mapped_objects.size());
    SelectorAllocator selector_allocator;
    if (!InitializeSelectorAllocator(&selector_allocator,
                                     kPiuDos4gwFirstObjectSelector))
    {
        SetError("DOS4GW object selector allocator is invalid", error);
        return false;
    }

    for (const RelocatableRuntimeObjectRegion& region :
         plan.object_regions)
    {
        if (region.object_index == 0 ||
            region.object_index > load_result.image.mapped_objects.size())
        {
            SetError("relocated runtime object index is invalid", error);
            return false;
        }

        const exe::LeMappedObject& mapped_object =
            load_result.image.mapped_objects[region.object_index - 1];

        RelocatedRuntimeObject object;
        object.object_index = region.object_index;
        object.relocated_base_address = region.relocated_base_address;
        object.virtual_size = region.virtual_size;
        object.flags = region.flags;
        object.memory = mapped_object.memory;
        image->objects.push_back(object);

        std::uint16_t selector = 0;
        if (region.virtual_size == 0 ||
            !AllocateSelector(&selector_allocator, &selector))
        {
            SetError("LE object selector binding is invalid", error);
            return false;
        }
        image->selector_bindings.push_back({
            selector,
            region.object_index,
            region.relocated_base_address,
            region.virtual_size - 1U,
        });
    }

    RelocatableRuntimeRelocationDryRun result;
    for (const exe::LeFixupRecord& record :
         load_result.fixup_record_info.records)
    {
        const std::uint8_t source_kind = record.source_type & 0x0f;
        if (source_kind != 0x07 && source_kind != 0x03)
        {
            ++result.unsupported_source_type_count;
            ++result.skipped_count;
            continue;
        }

        const RelocatableRuntimeObjectRegion* target_region =
            FindRelocatedObject(plan, record.target_object);
        if (target_region == nullptr)
        {
            ++result.failed_count;
            SetError("relocated image target object is invalid", error);
            return false;
        }

        std::uint32_t source_object_index = 0;
        std::uint32_t source_object_offset = 0;
        const exe::LeMappedObject* source_mapped_object =
            FindSourceObjectForPage(load_result.image,
                                    record,
                                    load_result.le_header.page_size,
                                    &source_object_index,
                                    &source_object_offset);
        if (source_mapped_object == nullptr)
        {
            ++result.failed_count;
            SetError("relocated image source page is invalid", error);
            return false;
        }

        RelocatedRuntimeObject* source_object =
            FindRelocatedImageObject(image, source_object_index);
        if (source_object == nullptr)
        {
            ++result.failed_count;
            SetError("relocated image source object is invalid", error);
            return false;
        }

        if (source_object_offset > source_object->memory.size() ||
            source_object->memory.size() - source_object_offset < 4)
        {
            ++result.source_out_of_range_count;
            ++result.skipped_count;
            continue;
        }

        if (source_kind == 0x03)
        {
            ++image->selector_binding_record_count;
            const auto existing = std::find_if(
                image->selector_bindings.begin(),
                image->selector_bindings.end(),
                [record](const RelocatedSelectorBinding& candidate) {
                    return candidate.target_object == record.target_object;
                });
            if (existing == image->selector_bindings.end())
            {
                ++image->selector_binding_conflict_count;
                ++result.failed_count;
            }
            else
            {
                WriteLe16(&source_object->memory,
                          source_object_offset,
                          static_cast<std::uint16_t>(record.target_offset));
                WriteLe16(&source_object->memory,
                          source_object_offset + 2,
                          existing->selector);
                ++result.applied_count;
            }
            continue;
        }

        const std::uint32_t relocated_value =
            target_region->relocated_base_address + record.target_offset;
        const std::uint32_t previous_value =
            ReadLe32(source_object->memory, source_object_offset);

        WriteLe32(&source_object->memory,
                  source_object_offset,
                  relocated_value);

        if (!result.has_first_applied)
        {
            result.first_relocated_value = relocated_value;
            result.first_original_value = previous_value;
            result.first_source_object = source_object_index;
            result.first_source_object_offset = source_object_offset;
            result.first_target_object = record.target_object;
            result.first_target_offset = record.target_offset;
            result.has_first_applied = true;
        }

        ++result.applied_count;
    }

    result.valid = true;
    image->relocation_result = result;
    image->valid = result.valid;
    return true;
}

}  // namespace repiu::runtime
