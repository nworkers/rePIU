#include "repiu/exe/dos16m_bound_module.h"

#include <algorithm>
#include <cstring>

namespace repiu::exe
{
namespace
{

constexpr std::uint32_t kBwHeaderSize = 176;
constexpr std::uint32_t kGdtEntrySize = 8;
constexpr std::uint32_t kReservedGdtBytes = 16 * kGdtEntrySize;
constexpr std::uint16_t kBssMarker = 0x2000;

bool Fail(ParseError* error, std::uint32_t offset, const char* message)
{
    if (error != nullptr)
    {
        error->file_offset = offset;
        error->message = message;
    }
    return false;
}

bool RangeValid(const std::vector<std::uint8_t>& data,
                std::uint32_t offset,
                std::uint32_t size)
{
    return offset <= data.size() && size <= data.size() - offset;
}

std::uint16_t Read16(const std::vector<std::uint8_t>& data,
                     std::uint32_t offset)
{
    return static_cast<std::uint16_t>(data[offset]) |
           static_cast<std::uint16_t>(data[offset + 1] << 8);
}

std::uint32_t Read32(const std::vector<std::uint8_t>& data,
                     std::uint32_t offset)
{
    return static_cast<std::uint32_t>(Read16(data, offset)) |
           (static_cast<std::uint32_t>(Read16(data, offset + 2)) << 16);
}

std::uint32_t Align16(std::uint32_t value)
{
    return (value + 15U) & ~15U;
}

Dos16mBoundSegment* FindSegment(Dos16mBoundModule* module,
                                std::uint16_t selector)
{
    for (Dos16mBoundSegment& segment : module->segments)
    {
        if (segment.selector == selector)
        {
            return &segment;
        }
    }
    return nullptr;
}

bool ParseModule(const std::vector<std::uint8_t>& data,
                 std::uint32_t header_offset,
                 Dos16mBoundModule* module,
                 ParseError* error)
{
    if (!RangeValid(data, header_offset, kBwHeaderSize) ||
        data[header_offset] != 'B' || data[header_offset + 1] != 'W')
    {
        return Fail(error, header_offset, "invalid DOS/16M BW header");
    }

    module->header_file_offset = header_offset;
    module->next_header_file_offset = Read32(data, header_offset + 28);
    module->initial_ip = Read16(data, header_offset + 20);
    module->initial_cs = Read16(data, header_offset + 22);
    const std::uint16_t first_relocation_selector =
        Read16(data, header_offset + 18);
    const std::uint32_t gdt_image_size =
        static_cast<std::uint32_t>(Read16(data, header_offset + 56)) + 1U;
    const std::uint16_t encoded_first_selector =
        Read16(data, header_offset + 58);
    const std::uint16_t first_selector =
        encoded_first_selector != 0 ? encoded_first_selector : 0x0080;
    if (module->next_header_file_offset <= header_offset ||
        module->next_header_file_offset > data.size() ||
        gdt_image_size < kReservedGdtBytes ||
        (gdt_image_size - kReservedGdtBytes) % kGdtEntrySize != 0 ||
        first_relocation_selector < first_selector ||
        (first_relocation_selector - first_selector) % kGdtEntrySize != 0)
    {
        return Fail(error, header_offset, "invalid DOS/16M BW layout");
    }

    const std::uint32_t name_offset = header_offset + 112;
    const std::uint32_t name_size = 64;
    if (!RangeValid(data, name_offset, name_size))
    {
        return Fail(error, name_offset, "truncated DOS/16M module name");
    }
    const auto name_end = std::find(data.begin() + name_offset,
                                    data.begin() + name_offset + name_size,
                                    0);
    module->name.assign(data.begin() + name_offset, name_end);

    const std::uint32_t gdt_count =
        (gdt_image_size - kReservedGdtBytes) / kGdtEntrySize;
    const std::uint32_t group_count =
        (first_relocation_selector - first_selector) / kGdtEntrySize;
    if (group_count >= gdt_count)
    {
        return Fail(error, header_offset + 18,
                    "DOS/16M relocation descriptor is missing");
    }
    const std::uint32_t gdt_offset = header_offset + kBwHeaderSize;
    if (!RangeValid(data, gdt_offset, gdt_count * kGdtEntrySize))
    {
        return Fail(error, gdt_offset, "truncated DOS/16M GDT image");
    }

    std::uint32_t copy_cursor = gdt_offset + gdt_count * kGdtEntrySize;
    for (std::uint32_t index = 0; index < group_count; ++index)
    {
        const std::uint32_t entry = gdt_offset + index * kGdtEntrySize;
        Dos16mBoundSegment segment;
        segment.selector = static_cast<std::uint16_t>(
            first_selector + index * kGdtEntrySize);
        segment.limit = Read16(data, entry);
        segment.access = data[entry + 5];
        const std::uint16_t reserved = Read16(data, entry + 6);
        segment.memory_paragraphs = reserved & ~kBssMarker;
        const std::uint32_t memory_size = std::max<std::uint32_t>(
            {static_cast<std::uint32_t>(segment.limit) + 1U,
             static_cast<std::uint32_t>(segment.memory_paragraphs) * 16U,
             1U});
        segment.image.assign(memory_size, 0);
        if ((reserved & kBssMarker) == 0)
        {
            const std::uint32_t file_size =
                static_cast<std::uint32_t>(segment.limit) + 1U;
            if (!RangeValid(data, copy_cursor, file_size) ||
                copy_cursor + file_size > module->next_header_file_offset)
            {
                return Fail(error, copy_cursor,
                            "DOS/16M segment copy exceeds module");
            }
            std::memcpy(segment.image.data(),
                        data.data() + copy_cursor,
                        file_size);
            copy_cursor = header_offset +
                Align16(copy_cursor + file_size - header_offset);
        }
        module->segments.push_back(std::move(segment));
    }

    const std::uint32_t relocation_entry =
        gdt_offset + group_count * kGdtEntrySize;
    const std::uint32_t relocation_size =
        static_cast<std::uint32_t>(Read16(data, relocation_entry)) + 1U;
    if (copy_cursor + relocation_size != module->next_header_file_offset)
    {
        return Fail(error, copy_cursor,
                    "DOS/16M RSI-2 range does not end at next header");
    }
    const std::uint32_t relocation_end = copy_cursor + relocation_size;
    bool terminal = false;
    while (copy_cursor + 4U <= relocation_end)
    {
        const std::uint16_t raw_selector = Read16(data, copy_cursor);
        const std::uint16_t count = Read16(data, copy_cursor + 2U);
        if (raw_selector == 0 && count == 0)
        {
            break;
        }
        copy_cursor += 4U;
        if (static_cast<std::uint64_t>(copy_cursor) + count * 2ULL >
            relocation_end)
        {
            return Fail(error, copy_cursor, "truncated DOS/16M RSI-2 block");
        }
        const std::uint16_t selector = raw_selector & ~0x0002U;
        Dos16mBoundSegment* segment = FindSegment(module, selector);
        if (segment == nullptr)
        {
            return Fail(error, copy_cursor, "RSI-2 targets unknown selector");
        }
        for (std::uint32_t index = 0; index < count; ++index)
        {
            const std::uint16_t offset =
                Read16(data, copy_cursor + index * 2U);
            if (offset + 2U > segment->image.size())
            {
                return Fail(error, copy_cursor + index * 2U,
                            "RSI-2 offset exceeds segment");
            }
            segment->selector_relocation_offsets.push_back(offset);
            ++module->relocation_count;
        }
        copy_cursor += count * 2U;
        if ((raw_selector & 0x0002U) != 0)
        {
            terminal = true;
            break;
        }
    }
    if (!terminal)
    {
        return Fail(error, copy_cursor, "RSI-2 stream has no terminator");
    }
    while (copy_cursor < relocation_end)
    {
        if (data[copy_cursor++] != 0)
        {
            return Fail(error, copy_cursor - 1,
                        "nonzero data after RSI-2 terminator");
        }
    }
    return true;
}

}  // namespace

bool ParseDos16mBoundModules(const std::vector<std::uint8_t>& data,
                             std::vector<Dos16mBoundModule>* modules,
                             ParseError* error)
{
    if (modules == nullptr)
    {
        return Fail(error, 0, "DOS/16M module result is null");
    }
    modules->clear();
    if (!RangeValid(data, 0, 0x1AU) || data[0] != 'M' || data[1] != 'Z')
    {
        return Fail(error, 0, "DOS/16M container has no MZ header");
    }
    const std::uint32_t last_page_bytes = Read16(data, 2);
    const std::uint32_t page_count = Read16(data, 4);
    if (page_count == 0)
    {
        return Fail(error, 4, "invalid MZ page count");
    }
    std::uint32_t cursor =
        (page_count - 1U) * 512U +
        (last_page_bytes != 0 ? last_page_bytes : 512U);
    if (cursor > data.size())
    {
        return Fail(error, cursor, "MZ declared size exceeds file");
    }
    while (cursor < data.size())
    {
        Dos16mBoundModule module;
        if (!ParseModule(data, cursor, &module, error))
        {
            modules->clear();
            return false;
        }
        cursor = module.next_header_file_offset;
        modules->push_back(std::move(module));
    }
    if (modules->empty())
    {
        return Fail(error, cursor, "DOS/16M container has no BW modules");
    }
    return true;
}

const Dos16mBoundModule* FindDos16mBoundModule(
    const std::vector<Dos16mBoundModule>& modules,
    const std::string& name)
{
    for (const Dos16mBoundModule& module : modules)
    {
        if (module.name == name)
        {
            return &module;
        }
    }
    return nullptr;
}

}  // namespace repiu::exe
