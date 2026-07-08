#include "repiu/runtime/guest_context.h"

namespace repiu::runtime
{

bool BuildGuestEntryContext(std::uint32_t entry_eip,
                            std::uint32_t initial_esp,
                            GuestContext* context)
{
    if (context == nullptr)
    {
        return false;
    }

    *context = GuestContext{};
    context->eip = entry_eip;
    context->registers.esp = initial_esp;
    context->valid = entry_eip != 0 && initial_esp != 0;
    context->note = context->valid
                        ? "guest entry context is ready"
                        : "guest entry context requires entry and stack";
    return context->valid;
}

bool BuildGuestStackSwitchPlan(std::uint32_t entry_eip,
                               std::uint32_t stack_base,
                               std::uint32_t stack_limit,
                               std::uint32_t initial_esp,
                               std::uint32_t guard_bytes,
                               GuestStackSwitchPlan* plan)
{
    if (plan == nullptr)
    {
        return false;
    }

    *plan = GuestStackSwitchPlan{};
    plan->entry_eip = entry_eip;
    plan->stack_base = stack_base;
    plan->stack_limit = stack_limit;
    plan->initial_esp = initial_esp;
    plan->guard_bytes = guard_bytes;

    if (entry_eip == 0)
    {
        plan->message = "guest stack switch plan requires an entry EIP";
        return false;
    }

    if (stack_base >= stack_limit)
    {
        plan->message = "guest stack bounds are invalid";
        return false;
    }

    if (initial_esp <= stack_base || initial_esp > stack_limit)
    {
        plan->message = "guest ESP is outside stack bounds";
        return false;
    }

    if (initial_esp - stack_base <= guard_bytes)
    {
        plan->message = "guest ESP leaves no room above the guard area";
        return false;
    }

    plan->valid = true;
    plan->message = "guest stack switch plan is ready";
    return true;
}

}  // namespace repiu::runtime
