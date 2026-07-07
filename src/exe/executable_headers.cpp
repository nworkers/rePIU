#include "repiu/exe/executable_headers.h"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <utility>

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
constexpr std::uint32_t kLeObjectRecordSize = 24;
constexpr std::uint32_t kLePageRecordSize = 4;

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

std::uint32_t ReadMemoryLe32(const std::vector<std::uint8_t>& data,
                             std::uint32_t offset)
{
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

void WriteMemoryLe32(std::vector<std::uint8_t>* data,
                     std::uint32_t offset,
                     std::uint32_t value)
{
    (*data)[offset] = static_cast<std::uint8_t>(value & 0xff);
    (*data)[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
    (*data)[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xff);
    (*data)[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xff);
}

std::uint32_t ReadBe24(const std::vector<std::uint8_t>& data,
                       std::uint32_t offset)
{
    return (static_cast<std::uint32_t>(data[offset]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           static_cast<std::uint32_t>(data[offset + 2]);
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

bool ParseLeObjectTable(const std::vector<std::uint8_t>& data,
                        const LeHeader& header,
                        std::vector<LeObjectRecord>* objects,
                        ParseError* error)
{
    if (objects == nullptr)
    {
        SetError(header.file_offset, "LE object output vector is null", error);
        return false;
    }

    objects->clear();

    const std::uint32_t table_offset =
        header.file_offset + header.object_table_offset;
    const std::uint64_t table_size =
        static_cast<std::uint64_t>(header.object_count) * kLeObjectRecordSize;
    if (table_size > UINT32_MAX ||
        !HasBytes(data, table_offset, static_cast<std::uint32_t>(table_size)))
    {
        SetError(table_offset, "LE object table points outside the file",
                 error);
        return false;
    }

    objects->reserve(header.object_count);
    for (std::uint32_t index = 0; index < header.object_count; ++index)
    {
        const std::uint32_t offset =
            table_offset + index * kLeObjectRecordSize;
        LeObjectRecord object;
        object.virtual_size = ReadLe32(data, offset + 0x00);
        object.relocation_base_address = ReadLe32(data, offset + 0x04);
        object.flags = ReadLe32(data, offset + 0x08);
        object.page_table_index = ReadLe32(data, offset + 0x0c);
        object.page_count = ReadLe32(data, offset + 0x10);
        object.reserved = ReadLe32(data, offset + 0x14);
        objects->push_back(object);
    }

    return true;
}

bool ParseLePageTable(const std::vector<std::uint8_t>& data,
                      const LeHeader& header,
                      std::vector<LePageRecord>* pages,
                      ParseError* error)
{
    if (pages == nullptr)
    {
        SetError(header.file_offset, "LE page output vector is null", error);
        return false;
    }

    pages->clear();

    const std::uint32_t table_offset =
        header.file_offset + header.object_page_table_offset;
    const std::uint64_t table_size =
        static_cast<std::uint64_t>(header.page_count) * kLePageRecordSize;
    if (table_size > UINT32_MAX ||
        !HasBytes(data, table_offset, static_cast<std::uint32_t>(table_size)))
    {
        SetError(table_offset, "LE page table points outside the file", error);
        return false;
    }

    pages->reserve(header.page_count);
    for (std::uint32_t index = 0; index < header.page_count; ++index)
    {
        const std::uint32_t offset = table_offset + index * kLePageRecordSize;
        LePageRecord page;
        page.data_page_number = ReadBe24(data, offset);
        page.flags = ReadU8(data, offset + 0x03);
        pages->push_back(page);
    }

    return true;
}

bool BuildLeImage(const std::vector<std::uint8_t>& data,
                  const LeHeader& header,
                  LeImage* image,
                  ParseError* error)
{
    if (image == nullptr)
    {
        SetError(header.file_offset, "LE image output is null", error);
        return false;
    }

    *image = LeImage{};

    if (!ParseLeObjectTable(data, header, &image->objects, error) ||
        !ParseLePageTable(data, header, &image->pages, error))
    {
        return false;
    }

    image->mapped_objects.reserve(image->objects.size());
    for (const LeObjectRecord& object : image->objects)
    {
        if (object.page_table_index == 0)
        {
            SetError(header.file_offset + header.object_table_offset,
                     "LE object uses a zero page table index", error);
            return false;
        }

        const std::uint32_t first_page_index = object.page_table_index - 1;
        if (first_page_index > image->pages.size() ||
            object.page_count > image->pages.size() - first_page_index)
        {
            SetError(header.file_offset + header.object_page_table_offset,
                     "LE object page range exceeds the page table", error);
            return false;
        }

        LeMappedObject mapped_object;
        mapped_object.record = object;
        mapped_object.memory.resize(object.virtual_size);
        image->total_virtual_size += object.virtual_size;

        for (std::uint32_t page_index = 0; page_index < object.page_count;
             ++page_index)
        {
            const std::uint64_t object_offset =
                static_cast<std::uint64_t>(page_index) * header.page_size;
            if (object_offset >= mapped_object.memory.size())
            {
                continue;
            }

            const LePageRecord& page =
                image->pages[first_page_index + page_index];
            if (page.data_page_number == 0)
            {
                continue;
            }

            const std::uint64_t file_offset =
                static_cast<std::uint64_t>(header.data_pages_offset) +
                static_cast<std::uint64_t>(page.data_page_number - 1) *
                    header.page_size;
            if (file_offset > data.size())
            {
                SetError(static_cast<std::uint32_t>(
                             std::min<std::uint64_t>(file_offset, UINT32_MAX)),
                         "LE page data points outside the file", error);
                return false;
            }

            std::uint32_t page_bytes = header.page_size;
            if (page.data_page_number == header.page_count &&
                header.last_page_size != 0)
            {
                page_bytes = header.last_page_size;
            }

            const std::uint64_t remaining_object_bytes =
                mapped_object.memory.size() - object_offset;
            const std::uint64_t remaining_file_bytes =
                data.size() - file_offset;
            const std::uint64_t copy_size =
                std::min<std::uint64_t>(
                    page_bytes,
                    std::min(remaining_object_bytes, remaining_file_bytes));

            std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(file_offset),
                        static_cast<std::ptrdiff_t>(copy_size),
                        mapped_object.memory.begin() +
                            static_cast<std::ptrdiff_t>(object_offset));

            mapped_object.copied_bytes += static_cast<std::uint32_t>(copy_size);
            image->total_copied_bytes += copy_size;
        }

        image->mapped_objects.push_back(std::move(mapped_object));
    }

    if (header.entry_object > 0 &&
        header.entry_object <= image->mapped_objects.size())
    {
        const LeMappedObject& entry_object =
            image->mapped_objects[header.entry_object - 1];
        image->entry_point_valid =
            header.entry_offset < entry_object.memory.size();
    }

    image->valid = true;
    return true;
}

bool AnalyzeLeFixups(const std::vector<std::uint8_t>& data,
                     const LeHeader& header,
                     LeFixupInfo* fixup_info,
                     ParseError* error)
{
    if (fixup_info == nullptr)
    {
        SetError(header.file_offset, "LE fixup output is null", error);
        return false;
    }

    *fixup_info = LeFixupInfo{};

    if (header.fixup_record_table_offset < header.fixup_page_table_offset)
    {
        SetError(header.file_offset + header.fixup_record_table_offset,
                 "LE fixup record table appears before fixup page table",
                 error);
        return false;
    }

    if (header.import_module_name_table_offset <
        header.fixup_record_table_offset)
    {
        SetError(header.file_offset + header.import_module_name_table_offset,
                 "LE import module table appears before fixup record table",
                 error);
        return false;
    }

    const std::uint64_t page_entry_count =
        static_cast<std::uint64_t>(header.page_count) + 1;
    const std::uint64_t page_table_size = page_entry_count * sizeof(uint32_t);
    if (page_entry_count > UINT32_MAX || page_table_size > UINT32_MAX)
    {
        SetError(header.file_offset + header.fixup_page_table_offset,
                 "LE fixup page table is too large", error);
        return false;
    }

    const std::uint32_t page_table_file_offset =
        header.file_offset + header.fixup_page_table_offset;
    const std::uint32_t record_table_file_offset =
        header.file_offset + header.fixup_record_table_offset;
    const std::uint32_t record_table_size =
        header.import_module_name_table_offset -
        header.fixup_record_table_offset;

    if (!HasBytes(data, page_table_file_offset,
                  static_cast<std::uint32_t>(page_table_size)))
    {
        SetError(page_table_file_offset,
                 "LE fixup page table points outside the file", error);
        return false;
    }

    if (!HasBytes(data, record_table_file_offset, record_table_size))
    {
        SetError(record_table_file_offset,
                 "LE fixup record table points outside the file", error);
        return false;
    }

    fixup_info->page_table_file_offset = page_table_file_offset;
    fixup_info->record_table_file_offset = record_table_file_offset;
    fixup_info->record_table_size = record_table_size;
    fixup_info->page_table_monotonic = true;
    fixup_info->page_offsets.reserve(
        static_cast<std::size_t>(page_entry_count));
    fixup_info->page_spans.reserve(header.page_count);

    for (std::uint32_t index = 0; index < page_entry_count; ++index)
    {
        const std::uint32_t offset =
            ReadLe32(data, page_table_file_offset + index * sizeof(uint32_t));
        fixup_info->page_offsets.push_back(offset);

        if (index > 0 && offset < fixup_info->page_offsets[index - 1])
        {
            fixup_info->page_table_monotonic = false;
        }

        if (offset > record_table_size)
        {
            SetError(page_table_file_offset + index * sizeof(uint32_t),
                     "LE fixup page table offset exceeds record table size",
                     error);
            return false;
        }
    }

    for (std::uint32_t page_index = 0; page_index < header.page_count;
         ++page_index)
    {
        const std::uint32_t begin = fixup_info->page_offsets[page_index];
        const std::uint32_t end = fixup_info->page_offsets[page_index + 1];
        if (end < begin)
        {
            SetError(page_table_file_offset + page_index * sizeof(uint32_t),
                     "LE fixup page span is negative", error);
            return false;
        }

        LeFixupPageSpan span;
        span.page_index = page_index;
        span.record_offset = begin;
        span.record_size = end - begin;
        fixup_info->page_spans.push_back(span);

        if (span.record_size > 0)
        {
            ++fixup_info->pages_with_fixups;
            fixup_info->largest_page_span =
                std::max(fixup_info->largest_page_span, span.record_size);
        }
    }

    const std::uint32_t consumed_record_bytes = fixup_info->page_offsets.back();
    fixup_info->trailing_record_bytes =
        record_table_size - consumed_record_bytes;
    fixup_info->valid = true;
    return true;
}

bool DecodeLeFixupRecords(const std::vector<std::uint8_t>& data,
                          const LeHeader& header,
                          const LeFixupInfo& fixup_info,
                          LeFixupRecordInfo* record_info,
                          ParseError* error)
{
    if (record_info == nullptr)
    {
        SetError(header.file_offset, "LE fixup record output is null", error);
        return false;
    }

    *record_info = LeFixupRecordInfo{};

    if (!fixup_info.valid)
    {
        SetError(header.file_offset + header.fixup_page_table_offset,
                 "LE fixup info is not valid", error);
        return false;
    }

    record_info->records.reserve(fixup_info.record_table_size / 6);

    for (const LeFixupPageSpan& span : fixup_info.page_spans)
    {
        std::uint32_t cursor = span.record_offset;
        const std::uint32_t span_end = span.record_offset + span.record_size;

        while (cursor < span_end)
        {
            const std::uint32_t record_file_offset =
                fixup_info.record_table_file_offset + cursor;
            const std::uint32_t remaining = span_end - cursor;
            if (remaining < 4)
            {
                SetError(record_file_offset,
                         "LE fixup record is too small for its prefix",
                         error);
                return false;
            }

            LeFixupRecord record;
            record.page_index = span.page_index;
            record.record_table_offset = cursor;
            record.source_type = ReadU8(data, record_file_offset + 0);
            record.target_flags = ReadU8(data, record_file_offset + 1);
            record.source_offset = ReadLe16(data, record_file_offset + 2);

            std::uint32_t record_size = 4;
            const bool internal_target = (record.target_flags & 0x03) == 0;
            const bool has_32_bit_target_offset =
                (record.target_flags & 0x10) != 0;
            const bool has_unsupported_flags =
                (record.target_flags & static_cast<std::uint8_t>(~0x10)) != 0;

            if (!internal_target || has_unsupported_flags)
            {
                record.supported = false;
                ++record_info->unsupported_record_count;
                if (record_info->first_unsupported_record_offset == 0)
                {
                    record_info->first_unsupported_record_offset = cursor;
                }
                SetError(record_file_offset,
                         "unsupported LE fixup target flags encountered",
                         error);
                return false;
            }

            const std::uint32_t target_offset_size =
                has_32_bit_target_offset ? 4 : 2;
            const std::uint32_t required_size = 4 + 1 + target_offset_size;
            if (remaining < required_size)
            {
                SetError(record_file_offset,
                         "LE fixup record target extends past page span",
                         error);
                return false;
            }

            record.target_object = ReadU8(data, record_file_offset + 4);
            if (has_32_bit_target_offset)
            {
                record.target_offset = ReadLe32(data, record_file_offset + 5);
                ++record_info->offset32_count;
            }
            else
            {
                record.target_offset = ReadLe16(data, record_file_offset + 5);
                ++record_info->offset16_count;
            }

            record_size = required_size;
            record.record_size = record_size;
            record.supported = true;

            ++record_info->decoded_record_count;
            ++record_info->internal_target_count;
            record_info->consumed_record_bytes += record_size;
            record_info->records.push_back(record);
            cursor += record_size;
        }
    }

    if (record_info->consumed_record_bytes != fixup_info.record_table_size)
    {
        SetError(fixup_info.record_table_file_offset +
                     record_info->consumed_record_bytes,
                 "decoded LE fixup bytes do not match record table size",
                 error);
        return false;
    }

    record_info->valid = true;
    return true;
}

bool ApplyLeInternalRelocations(const LeHeader& header,
                                const LeFixupRecordInfo& record_info,
                                LeImage* image,
                                LeRelocationDryRun* dry_run,
                                ParseError* error)
{
    if (image == nullptr)
    {
        SetError(header.file_offset, "LE image is null", error);
        return false;
    }

    if (dry_run == nullptr)
    {
        SetError(header.file_offset, "LE relocation dry-run output is null",
                 error);
        return false;
    }

    *dry_run = LeRelocationDryRun{};

    if (!image->valid || !record_info.valid)
    {
        SetError(header.file_offset, "LE image or fixup records are invalid",
                 error);
        return false;
    }

    for (const LeFixupRecord& record : record_info.records)
    {
        const std::uint8_t source_kind = record.source_type & 0x0f;
        if (source_kind != 0x07)
        {
            ++dry_run->unsupported_source_type_count;
            ++dry_run->skipped_count;
            continue;
        }

        if (record.target_object == 0 ||
            record.target_object > image->mapped_objects.size())
        {
            ++dry_run->failed_count;
            dry_run->first_failed_record_offset = record.record_table_offset;
            SetError(header.file_offset + header.fixup_record_table_offset +
                         record.record_table_offset,
                     "LE relocation target object is outside the image",
                     error);
            return false;
        }

        LeMappedObject& target_object =
            image->mapped_objects[record.target_object - 1];
        const std::uint32_t applied_value =
            target_object.record.relocation_base_address +
            record.target_offset;

        LeMappedObject* source_object = nullptr;
        std::uint32_t source_object_index = 0;
        std::uint32_t source_object_offset = 0;
        for (std::uint32_t index = 0; index < image->mapped_objects.size();
             ++index)
        {
            LeMappedObject& candidate = image->mapped_objects[index];
            const std::uint32_t first_page =
                candidate.record.page_table_index - 1;
            const std::uint32_t page_count = candidate.record.page_count;
            if (record.page_index >= first_page &&
                record.page_index < first_page + page_count)
            {
                const std::uint32_t object_page_index =
                    record.page_index - first_page;
                const std::uint64_t offset =
                    static_cast<std::uint64_t>(object_page_index) *
                        header.page_size +
                    record.source_offset;
                if (offset > UINT32_MAX)
                {
                    break;
                }

                source_object = &candidate;
                source_object_index = index + 1;
                source_object_offset = static_cast<std::uint32_t>(offset);
                break;
            }
        }

        if (source_object == nullptr)
        {
            ++dry_run->failed_count;
            dry_run->first_failed_record_offset = record.record_table_offset;
            SetError(header.file_offset + header.fixup_record_table_offset +
                         record.record_table_offset,
                     "LE relocation source page has no owning object", error);
            return false;
        }

        if (source_object_offset > source_object->memory.size() ||
            source_object->memory.size() - source_object_offset < 4)
        {
            ++dry_run->source_out_of_range_count;
            ++dry_run->skipped_count;
            continue;
        }

        const std::uint32_t previous_value =
            ReadMemoryLe32(source_object->memory, source_object_offset);
        WriteMemoryLe32(&source_object->memory, source_object_offset,
                        applied_value);

        if (!dry_run->has_first_applied)
        {
            dry_run->first_applied.source_object = source_object_index;
            dry_run->first_applied.source_object_offset = source_object_offset;
            dry_run->first_applied.target_object = record.target_object;
            dry_run->first_applied.target_offset = record.target_offset;
            dry_run->first_applied.previous_value = previous_value;
            dry_run->first_applied.applied_value = applied_value;
            dry_run->has_first_applied = true;
        }

        ++dry_run->applied_count;
    }

    dry_run->valid = true;
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
