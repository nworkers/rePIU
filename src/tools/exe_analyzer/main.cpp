#include "repiu/exe/executable_headers.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
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

}  // namespace

int main(int argc, char** argv)
{
    const std::filesystem::path path =
        argc >= 2 ? std::filesystem::path(argv[1])
                  : std::filesystem::path("MASTER/PIU_1ST/PIU.EXE");

    std::vector<std::uint8_t> data;
    std::string read_error;
    if (!ReadBinaryFile(path, &data, &read_error))
    {
        std::cerr << "Failed to read " << path.string() << ": "
                  << read_error << "\n";
        return 1;
    }

    std::cout << "Target: piu_1st\n";
    std::cout << "Path: " << path.string() << "\n";
    std::cout << "File size: " << data.size() << " bytes\n";

    repiu::exe::MzHeader mz_header;
    repiu::exe::ParseError error;
    if (!repiu::exe::ParseMzHeader(data, &mz_header, &error))
    {
        PrintParseError(error);
        return 1;
    }

    PrintMzHeader(mz_header);

    repiu::exe::LeHeader le_header;
    if (!repiu::exe::ParseLeHeader(data, mz_header.le_offset, &le_header,
                                   &error))
    {
        PrintParseError(error);
        return 1;
    }

    PrintLeHeader(le_header);

    repiu::exe::LeImage image;
    if (!repiu::exe::BuildLeImage(data, le_header, &image, &error))
    {
        PrintParseError(error);
        return 1;
    }

    PrintObjectTable(image.objects);
    PrintPageSummary(image.pages);
    PrintImageSummary(image);
    return 0;
}
