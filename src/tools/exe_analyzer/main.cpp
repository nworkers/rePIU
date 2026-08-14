#include "repiu/assets/piu_chd_mount.h"
#include "repiu/exe/dos4gw_loader.h"
#include "repiu/exe/executable_headers.h"
#include "repiu/hle/hle_profile.h"
#include "repiu/platform/win32/runtime_memory_policy.h"
#include "repiu/runtime/runtime_memory.h"
#include "repiu/target/target_profile.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

std::string Hex32(std::uint32_t value)
{
    std::ostringstream stream;
    stream << "0x" << std::uppercase << std::hex << std::setw(8)
           << std::setfill('0') << value;
    return stream.str();
}

std::string Hex16(std::uint16_t value)
{
    std::ostringstream stream;
    stream << "0x" << std::uppercase << std::hex << std::setw(4)
           << std::setfill('0') << value;
    return stream.str();
}

bool ReadBinaryFile(const std::filesystem::path& path,
                    std::vector<std::uint8_t>* data,
                    std::string* error_message)
{
    if (data == nullptr)
    {
        if (error_message != nullptr)
        {
            *error_message = "output buffer is null";
        }
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        if (error_message != nullptr)
        {
            *error_message = "failed to open file";
        }
        return false;
    }

    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size < 0)
    {
        if (error_message != nullptr)
        {
            *error_message = "failed to determine file size";
        }
        return false;
    }

    file.seekg(0, std::ios::beg);
    data->resize(static_cast<std::size_t>(size));
    if (!data->empty())
    {
        file.read(reinterpret_cast<char*>(data->data()), size);
        if (!file)
        {
            if (error_message != nullptr)
            {
                *error_message = "failed to read file contents";
            }
            return false;
        }
    }

    return true;
}

void PrintParseError(const repiu::exe::ParseError& error)
{
    std::cerr << "Parse error at " << Hex32(error.file_offset) << ": "
              << error.message << "\n";
}

void PrintMzHeader(const repiu::exe::MzHeader& header)
{
    std::cout << "MZ: " << (header.valid ? "valid" : "invalid") << "\n";
    std::cout << "MZ bytes on last page: "
              << header.bytes_on_last_page << "\n";
    std::cout << "MZ page count: " << header.page_count << "\n";
    std::cout << "MZ relocation count: " << header.relocation_count << "\n";
    std::cout << "MZ header paragraphs: "
              << header.header_paragraph_count << "\n";
    std::cout << "MZ initial CS:IP: " << Hex16(header.initial_cs) << ":"
              << Hex16(header.initial_ip) << "\n";
    std::cout << "LE offset: " << Hex32(header.le_offset) << "\n";
}

