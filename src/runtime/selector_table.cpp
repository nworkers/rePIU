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

bool FindSelectorForLinearAddress(const SelectorTable& table,
                                  std::uint32_t linear_address,
                                  std::uint16_t* selector)
{
    if (selector == nullptr || !table.valid)
    {
        return false;
    }

    const GuestDescriptor* match = nullptr;
    for (const GuestDescriptor& descriptor : table.descriptors)
    {
        const std::uint64_t begin = descriptor.base;
        const std::uint64_t end = begin + descriptor.limit;
        if (!descriptor.present || linear_address < begin ||
            linear_address > end)
        {
            continue;
        }
        if (match != nullptr)
        {
            return false;
        }
        match = &descriptor;
    }
    if (match == nullptr)
    {
        return false;
    }
    *selector = match->selector;
    return true;
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

bool ResolveGuestFarReturn32Frame(
    const SelectorTable& table,
    std::uint16_t current_selector,
    std::uint32_t target_offset,
    std::uint32_t raw_selector_slot,
    GuestFarReturnResolution* resolution)
{
    if (resolution == nullptr)
    {
        return false;
    }
    *resolution = GuestFarReturnResolution{};

    const GuestDescriptor* current =
        FindDescriptor(table, current_selector);
    if (current == nullptr || !current->executable ||
        current->code_default_operand_size !=
            GuestCodeDefaultOperandSize::k16)
    {
        return false;
    }

    const std::uint16_t target_selector =
        static_cast<std::uint16_t>(raw_selector_slot & 0xFFFFU);
    const GuestDescriptor* target =
        FindDescriptor(table, target_selector);
    if (target == nullptr || !target->executable)
    {
        return false;
    }

    std::uint32_t target_linear = 0;
    if (!TranslateSelectorOffset(table, target_selector, target_offset, 1U,
                                 &target_linear))
    {
        // The observed LE wrapper stores a relocated linear return address in
        // this frame even though the accompanying CS is object-relative in
        // the runtime selector table. Accept that representation only when
        // the raw value is itself inside the target descriptor's mapped
        // linear window; this is not an unrestricted flat-address fallback.
        const std::uint64_t target_begin = target->base;
        const std::uint64_t target_end =
            target_begin + static_cast<std::uint64_t>(target->limit);
        if (static_cast<std::uint64_t>(target_offset) < target_begin ||
            static_cast<std::uint64_t>(target_offset) > target_end)
        {
            return false;
        }
        target_linear = target_offset;
    }

    resolution->valid = true;
    resolution->target_offset = target_offset;
    resolution->target_selector = target_selector;
    resolution->target_linear = target_linear;
    resolution->stack_bytes = kGuestFarReturn32FrameBytes;
    return true;
}

}  // namespace repiu::runtime
