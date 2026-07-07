#include "repiu/exe/executable_headers.h"

#include <cstddef>
#include <sstream>

namespace repiu::exe
{
namespace
{

constexpr std::uint32_t kMzMinimumHeaderSize = 0x40;
constexpr std::uint32_t kMzSignatureOffset = 0x00;
constexpr std::uint32_t kMzBytesOnLastPageOffset = 0x02;
constexpr std::uint32_t kMzPageCountOffset = 0x04;
constexpr std::uint32_t kMzRelocationCountOffset = 0x06;
constexpr std::uint32_t kMzHeaderParagraphCountOffset = 0x08;
constexpr std::uint32_t kMzInitialIpOffset = 0x14;
constexpr std::uint32_t kMzInitialCsOffset = 0x16;
constexpr std::uint32_t kMzLeOffsetOffset = 0x3c;

constexpr std::uint32_t kLeHeaderMinimumSize = 0xa0;
constexpr std::uint32_t kLeSignatureOffset = 0x00;

bool HasBytes(const std::vector<std::uint8_t>& data,
              std::uint32_t offset,
              std::uint32_t size)
{
    return offset <= data.size() && size <= data.size() - offset;
}

std::uint8_t ReadU8(const std::vector<std::uint8_t>& data, std::uint32_t offset)
{
    return data[offset];
}

std::uint16_t ReadLe16(const std::vector<std::uint8_t>& data,
                       std::uint32_t offset)
{
    return static_cast<std::uint16_t>(data[offset]) |
           (static_cast<std::uint16_t>(data[offset + 1]) << 8);
}

std::uint32_t ReadLe32(const std::vector<std::uint8_t>& data,
                       std::uint32_t offset)
{
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

void SetError(std::uint32_t file_offset,
              const std::string& message,
              ParseError* error)
{
    if (error != nullptr)
    {
        error->file_offset = file_offset;
        error->message = message;
    }
}

std::string UnknownName(std::uint32_t value)
{
    std::ostringstream stream;
    stream << "unknown(" << value << ")";
    return stream.str();
}

}  // namespace

bool ParseMzHeader(const std::vector<std::uint8_t>& data,
                   MzHeader* header,
                   ParseError* error)
{
    if (header == nullptr)
    {
        SetError(0, "MZ output header is null", error);
        return false;
    }

    *header = MzHeader{};

    if (!HasBytes(data, 0, kMzMinimumHeaderSize))
    {
        SetError(0, "file is too small for an MZ header", error);
        return false;
    }

    const std::uint16_t signature = ReadLe16(data, kMzSignatureOffset);
    if (signature != 0x5a4d)
    {
        SetError(kMzSignatureOffset, "MZ signature was not found", error);
        return false;
    }

    header->valid = true;
    header->bytes_on_last_page = ReadLe16(data, kMzBytesOnLastPageOffset);
    header->page_count = ReadLe16(data, kMzPageCountOffset);
    header->relocation_count = ReadLe16(data, kMzRelocationCountOffset);
    header->header_paragraph_count =
        ReadLe16(data, kMzHeaderParagraphCountOffset);
    header->initial_ip = ReadLe16(data, kMzInitialIpOffset);
    header->initial_cs = ReadLe16(data, kMzInitialCsOffset);
    header->le_offset = ReadLe32(data, kMzLeOffsetOffset);

    if (!HasBytes(data, header->le_offset, kLeHeaderMinimumSize))
    {
        SetError(header->le_offset, "LE header offset points outside the file",
                 error);
        return false;
    }

    return true;
}

bool ParseLeHeader(const std::vector<std::uint8_t>& data,
                   std::uint32_t file_offset,
                   LeHeader* header,
                   ParseError* error)
{
    if (header == nullptr)
    {
        SetError(file_offset, "LE output header is null", error);
        return false;
    }

    *header = LeHeader{};

    if (!HasBytes(data, file_offset, kLeHeaderMinimumSize))
    {
        SetError(file_offset, "file is too small for an LE header", error);
        return false;
    }

    const std::uint16_t signature =
        ReadLe16(data, file_offset + kLeSignatureOffset);
    if (signature != 0x454c)
    {
        SetError(file_offset, "LE signature was not found", error);
        return false;
    }

    header->valid = true;
    header->file_offset = file_offset;
    header->byte_order = ReadU8(data, file_offset + 0x02);
    header->word_order = ReadU8(data, file_offset + 0x03);
    header->format_level = ReadLe32(data, file_offset + 0x04);
    header->cpu_type = ReadLe16(data, file_offset + 0x08);
    header->os_type = ReadLe16(data, file_offset + 0x0a);
    header->module_version = ReadLe32(data, file_offset + 0x0c);
    header->module_flags = ReadLe32(data, file_offset + 0x10);
    header->page_count = ReadLe32(data, file_offset + 0x14);
    header->entry_object = ReadLe32(data, file_offset + 0x18);
    header->entry_offset = ReadLe32(data, file_offset + 0x1c);
    header->stack_object = ReadLe32(data, file_offset + 0x20);
    header->stack_offset = ReadLe32(data, file_offset + 0x24);
    header->page_size = ReadLe32(data, file_offset + 0x28);
    header->last_page_size = ReadLe32(data, file_offset + 0x2c);
    header->fixup_section_size = ReadLe32(data, file_offset + 0x30);
    header->loader_section_size = ReadLe32(data, file_offset + 0x38);
    header->object_table_offset = ReadLe32(data, file_offset + 0x40);
    header->object_count = ReadLe32(data, file_offset + 0x44);
    header->object_page_table_offset = ReadLe32(data, file_offset + 0x48);
    header->resource_table_offset = ReadLe32(data, file_offset + 0x50);
    header->resident_name_table_offset = ReadLe32(data, file_offset + 0x58);
    header->entry_table_offset = ReadLe32(data, file_offset + 0x5c);
    header->fixup_page_table_offset = ReadLe32(data, file_offset + 0x68);
    header->fixup_record_table_offset = ReadLe32(data, file_offset + 0x6c);
    header->import_module_name_table_offset =
        ReadLe32(data, file_offset + 0x70);
    header->import_module_count = ReadLe32(data, file_offset + 0x74);
    header->import_procedure_name_table_offset =
        ReadLe32(data, file_offset + 0x78);
    header->data_pages_offset = ReadLe32(data, file_offset + 0x80);
    header->preload_page_count = ReadLe32(data, file_offset + 0x84);
    header->auto_data_object = ReadLe32(data, file_offset + 0x94);

    return true;
}

std::string CpuTypeName(std::uint16_t cpu_type)
{
    switch (cpu_type)
    {
        case 0x01:
            return "80286";
        case 0x02:
            return "80386";
        case 0x03:
            return "80486";
        default:
            return UnknownName(cpu_type);
    }
}

std::string OsTypeName(std::uint16_t os_type)
{
    switch (os_type)
    {
        case 0x01:
            return "OS/2";
        case 0x02:
            return "Windows";
        case 0x03:
            return "DOS 4.x";
        case 0x04:
            return "Windows 386";
        default:
            return UnknownName(os_type);
    }
}

}  // namespace repiu::exe