void PrintLeHeader(const repiu::exe::LeHeader& header)
{
    std::cout << "LE signature: " << (header.valid ? "valid" : "invalid")
              << "\n";
    std::cout << "LE byte order: " << static_cast<int>(header.byte_order)
              << "\n";
    std::cout << "LE word order: " << static_cast<int>(header.word_order)
              << "\n";
    std::cout << "LE format level: " << header.format_level << "\n";
    std::cout << "LE CPU type: " << Hex16(header.cpu_type) << " ("
              << repiu::exe::CpuTypeName(header.cpu_type) << ")\n";
    std::cout << "LE OS type: " << Hex16(header.os_type) << " ("
              << repiu::exe::OsTypeName(header.os_type) << ")\n";
    std::cout << "LE module version: " << header.module_version << "\n";
    std::cout << "LE module flags: " << Hex32(header.module_flags) << "\n";
    std::cout << "LE page count: " << header.page_count << "\n";
    std::cout << "LE entry object: " << header.entry_object << "\n";
    std::cout << "LE entry offset: " << Hex32(header.entry_offset) << "\n";
    std::cout << "LE stack object: " << header.stack_object << "\n";
    std::cout << "LE stack offset: " << Hex32(header.stack_offset) << "\n";
    std::cout << "LE page size: " << header.page_size << "\n";
    std::cout << "LE last page size: " << header.last_page_size << "\n";
    std::cout << "LE fixup section size: "
              << header.fixup_section_size << "\n";
    std::cout << "LE loader section size: "
              << header.loader_section_size << "\n";
    std::cout << "LE object table offset: "
              << Hex32(header.object_table_offset) << "\n";
    std::cout << "LE object count: " << header.object_count << "\n";
    std::cout << "LE object page table offset: "
              << Hex32(header.object_page_table_offset) << "\n";
    std::cout << "LE resource table offset: "
              << Hex32(header.resource_table_offset) << "\n";
    std::cout << "LE resident name table offset: "
              << Hex32(header.resident_name_table_offset) << "\n";
    std::cout << "LE entry table offset: "
              << Hex32(header.entry_table_offset) << "\n";
    std::cout << "LE fixup page table offset: "
              << Hex32(header.fixup_page_table_offset) << "\n";
    std::cout << "LE fixup record table offset: "
              << Hex32(header.fixup_record_table_offset) << "\n";
    std::cout << "LE import module name table offset: "
              << Hex32(header.import_module_name_table_offset) << "\n";
    std::cout << "LE import module count: "
              << header.import_module_count << "\n";
    std::cout << "LE import procedure name table offset: "
              << Hex32(header.import_procedure_name_table_offset) << "\n";
    std::cout << "LE data pages offset: "
              << Hex32(header.data_pages_offset) << "\n";
    std::cout << "LE preload page count: "
              << header.preload_page_count << "\n";
    std::cout << "LE auto data object: "
              << header.auto_data_object << "\n";
}

void PrintObjectTable(const std::vector<repiu::exe::LeObjectRecord>& objects)
{
    std::cout << "LE objects:\n";
    for (std::size_t index = 0; index < objects.size(); ++index)
    {
        const repiu::exe::LeObjectRecord& object = objects[index];
        std::cout << "  [" << (index + 1) << "] virtual_size="
                  << Hex32(object.virtual_size)
                  << " base=" << Hex32(object.relocation_base_address)
                  << " flags=" << Hex32(object.flags)
                  << " page_table_index=" << object.page_table_index
                  << " page_count=" << object.page_count << "\n";
    }
}

void PrintPageSummary(const std::vector<repiu::exe::LePageRecord>& pages)
{
    std::cout << "LE page records: " << pages.size() << "\n";
    if (pages.empty())
    {
        return;
    }

    const repiu::exe::LePageRecord& first = pages.front();
    const repiu::exe::LePageRecord& last = pages.back();
    std::cout << "LE first page: data_page=" << first.data_page_number
              << " flags=" << Hex16(first.flags) << "\n";
    std::cout << "LE last page: data_page=" << last.data_page_number
              << " flags=" << Hex16(last.flags) << "\n";
}

void PrintImageSummary(const repiu::exe::LeImage& image)
{
    std::cout << "LE image map: " << (image.valid ? "valid" : "invalid")
              << "\n";
    std::cout << "LE mapped objects: "
              << image.mapped_objects.size() << "\n";
    std::cout << "LE total virtual size: "
              << image.total_virtual_size << " bytes\n";
    std::cout << "LE total copied bytes: "
              << image.total_copied_bytes << " bytes\n";
    std::cout << "LE entry mapping: "
              << (image.entry_point_valid ? "valid" : "invalid") << "\n";

    for (std::size_t index = 0; index < image.mapped_objects.size(); ++index)
    {
        const repiu::exe::LeMappedObject& object =
            image.mapped_objects[index];
        std::cout << "  mapped[" << (index + 1) << "] size="
                  << object.memory.size()
                  << " copied=" << object.copied_bytes << "\n";
    }
}

