#ifndef REPIU_RUNTIME_GUEST_CONTEXT_H_
#define REPIU_RUNTIME_GUEST_CONTEXT_H_

#include <cstdint>
#include <string>

namespace repiu::runtime
{

struct GuestRegisters
{
    std::uint32_t eax = 0;
    std::uint32_t ebx = 0;
    std::uint32_t ecx = 0;
    std::uint32_t edx = 0;
    std::uint32_t esi = 0;
    std::uint32_t edi = 0;
    std::uint32_t ebp = 0;
    std::uint32_t esp = 0;
};

struct GuestSegments
{
    std::uint16_t cs = 0;
    std::uint16_t ds = 0;
    std::uint16_t es = 0;
    std::uint16_t fs = 0;
    std::uint16_t gs = 0;
    std::uint16_t ss = 0;
};

struct GuestContext
{
    bool valid = false;
    GuestRegisters registers;
    GuestSegments segments;
    std::uint32_t eip = 0;
    std::uint32_t eflags = 0x00000202;
    std::string note;
};

struct GuestStackSwitchPlan
{
    bool valid = false;
    std::uint32_t entry_eip = 0;
    std::uint32_t stack_base = 0;
    std::uint32_t stack_limit = 0;
    std::uint32_t initial_esp = 0;
    std::uint32_t guard_bytes = 0;
    std::string message;
};

bool BuildGuestEntryContext(std::uint32_t entry_eip,
                            std::uint32_t initial_esp,
                            GuestContext* context);

bool BuildGuestStackSwitchPlan(std::uint32_t entry_eip,
                               std::uint32_t stack_base,
                               std::uint32_t stack_limit,
                               std::uint32_t initial_esp,
                               std::uint32_t guard_bytes,
                               GuestStackSwitchPlan* plan);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_GUEST_CONTEXT_H_
