#ifndef REPIU_RUNTIME_SELECTOR_TABLE_H_
#define REPIU_RUNTIME_SELECTOR_TABLE_H_

#include <cstdint>
#include <string>
#include <vector>

namespace repiu::runtime
{

struct GuestDescriptor
{
    std::uint16_t selector = 0;
    std::uint32_t base = 0;
    std::uint32_t limit = 0;
    std::uint32_t flags = 0;
    bool present = false;
};

struct SelectorTable
{
    bool valid = false;
    std::vector<GuestDescriptor> descriptors;
    std::string message;
};

bool InitializeSelectorTable(SelectorTable* table);

bool RegisterDescriptor(SelectorTable* table,
                        const GuestDescriptor& descriptor);

const GuestDescriptor* FindDescriptor(const SelectorTable& table,
                                      std::uint16_t selector);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_SELECTOR_TABLE_H_
