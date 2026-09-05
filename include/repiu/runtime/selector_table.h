#ifndef REPIU_RUNTIME_SELECTOR_TABLE_H_
#define REPIU_RUNTIME_SELECTOR_TABLE_H_

#include <cstdint>
#include <string>
#include <vector>

namespace repiu::runtime
{

constexpr std::uint32_t kLeObjectExecutable = 0x00000004U;
constexpr std::uint32_t kLeObjectBigDefault = 0x00002000U;
constexpr std::uint32_t kGuestFarReturn32FrameBytes = 8U;

enum class GuestCodeDefaultOperandSize : std::uint8_t
{
    kUnknown = 0,
    k16 = 16,
    k32 = 32,
};

struct GuestDescriptor
{
    std::uint16_t selector = 0;
    std::uint32_t base = 0;
    std::uint32_t limit = 0;
    std::uint32_t flags = 0;
    bool present = false;
    std::uint32_t object_flags = 0;
    bool executable = false;
    GuestCodeDefaultOperandSize code_default_operand_size =
        GuestCodeDefaultOperandSize::kUnknown;
};

struct SelectorTable
{
    bool valid = false;
    std::vector<GuestDescriptor> descriptors;
    std::string message;
};

struct SelectorAllocator
{
    bool valid = false;
    std::uint16_t next_selector = 0;
    std::uint32_t allocated_count = 0;
};

bool InitializeSelectorAllocator(SelectorAllocator* allocator,
                                 std::uint16_t first_selector);

bool AllocateSelector(SelectorAllocator* allocator,
                      std::uint16_t* selector);

bool InitializeSelectorTable(SelectorTable* table);

bool RegisterDescriptor(SelectorTable* table,
                        const GuestDescriptor& descriptor);

const GuestDescriptor* FindDescriptor(const SelectorTable& table,
                                      std::uint16_t selector);

bool FindSelectorForLinearAddress(const SelectorTable& table,
                                  std::uint32_t linear_address,
                                  std::uint16_t* selector);

bool TranslateSelectorOffset(const SelectorTable& table,
                             std::uint16_t selector,
                             std::uint32_t offset,
                             std::uint32_t byte_count,
                             std::uint32_t* linear_address);

struct GuestFarReturnResolution
{
    bool valid = false;
    std::uint32_t target_offset = 0;
    std::uint16_t target_selector = 0;
    std::uint32_t target_linear = 0;
    std::uint32_t stack_bytes = 0;
};

bool ResolveGuestFarReturn32Frame(
    const SelectorTable& table,
    std::uint16_t current_selector,
    std::uint32_t target_offset,
    std::uint32_t raw_selector_slot,
    GuestFarReturnResolution* resolution);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_SELECTOR_TABLE_H_