void PrintFixupSummary(const repiu::exe::LeFixupInfo& fixup_info)
{
    std::cout << "LE fixup page table: "
              << (fixup_info.valid ? "valid" : "invalid") << "\n";
    std::cout << "LE fixup page table file offset: "
              << Hex32(fixup_info.page_table_file_offset) << "\n";
    std::cout << "LE fixup record table file offset: "
              << Hex32(fixup_info.record_table_file_offset) << "\n";
    std::cout << "LE fixup record table size: "
              << fixup_info.record_table_size << " bytes\n";
    std::cout << "LE fixup page table entries: "
              << fixup_info.page_offsets.size() << "\n";
    std::cout << "LE fixup page table monotonic: "
              << (fixup_info.page_table_monotonic ? "true" : "false")
              << "\n";
    std::cout << "LE pages with fixups: "
              << fixup_info.pages_with_fixups << "\n";
    std::cout << "LE largest fixup page span: "
              << fixup_info.largest_page_span << " bytes\n";
    std::cout << "LE trailing fixup record bytes: "
              << fixup_info.trailing_record_bytes << "\n";

    if (!fixup_info.page_spans.empty())
    {
        const repiu::exe::LeFixupPageSpan& first =
            fixup_info.page_spans.front();
        const repiu::exe::LeFixupPageSpan& last =
            fixup_info.page_spans.back();
        std::cout << "LE first fixup span: page=" << first.page_index
                  << " offset=" << Hex32(first.record_offset)
                  << " size=" << first.record_size << "\n";
        std::cout << "LE last fixup span: page=" << last.page_index
                  << " offset=" << Hex32(last.record_offset)
                  << " size=" << last.record_size << "\n";
    }
}

void PrintFixupRecordSummary(
    const repiu::exe::LeFixupRecordInfo& record_info)
{
    std::cout << "LE fixup records: "
              << (record_info.valid ? "valid" : "invalid") << "\n";
    std::cout << "LE decoded fixup records: "
              << record_info.decoded_record_count << "\n";
    std::cout << "LE unsupported fixup records: "
              << record_info.unsupported_record_count << "\n";
    std::cout << "LE internal target fixups: "
              << record_info.internal_target_count << "\n";
    std::cout << "LE 16-bit target offset fixups: "
              << record_info.offset16_count << "\n";
    std::cout << "LE 32-bit target offset fixups: "
              << record_info.offset32_count << "\n";
    std::cout << "LE consumed fixup record bytes: "
              << record_info.consumed_record_bytes << "\n";

    if (!record_info.records.empty())
    {
        const repiu::exe::LeFixupRecord& first = record_info.records.front();
        const repiu::exe::LeFixupRecord& last = record_info.records.back();
        std::cout << "LE first decoded fixup: page=" << first.page_index
                  << " source_type=" << Hex16(first.source_type)
                  << " flags=" << Hex16(first.target_flags)
                  << " source_offset=" << Hex16(first.source_offset)
                  << " target_object=" << first.target_object
                  << " target_offset=" << Hex32(first.target_offset)
                  << "\n";
        std::cout << "LE last decoded fixup: page=" << last.page_index
                  << " source_type=" << Hex16(last.source_type)
                  << " flags=" << Hex16(last.target_flags)
                  << " source_offset=" << Hex16(last.source_offset)
                  << " target_object=" << last.target_object
                  << " target_offset=" << Hex32(last.target_offset)
                  << "\n";
    }
}

