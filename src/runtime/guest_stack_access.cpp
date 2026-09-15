#include "repiu/runtime/guest_stack_access.h"

#include <limits>

namespace repiu::runtime
{

bool ResolveGuestStackPushAccess(const GuestDescriptor& stack,
                                std::uint32_t esp,
                                std::uint32_t operand_bytes,
                                GuestStackPushAccess* access)
{
    if (access == nullptr)
    {
        return false;
    }
    *access = {};
    // Require present, writable, expand-up data. The flags use the DPMI
    // access-rights layout; bit 14 is the stack descriptor's B bit.
    if (!stack.present || (stack.flags & 0x9EU) != 0x92U ||
        (operand_bytes != 2U && operand_bytes != 4U))
    {
        return false;
    }
    const bool big = (stack.flags & 0x4000U) != 0U;
    const std::uint32_t next_esp = big
        ? esp - operand_bytes
        : (esp & 0xFFFF0000U) | ((esp - operand_bytes) & 0xFFFFU);
    const std::uint32_t offset = big ? next_esp : next_esp & 0xFFFFU;
    const std::uint64_t last_offset =
        static_cast<std::uint64_t>(offset) + operand_bytes - 1U;
    const std::uint64_t linear =
        static_cast<std::uint64_t>(stack.base) + offset;
    if (last_offset > stack.limit ||
        linear + operand_bytes - 1U > 0xFFFFFFFFULL)
    {
        return false;
    }
    access->next_esp = next_esp;
    access->linear_address = static_cast<std::uint32_t>(linear);
    return true;
}

bool ResolveGuestStackReadAccess(const GuestDescriptor& stack,
                                std::uint32_t esp,
                                std::uint32_t operand_bytes,
                                GuestStackReadAccess* access)
{
    if (access == nullptr)
    {
        return false;
    }
    *access = {};
    if (!stack.present || (stack.flags & 0x9EU) != 0x92U ||
        (operand_bytes != 2U && operand_bytes != 4U))
    {
        return false;
    }

    const bool big = (stack.flags & 0x4000U) != 0U;
    const std::uint32_t offset = big ? esp : esp & 0xFFFFU;
    const std::uint64_t last_offset =
        static_cast<std::uint64_t>(offset) + operand_bytes - 1U;
    const std::uint64_t linear =
        static_cast<std::uint64_t>(stack.base) + offset;
    if (last_offset > stack.limit ||
        linear + operand_bytes - 1U >
            std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }

    std::uint32_t next_esp = 0U;
    if (big)
    {
        if (esp > std::numeric_limits<std::uint32_t>::max() - operand_bytes)
        {
            return false;
        }
        next_esp = esp + operand_bytes;
    }
    else
    {
        next_esp = (esp & 0xFFFF0000U) |
            ((esp + operand_bytes) & 0xFFFFU);
    }
    access->next_esp = next_esp;
    access->linear_address = static_cast<std::uint32_t>(linear);
    return true;
}

}  // namespace repiu::runtime
