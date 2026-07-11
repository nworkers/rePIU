#include "repiu/runtime/selector_table.h"

namespace repiu::runtime
{

bool InitializeSelectorAllocator(SelectorAllocator* allocator,
                                 std::uint16_t first_selector)
{
    if (allocator == nullptr || first_selector == 0 ||
        (first_selector & 0x07U) != 0x04U)
    {
        return false;
    }

    *allocator = SelectorAllocator{};
    allocator->valid = true;
    allocator->next_selector = first_selector;
    return true;
}

bool AllocateSelector(SelectorAllocator* allocator,
                      std::uint16_t* selector)
{
    if (allocator == nullptr || selector == nullptr ||
        !allocator->valid || allocator->next_selector == 0 ||
        allocator->next_selector > 0xFFF4U)
    {
        return false;
    }

    *selector = allocator->next_selector;
    allocator->next_selector =
        static_cast<std::uint16_t>(allocator->next_selector + 8U);
    ++allocator->allocated_count;
    return true;
}

bool InitializeSelectorTable(SelectorTable* table)
{
    if (table == nullptr)
    {
        return false;
    }

    *table = SelectorTable{};
    table->valid = true;
    table->message = "selector table is initialized";
    return true;
}

bool RegisterDescriptor(SelectorTable* table,
                        const GuestDescriptor& descriptor)
{
    if (table == nullptr || !table->valid || descriptor.selector == 0)
    {
        return false;
    }

    for (GuestDescriptor& existing : table->descriptors)
    {
        if (existing.selector == descriptor.selector)
        {
            existing = descriptor;
            table->message = "selector descriptor was replaced";
            return true;
        }
    }

    table->descriptors.push_back(descriptor);
    table->message = "selector descriptor was registered";
    return true;
}

const GuestDescriptor* FindDescriptor(const SelectorTable& table,
                                      std::uint16_t selector)
{
    if (!table.valid || selector == 0)
    {
        return nullptr;
    }

    for (const GuestDescriptor& descriptor : table.descriptors)
    {
        if (descriptor.selector == selector && descriptor.present)
        {
            return &descriptor;
        }
    }

    return nullptr;
}

bool TranslateSelectorOffset(const SelectorTable& table,
                             std::uint16_t selector,
                             std::uint32_t offset,
                             std::uint32_t byte_count,
                             std::uint32_t* linear_address)
{
    if (linear_address == nullptr || byte_count == 0)
    {
        return false;
    }
    const GuestDescriptor* descriptor = FindDescriptor(table, selector);
    if (descriptor == nullptr)
    {
        return false;
    }
    const std::uint64_t last_offset =
        static_cast<std::uint64_t>(offset) + byte_count - 1U;
    const std::uint64_t linear =
        static_cast<std::uint64_t>(descriptor->base) + offset;
    if (last_offset > descriptor->limit || linear > 0xFFFFFFFFULL ||
        linear + byte_count - 1U > 0xFFFFFFFFULL)
    {
        return false;
    }
    *linear_address = static_cast<std::uint32_t>(linear);
    return true;
}

}  // namespace repiu::runtime