void PrintRelocationDryRunSummary(
    const repiu::exe::LeRelocationDryRun& dry_run)
{
    std::cout << "LE relocation dry run: "
              << (dry_run.valid ? "valid" : "invalid") << "\n";
    std::cout << "LE applied relocations: "
              << dry_run.applied_count << "\n";
    std::cout << "LE failed relocations: "
              << dry_run.failed_count << "\n";
    std::cout << "LE skipped relocations: "
              << dry_run.skipped_count << "\n";
    std::cout << "LE unsupported relocation source types: "
              << dry_run.unsupported_source_type_count << "\n";
    std::cout << "LE source out-of-range relocations: "
              << dry_run.source_out_of_range_count << "\n";
    std::cout << "LE relocation source kind counts:";
    for (std::size_t index = 0; index < dry_run.source_kind_counts.size();
         ++index)
    {
        if (dry_run.source_kind_counts[index] != 0)
        {
            std::cout << " kind" << index << "="
                      << dry_run.source_kind_counts[index];
        }
    }
    std::cout << "\n";

    std::cout << "LE relocation source type counts:";
    for (std::size_t index = 0; index < dry_run.source_type_counts.size();
         ++index)
    {
        if (dry_run.source_type_counts[index] != 0)
        {
            std::cout << " type" << Hex16(static_cast<std::uint16_t>(index))
                      << "=" << dry_run.source_type_counts[index];
        }
    }
    std::cout << "\n";

    if (dry_run.has_first_applied)
    {
        const repiu::exe::LeAppliedRelocation& relocation =
            dry_run.first_applied;
        std::cout << "LE first applied relocation: source_object="
                  << relocation.source_object
                  << " source_offset="
                  << Hex32(relocation.source_object_offset)
                  << " target_object=" << relocation.target_object
                  << " target_offset=" << Hex32(relocation.target_offset)
                  << " previous=" << Hex32(relocation.previous_value)
                  << " applied=" << Hex32(relocation.applied_value)
                  << "\n";
    }

    if (dry_run.has_first_unsupported_source)
    {
        const repiu::exe::LeSkippedRelocation& skipped =
            dry_run.first_unsupported_source;
        std::cout << "LE first unsupported source relocation: page="
                  << skipped.page_index
                  << " record_offset=" << Hex32(skipped.record_table_offset)
                  << " source_type=" << Hex16(skipped.source_type)
                  << " source_kind=" << Hex16(skipped.source_kind)
                  << " source_offset=" << Hex16(skipped.source_offset)
                  << " target_object=" << skipped.target_object
                  << " target_offset=" << Hex32(skipped.target_offset)
                  << "\n";
    }

    for (std::size_t index = 0;
         index < dry_run.has_first_unsupported_source_by_kind.size();
         ++index)
    {
        if (!dry_run.has_first_unsupported_source_by_kind[index])
        {
            continue;
        }

        const repiu::exe::LeSkippedRelocation& skipped =
            dry_run.first_unsupported_source_by_kind[index];
        std::cout << "LE first unsupported source kind " << index
                  << ": page=" << skipped.page_index
                  << " record_offset=" << Hex32(skipped.record_table_offset)
                  << " source_type=" << Hex16(skipped.source_type)
                  << " source_offset=" << Hex16(skipped.source_offset)
                  << " target_object=" << skipped.target_object
                  << " target_offset=" << Hex32(skipped.target_offset)
                  << "\n";
    }

    if (dry_run.has_first_out_of_range)
    {
        const repiu::exe::LeSkippedRelocation& skipped =
            dry_run.first_out_of_range;
        std::cout << "LE first out-of-range relocation: page="
                  << skipped.page_index
                  << " record_offset=" << Hex32(skipped.record_table_offset)
                  << " source_type=" << Hex16(skipped.source_type)
                  << " source_kind=" << Hex16(skipped.source_kind)
                  << " source_offset=" << Hex16(skipped.source_offset)
                  << " source_object=" << skipped.source_object
                  << " source_object_offset="
                  << Hex32(skipped.source_object_offset)
                  << " target_object=" << skipped.target_object
                  << " target_offset=" << Hex32(skipped.target_offset)
                  << "\n";
    }

    if (!dry_run.skipped_relocations.empty())
    {
        std::cout << "LE skipped relocation details: "
                  << dry_run.skipped_relocations.size() << "\n";
        for (std::size_t index = 0;
             index < dry_run.skipped_relocations.size(); ++index)
        {
            const repiu::exe::LeSkippedRelocation& skipped =
                dry_run.skipped_relocations[index];
            std::cout << "  skipped[" << index << "] page="
                      << skipped.page_index
                      << " record_offset="
                      << Hex32(skipped.record_table_offset)
                      << " source_type=" << Hex16(skipped.source_type)
                      << " source_kind=" << Hex16(skipped.source_kind)
                      << " source_offset=" << Hex16(skipped.source_offset)
                      << " source_object=" << skipped.source_object
                      << " source_object_offset="
                      << Hex32(skipped.source_object_offset)
                      << " target_object=" << skipped.target_object
                      << " target_offset=" << Hex32(skipped.target_offset)
                      << "\n";
        }
    }
}

