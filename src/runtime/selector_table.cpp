#include "repiu/runtime/selector_table.h"

namespace repiu::runtime
{

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

}  // namespace repiu::runtime
