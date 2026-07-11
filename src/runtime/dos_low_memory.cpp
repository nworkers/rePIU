#include "repiu/runtime/dos_low_memory.h"

namespace repiu::runtime
{
namespace
{

bool IsDosLowMemoryRangeValid(std::uint32_t address,
                              std::uint32_t byte_count)
{
    const std::uint64_t end =
        static_cast<std::uint64_t>(address) + byte_count;
    return byte_count != 0 && end <= kDosLowMemorySize;
}

}  // namespace

bool InitializeDosLowMemory(DosLowMemory* memory)
{
    if (memory == nullptr)
    {
        return false;
    }

    *memory = DosLowMemory{};
    memory->valid = true;
    return true;
}

bool ReadDosLowMemoryUInt8(const DosLowMemory& memory,
                           std::uint32_t address,
                           std::uint8_t* value)
{
    if (!memory.valid || value == nullptr ||
        !IsDosLowMemoryRangeValid(address, 1))
    {
        return false;
    }
    *value = memory.bytes[address];
    return true;
}

bool ReadDosLowMemoryUInt16(const DosLowMemory& memory,
                            std::uint32_t address,
                            std::uint16_t* value)
{
    if (!memory.valid || value == nullptr ||
        !IsDosLowMemoryRangeValid(address, 2))
    {
        return false;
    }
    *value = static_cast<std::uint16_t>(memory.bytes[address]) |
        (static_cast<std::uint16_t>(memory.bytes[address + 1]) << 8);
    return true;
}

bool ReadDosLowMemoryUInt32(const DosLowMemory& memory,
                            std::uint32_t address,
                            std::uint32_t* value)
{
    if (!memory.valid || value == nullptr ||
        !IsDosLowMemoryRangeValid(address, 4))
    {
        return false;
    }
    *value = static_cast<std::uint32_t>(memory.bytes[address]) |
        (static_cast<std::uint32_t>(memory.bytes[address + 1]) << 8) |
        (static_cast<std::uint32_t>(memory.bytes[address + 2]) << 16) |
        (static_cast<std::uint32_t>(memory.bytes[address + 3]) << 24);
    return true;
}

bool WriteDosLowMemory(DosLowMemory* memory,
                       std::uint32_t address,
                       std::uint32_t value,
                       std::uint32_t byte_count)
{
    if (memory == nullptr || !memory->valid || byte_count > 4 ||
        !IsDosLowMemoryRangeValid(address, byte_count))
    {
        return false;
    }
    for (std::uint32_t index = 0; index < byte_count; ++index)
    {
        memory->bytes[address + index] = static_cast<std::uint8_t>(
            (value >> (index * 8)) & 0xFFU);
    }
    return true;
}

}  // namespace repiu::runtime