void PrintRuntimeMemoryPlan(const repiu::runtime::RuntimeMemoryPlan& plan)
{
    std::cout << "Runtime memory dry run: "
              << (plan.valid ? "valid" : "invalid") << "\n";
    std::cout << "Runtime object regions: "
              << plan.object_regions.size() << "\n";
    std::cout << "Runtime total object virtual bytes: "
              << plan.total_object_virtual_bytes << "\n";
    std::cout << "Runtime entry: "
              << (plan.entry_valid ? Hex32(plan.entry_linear_address)
                                   : "invalid")
              << "\n";
    std::cout << "Runtime stack top: "
              << (plan.stack_valid ? Hex32(plan.stack_top_linear_address)
                                   : "invalid")
              << "\n";
    std::cout << "Runtime HLE reserve base: "
              << Hex32(plan.hle_reserve_base) << "\n";

    for (const repiu::runtime::RuntimeObjectRegion& region :
         plan.object_regions)
    {
        std::cout << "  runtime_object[" << region.object_index << "] base="
                  << Hex32(region.base_address)
                  << " size=" << Hex32(region.virtual_size)
                  << " copied=" << region.copied_bytes
                  << " flags=" << Hex32(region.flags) << "\n";
    }
}

void PrintRelocatableRuntimeImagePlan(
    const repiu::runtime::RelocatableRuntimeImagePlan& plan)
{
    std::cout << "Relocatable runtime image dry run: "
              << (plan.valid ? "valid" : "invalid") << "\n";
    std::cout << "Relocatable original image base: "
              << Hex32(plan.original_image_base) << "\n";
    std::cout << "Relocatable image base: "
              << Hex32(plan.relocated_image_base) << "\n";
    std::cout << "Relocatable delta: "
              << Hex32(plan.relocation_delta) << "\n";
    std::cout << "Relocatable entry: "
              << (plan.entry_valid
                      ? Hex32(plan.relocated_entry_linear_address)
                      : "invalid")
              << "\n";
    std::cout << "Relocatable stack top: "
              << (plan.stack_valid
                      ? Hex32(plan.relocated_stack_top_linear_address)
                      : "invalid")
              << "\n";
    std::cout << "Relocatable HLE reserve base: "
              << Hex32(plan.relocated_hle_reserve_base) << "\n";
    std::cout << "Relocatable object regions: "
              << plan.object_regions.size() << "\n";

    for (const repiu::runtime::RelocatableRuntimeObjectRegion& region :
         plan.object_regions)
    {
        std::cout << "  reloc_object[" << region.object_index << "] old="
                  << Hex32(region.original_base_address)
                  << " new=" << Hex32(region.relocated_base_address)
                  << " size=" << Hex32(region.virtual_size)
                  << " copied=" << region.copied_bytes
                  << " flags=" << Hex32(region.flags) << "\n";
    }

    const repiu::runtime::RelocatableRuntimeRelocationDryRun& dry_run =
        plan.relocation_dry_run;
    std::cout << "Relocatable relocation dry run: "
              << (dry_run.valid ? "valid" : "invalid") << "\n";
    std::cout << "Relocatable applied relocations: "
              << dry_run.applied_count << "\n";
    std::cout << "Relocatable skipped relocations: "
              << dry_run.skipped_count << "\n";
    std::cout << "Relocatable failed relocations: "
              << dry_run.failed_count << "\n";
    std::cout << "Relocatable unsupported relocation source types: "
              << dry_run.unsupported_source_type_count << "\n";
    std::cout << "Relocatable source out-of-range relocations: "
              << dry_run.source_out_of_range_count << "\n";
    if (dry_run.has_first_applied)
    {
        std::cout << "Relocatable first applied relocation: source_object="
                  << dry_run.first_source_object
                  << " source_offset="
                  << Hex32(dry_run.first_source_object_offset)
                  << " target_object=" << dry_run.first_target_object
                  << " target_offset="
                  << Hex32(dry_run.first_target_offset)
                  << " original=" << Hex32(dry_run.first_original_value)
                  << " relocated="
                  << Hex32(dry_run.first_relocated_value) << "\n";
    }
}

