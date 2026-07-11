#ifndef REPIU_EXE_EXECUTABLE_HEADERS_H_
#define REPIU_EXE_EXECUTABLE_HEADERS_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace repiu::exe
{

struct ParseError
{
    std::uint32_t file_offset = 0;
    std::string message;
};

struct MzHeader
{
    bool valid = false;
    std::uint16_t bytes_on_last_page = 0;
    std::uint16_t page_count = 0;
    std::uint16_t relocation_count = 0;
    std::uint16_t header_paragraph_count = 0;
    std::uint16_t initial_cs = 0;
    std::uint16_t initial_ip = 0;
    std::uint32_t le_offset = 0;
};

struct LeHeader
{
    bool valid = false;
    std::uint32_t file_offset = 0;
    std::uint8_t byte_order = 0;
    std::uint8_t word_order = 0;
    std::uint32_t format_level = 0;
    std::uint16_t cpu_type = 0;
    std::uint16_t os_type = 0;
    std::uint32_t module_version = 0;
    std::uint32_t module_flags = 0;
    std::uint32_t page_count = 0;
    std::uint32_t entry_object = 0;
    std::uint32_t entry_offset = 0;
    std::uint32_t stack_object = 0;
    std::uint32_t stack_offset = 0;
    std::uint32_t page_size = 0;
    std::uint32_t last_page_size = 0;
    std::uint32_t fixup_section_size = 0;
    std::uint32_t loader_section_size = 0;
    std::uint32_t object_table_offset = 0;
    std::uint32_t object_count = 0;
    std::uint32_t object_page_table_offset = 0;
    std::uint32_t resource_table_offset = 0;
    std::uint32_t resident_name_table_offset = 0;
    std::uint32_t entry_table_offset = 0;
    std::uint32_t fixup_page_table_offset = 0;
    std::uint32_t fixup_record_table_offset = 0;
    std::uint32_t import_module_name_table_offset = 0;
    std::uint32_t import_module_count = 0;
    std::uint32_t import_procedure_name_table_offset = 0;
    std::uint32_t data_pages_offset = 0;
    std::uint32_t preload_page_count = 0;
    std::uint32_t auto_data_object = 0;
};

struct LeObjectRecord
{
    std::uint32_t virtual_size = 0;
    std::uint32_t relocation_base_address = 0;
    std::uint32_t flags = 0;
    std::uint32_t page_table_index = 0;
    std::uint32_t page_count = 0;
    std::uint32_t reserved = 0;
};

struct LePageRecord
{
    std::uint32_t data_page_number = 0;
    std::uint8_t flags = 0;
};

struct LeMappedObject
{
    LeObjectRecord record;
    std::vector<std::uint8_t> memory;
    std::uint32_t copied_bytes = 0;
};

struct LeImage
{
    bool valid = false;
    std::vector<LeObjectRecord> objects;
    std::vector<LePageRecord> pages;
    std::vector<LeMappedObject> mapped_objects;
    std::uint64_t total_virtual_size = 0;
    std::uint64_t total_copied_bytes = 0;
    bool entry_point_valid = false;
};

struct LeResidentName
{
    std::string name;
    std::uint16_t ordinal = 0;
    std::uint32_t argument_byte_count = 0;
    bool decorated_argument_size_valid = false;
};

struct LeFixupPageSpan
{
    std::uint32_t page_index = 0;
    std::uint32_t record_offset = 0;
    std::uint32_t record_size = 0;
};

struct LeFixupInfo
{
    bool valid = false;
    std::uint32_t page_table_file_offset = 0;
    std::uint32_t record_table_file_offset = 0;
    std::uint32_t record_table_size = 0;
    std::uint32_t trailing_record_bytes = 0;
    bool page_table_monotonic = false;
    std::vector<std::uint32_t> page_offsets;
    std::vector<LeFixupPageSpan> page_spans;
    std::uint32_t pages_with_fixups = 0;
    std::uint32_t largest_page_span = 0;
};

struct LeFixupRecord
{
    std::uint32_t page_index = 0;
    std::uint32_t record_table_offset = 0;
    std::uint32_t record_size = 0;
    std::uint8_t source_type = 0;
    std::uint8_t target_flags = 0;
    std::uint16_t source_offset = 0;
    std::uint32_t target_object = 0;
    std::uint32_t target_offset = 0;
    bool supported = false;
};

struct LeFixupRecordInfo
{
    bool valid = false;
    std::vector<LeFixupRecord> records;
    std::uint32_t decoded_record_count = 0;
    std::uint32_t unsupported_record_count = 0;
    std::uint32_t internal_target_count = 0;
    std::uint32_t offset16_count = 0;
    std::uint32_t offset32_count = 0;
    std::uint32_t consumed_record_bytes = 0;
    std::uint32_t first_unsupported_record_offset = 0;
};

struct LeAppliedRelocation
{
    std::uint32_t source_object = 0;
    std::uint32_t source_object_offset = 0;
    std::uint32_t target_object = 0;
    std::uint32_t target_offset = 0;
    std::uint32_t previous_value = 0;
    std::uint32_t applied_value = 0;
};

struct LeSkippedRelocation
{
    std::uint32_t page_index = 0;
    std::uint32_t record_table_offset = 0;
    std::uint8_t source_type = 0;
    std::uint8_t source_kind = 0;
    std::uint16_t source_offset = 0;
    std::uint32_t source_object = 0;
    std::uint32_t source_object_offset = 0;
    std::uint32_t target_object = 0;
    std::uint32_t target_offset = 0;
};

struct LeRelocationDryRun
{
    bool valid = false;
    std::uint32_t applied_count = 0;
    std::uint32_t failed_count = 0;
    std::uint32_t unsupported_source_type_count = 0;
    std::uint32_t source_out_of_range_count = 0;
    std::uint32_t skipped_count = 0;
    std::uint32_t first_failed_record_offset = 0;
    std::array<std::uint32_t, 16> source_kind_counts = {};
    std::array<std::uint32_t, 256> source_type_counts = {};
    LeAppliedRelocation first_applied;
    bool has_first_applied = false;
    LeSkippedRelocation first_unsupported_source;
    bool has_first_unsupported_source = false;
    std::array<LeSkippedRelocation, 16> first_unsupported_source_by_kind;
    std::array<bool, 16> has_first_unsupported_source_by_kind = {};
    LeSkippedRelocation first_out_of_range;
    bool has_first_out_of_range = false;
    std::vector<LeSkippedRelocation> skipped_relocations;
};

bool ParseMzHeader(const std::vector<std::uint8_t>& data,
                   MzHeader* header,
                   ParseError* error);

bool ParseLeHeader(const std::vector<std::uint8_t>& data,
                   std::uint32_t file_offset,
                   LeHeader* header,
                   ParseError* error);

bool ParseLeObjectTable(const std::vector<std::uint8_t>& data,
                        const LeHeader& header,
                        std::vector<LeObjectRecord>* objects,
                        ParseError* error);

bool ParseLePageTable(const std::vector<std::uint8_t>& data,
                      const LeHeader& header,
                      std::vector<LePageRecord>* pages,
                        ParseError* error);

bool ParseLeResidentNames(const std::vector<std::uint8_t>& data,
                          const LeHeader& header,
                          std::vector<LeResidentName>* names,
                          ParseError* error);

bool BuildLeImage(const std::vector<std::uint8_t>& data,
                  const LeHeader& header,
                  LeImage* image,
                  ParseError* error);

bool AnalyzeLeFixups(const std::vector<std::uint8_t>& data,
                     const LeHeader& header,
                     LeFixupInfo* fixup_info,
                     ParseError* error);

bool DecodeLeFixupRecords(const std::vector<std::uint8_t>& data,
                          const LeHeader& header,
                          const LeFixupInfo& fixup_info,
                          LeFixupRecordInfo* record_info,
                          ParseError* error);

bool ApplyLeInternalRelocations(const LeHeader& header,
                                const LeFixupRecordInfo& record_info,
                                LeImage* image,
                                LeRelocationDryRun* dry_run,
                                ParseError* error);

std::string CpuTypeName(std::uint16_t cpu_type);

std::string OsTypeName(std::uint16_t os_type);

}  // namespace repiu::exe

#endif  // REPIU_EXE_EXECUTABLE_HEADERS_H_
