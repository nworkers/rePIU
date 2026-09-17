#ifndef REPIU_RUNTIME_GUEST_STACK_ACCESS_H_
#define REPIU_RUNTIME_GUEST_STACK_ACCESS_H_

#include "repiu/runtime/selector_table.h"

namespace repiu::runtime
{

struct GuestStackPushAccess
{
    std::uint32_t next_esp = 0;
    std::uint32_t linear_address = 0;
};

struct GuestStackReadAccess
{
    std::uint32_t next_esp = 0;
    std::uint32_t linear_address = 0;
};

bool ResolveGuestStackPushAccess(const GuestDescriptor& stack,
                                std::uint32_t esp,
                                std::uint32_t operand_bytes,
                                GuestStackPushAccess* access);

bool ResolveGuestStackReadAccess(const GuestDescriptor& stack,
                                std::uint32_t esp,
                                std::uint32_t operand_bytes,
                                GuestStackReadAccess* access);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_GUEST_STACK_ACCESS_H_