void PrintRelocatedRuntimeImage(
    const repiu::runtime::RelocatedRuntimeImage& image)
{
    std::cout << "Relocated image buffer: "
              << (image.valid ? "valid" : "invalid") << "\n";
    std::cout << "Relocated image buffer base: "
              << Hex32(image.relocated_image_base) << "\n";
    std::cout << "Relocated image buffer entry: "
              << Hex32(image.relocated_entry_linear_address) << "\n";
    std::cout << "Relocated image buffer stack top: "
              << Hex32(image.relocated_stack_top_linear_address) << "\n";
    std::cout << "Relocated image buffer objects: "
              << image.objects.size() << "\n";

    for (const repiu::runtime::RelocatedRuntimeObject& object :
         image.objects)
    {
        std::cout << "  relocated_buffer[" << object.object_index
                  << "] base=" << Hex32(object.relocated_base_address)
                  << " size=" << Hex32(object.virtual_size)
                  << " bytes=" << object.memory.size()
                  << " flags=" << Hex32(object.flags) << "\n";
    }

    std::cout << "Relocated image selector binding records: "
              << image.selector_binding_record_count << "\n";
    std::cout << "Relocated image selector binding conflicts: "
              << image.selector_binding_conflict_count << "\n";
    std::cout << "Relocated image selector bindings: "
              << image.selector_bindings.size() << "\n";
    for (const repiu::runtime::RelocatedSelectorBinding& binding :
         image.selector_bindings)
    {
        std::cout << "  selector_binding selector="
                  << Hex16(binding.selector)
                  << " target_object=" << binding.target_object
                  << " base=" << Hex32(binding.relocated_base_address)
                  << " limit=" << Hex32(binding.limit) << "\n";
    }

    const repiu::runtime::RelocatableRuntimeRelocationDryRun& result =
        image.relocation_result;
    std::cout << "Relocated image relocation write: "
              << (result.valid ? "valid" : "invalid") << "\n";
    std::cout << "Relocated image applied relocations: "
              << result.applied_count << "\n";
    std::cout << "Relocated image skipped relocations: "
              << result.skipped_count << "\n";
    std::cout << "Relocated image failed relocations: "
              << result.failed_count << "\n";
    std::cout << "Relocated image unsupported relocation source types: "
              << result.unsupported_source_type_count << "\n";
    std::cout << "Relocated image source out-of-range relocations: "
              << result.source_out_of_range_count << "\n";
    if (result.has_first_applied)
    {
        std::cout << "Relocated image first written relocation: "
                  << "source_object=" << result.first_source_object
                  << " source_offset="
                  << Hex32(result.first_source_object_offset)
                  << " target_object=" << result.first_target_object
                  << " target_offset="
                  << Hex32(result.first_target_offset)
                  << " previous=" << Hex32(result.first_original_value)
                  << " applied="
                  << Hex32(result.first_relocated_value) << "\n";
    }
}

