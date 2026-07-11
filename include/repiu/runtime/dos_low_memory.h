#ifndef REPIU_RUNTIME_DOS_LOW_MEMORY_H_
#define REPIU_RUNTIME_DOS_LOW_MEMORY_H_

#include <array>
#include <cstdint>

namespace repiu::runtime
{

constexpr std::uint32_t kDosLowMemorySize = 0x00010000U;

struct DosLowMemory
{
    bool valid = false;
    std::array<std::uint8_t, kDosLowMemorySize> bytes = {};
};

bool InitializeDosLowMemory(DosLowMemory* memory);

bool ReadDosLowMemoryUInt8(const DosLowMemory& memory,
                           std::uint32_t address,
                           std::uint8_t* value);

bool ReadDosLowMemoryUInt16(const DosLowMemory& memory,
                            std::uint32_t address,
                            std::uint16_t* value);

bool ReadDosLowMemoryUInt32(const DosLowMemory& memory,
                            std::uint32_t address,
                            std::uint32_t* value);

bool WriteDosLowMemory(DosLowMemory* memory,
                       std::uint32_t address,
                       std::uint32_t value,
                       std::uint32_t byte_count);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_DOS_LOW_MEMORY_H_
