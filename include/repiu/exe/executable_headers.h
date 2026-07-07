#ifndef REPIU_EXE_EXECUTABLE_HEADERS_H_
#define REPIU_EXE_EXECUTABLE_HEADERS_H_

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

bool BuildLeImage(const std::vector<std::uint8_t>& data,
                  const LeHeader& header,
                  LeImage* image,
                  ParseError* error);

std::string CpuTypeName(std::uint16_t cpu_type);

std::string OsTypeName(std::uint16_t os_type);

}  // namespace repiu::exe

#endif  // REPIU_EXE_EXECUTABLE_HEADERS_H_