void PrintWin32RuntimeMemoryPolicy(
    const repiu::platform::win32::Win32RuntimeMemoryPolicy& policy)
{
    std::cout << "Win32 runtime memory policy: "
              << (policy.valid ? "valid" : "invalid") << "\n";
    std::cout << "Win32 host pointer bits: "
              << policy.host_pointer_bits << "\n";
    std::cout << "Win32 direct x86 execution: "
              << (policy.direct_x86_execution_supported ? "supported"
                                                        : "unsupported")
              << "\n";
    std::cout << "Win32 preferred allocation base: "
              << Hex32(policy.preferred_allocation_base) << "\n";
    std::cout << "Win32 required reserve size: "
              << Hex32(policy.required_reserve_size) << "\n";
    std::cout << "Win32 HLE reserve base: "
              << Hex32(policy.hle_reserve_base) << "\n";
    std::cout << "Win32 memory policy message: "
              << policy.message << "\n";
}

void PrintWin32AddressRangeProbe(
    const repiu::platform::win32::Win32AddressRangeProbe& probe)
{
    std::cout << "Win32 allocation dry run: "
              << (probe.valid ? "valid" : "invalid") << "\n";
    std::cout << "Win32 target range available: "
              << (probe.range_available ? "true" : "false") << "\n";
    std::cout << "Win32 checked base: "
              << Hex32(probe.checked_base) << "\n";
    std::cout << "Win32 checked size: "
              << Hex32(probe.checked_size) << "\n";
    if (probe.valid && !probe.range_available)
    {
        std::cout << "Win32 first blocking block base: "
                  << Hex32(probe.first_block_base) << "\n";
        std::cout << "Win32 first blocking block size: "
                  << Hex32(probe.first_block_size) << "\n";
        std::cout << "Win32 first blocking block state: "
                  << probe.first_block_state << "\n";
    }
    std::cout << "Win32 allocation dry run message: "
              << probe.message << "\n";
}

const repiu::target::TargetProfile* SelectTargetProfile(
    int argc,
    char** argv,
    std::filesystem::path* explicit_path)
{
    const repiu::target::TargetProfile* default_profile =
        repiu::target::FindTargetProfileById("pumpit1");
    if (argc < 2)
    {
        return default_profile;
    }

    const std::string_view first_arg(argv[1]);
    const repiu::target::TargetProfile* selected_profile =
        repiu::target::FindTargetProfileById(first_arg);
    if (selected_profile != nullptr)
    {
        if (argc >= 3 && explicit_path != nullptr)
        {
            *explicit_path = std::filesystem::path(argv[2]);
        }
        return selected_profile;
    }

    if (explicit_path != nullptr)
    {
        *explicit_path = std::filesystem::path(argv[1]);
    }
    return default_profile;
}

}  // namespace

int main(int argc, char** argv)
{
    std::filesystem::path explicit_path;
    const repiu::target::TargetProfile* target_profile =
        SelectTargetProfile(argc, argv, &explicit_path);
    std::optional<repiu::target::TargetProfile> mounted_profile;
    if (target_profile == nullptr)
    {
        std::cerr << "Default target profile was not found\n";
        return 1;
    }

    if (!target_profile->rom_set_id.empty() && explicit_path.empty())
    {
        repiu::assets::PiuChdMountResult mount;
        if (!repiu::assets::PreparePiuChdMount(
                target_profile->rom_set_id, "roms", "build/runtime_mounts",
                &mount) ||
            !mount.valid || !mount.mounted)
        {
            std::cerr << target_profile->rom_set_id
                      << " CHD mount failed: " << mount.message << "\n";
            return 1;
        }
        mounted_profile = *target_profile;
        mounted_profile->executable_path = mount.executable_path;
        mounted_profile->working_directory = mount.mount_root / "PIU";
        mounted_profile->asset_root = mount.mount_root;
        target_profile = &mounted_profile.value();
    }
    const std::filesystem::path path =
        explicit_path.empty() ? target_profile->executable_path
                              : explicit_path;
    const repiu::hle::HleProfile* hle_profile =
        repiu::hle::FindHleProfileById(target_profile->hle_profile_id);
    if (hle_profile == nullptr)
    {
        std::cerr << "HLE profile was not found: "
                  << target_profile->hle_profile_id << "\n";
        return 1;
    }

    std::vector<std::uint8_t> data;
    std::string read_error;
    if (!ReadBinaryFile(path, &data, &read_error))
    {
        std::cerr << "Failed to read " << path.string() << ": "
                  << read_error << "\n";
        return 1;
    }

    std::cout << "Target: " << target_profile->id << "\n";
    std::cout << "Target name: " << target_profile->display_name << "\n";
    std::cout << "Format hint: "
              << repiu::target::ExecutableFormatHintName(
                     target_profile->format_hint)
              << "\n";
    std::cout << "Working directory: "
              << target_profile->working_directory.string() << "\n";
    std::cout << "Asset root: "
              << target_profile->asset_root.string() << "\n";
    std::cout << "HLE profile: " << target_profile->hle_profile_id << "\n";
    std::cout << "HLE profile name: " << hle_profile->display_name << "\n";
    std::cout << "HLE services:";
    for (repiu::hle::HleService service : hle_profile->services)
    {
        std::cout << " " << repiu::hle::HleServiceName(service);
    }
    std::cout << "\n";
    std::cout << "Path: " << path.string() << "\n";
    std::cout << "File size: " << data.size() << " bytes\n";
#if defined(REPIU_WIN32_HOST_IMAGE_BASE)
    std::cout << "Win32 host image base policy: "
              << Hex32(REPIU_WIN32_HOST_IMAGE_BASE) << "\n";
#endif

    repiu::exe::ParseError error;
    repiu::exe::Dos4gwLoadResult load_result;
    if (!repiu::exe::LoadDos4gwExecutable(data, *target_profile,
                                          &load_result, &error))
    {
        PrintParseError(error);
        return 1;
    }

    PrintMzHeader(load_result.mz_header);
    PrintLeHeader(load_result.le_header);
    PrintObjectTable(load_result.image.objects);
    PrintPageSummary(load_result.image.pages);
    PrintImageSummary(load_result.image);
    PrintFixupSummary(load_result.fixup_info);
    PrintFixupRecordSummary(load_result.fixup_record_info);
    PrintRelocationDryRunSummary(load_result.relocation_dry_run);

    repiu::runtime::RuntimeMemoryPlan runtime_plan;
    if (!repiu::runtime::BuildRuntimeMemoryPlan(load_result, &runtime_plan,
                                                &error))
    {
        PrintParseError(error);
        return 1;
    }

    PrintRuntimeMemoryPlan(runtime_plan);

    repiu::runtime::RelocatableRuntimeImagePlan relocatable_plan;
    if (!repiu::runtime::BuildRelocatableRuntimeImagePlan(
            load_result, 0x01000000, &relocatable_plan, &error))
    {
        PrintParseError(error);
        return 1;
    }

    PrintRelocatableRuntimeImagePlan(relocatable_plan);

    repiu::runtime::RelocatedRuntimeImage relocated_image;
    if (!repiu::runtime::BuildRelocatedRuntimeImage(
            load_result, relocatable_plan, &relocated_image, &error))
    {
        PrintParseError(error);
        return 1;
    }

    PrintRelocatedRuntimeImage(relocated_image);

    repiu::platform::win32::Win32RuntimeMemoryPolicy win32_policy;
    if (!repiu::platform::win32::BuildWin32RuntimeMemoryPolicy(
            runtime_plan, &win32_policy))
    {
        std::cerr << "Failed to build Win32 runtime memory policy\n";
        return 1;
    }

    PrintWin32RuntimeMemoryPolicy(win32_policy);

    repiu::platform::win32::Win32AddressRangeProbe win32_probe;
    if (!repiu::platform::win32::ProbeWin32RuntimeAddressRange(
            win32_policy, &win32_probe))
    {
        std::cerr << "Failed to probe Win32 runtime address range: "
                  << win32_probe.message << "\n";
        return 1;
    }

    PrintWin32AddressRangeProbe(win32_probe);
    return 0;
}
