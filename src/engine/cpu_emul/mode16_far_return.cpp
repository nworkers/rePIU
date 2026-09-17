#include "mode16_far_return.h"

#include "execution_internal.h"
#include "guest_memory_access.h"
#include "repiu/runtime/guest_stack_access.h"
#include "repiu/runtime/selector_table.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace repiu::engine
{

std::optional<bool> HandleMode16FarReturn(
    repiu::platform::GuestCpuContext* registers, ThreadContext* context)
{
    if (registers == nullptr || context == nullptr)
    {
        return std::nullopt;
    }

    std::uint16_t current_selector =
        static_cast<std::uint16_t>(registers->SegCs & 0xFFFFU);
    const auto* current = runtime::FindDescriptor(
        context->selector_table, current_selector);
    const bool current_cs_covers_eip = current != nullptr &&
        static_cast<std::uint64_t>(registers->Eip) >= current->base &&
        static_cast<std::uint64_t>(registers->Eip) <=
            static_cast<std::uint64_t>(current->base) + current->limit;
    if (!current_cs_covers_eip &&
        !runtime::FindSelectorForLinearAddress(
            context->selector_table, registers->Eip, &current_selector))
    {
        return std::nullopt;
    }
    current = runtime::FindDescriptor(
        context->selector_table, current_selector);
    if (current == nullptr || !current->executable ||
        current->code_default_operand_size !=
            runtime::GuestCodeDefaultOperandSize::k16)
    {
        return std::nullopt;
    }

    const auto* instruction = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(registers->Eip));
    if (!IsGuestRangeReadable(context, instruction, 1U))
    {
        return false;
    }
    if (instruction[0] != 0xCBU)
    {
        return std::nullopt;
    }

    const auto* stack = runtime::FindDescriptor(
        context->selector_table, context->guest_ss);
    runtime::GuestStackReadAccess access;
    if (stack == nullptr || !runtime::ResolveGuestStackReadAccess(
            *stack,
            registers->Esp,
            runtime::kGuestFarReturn16FrameBytes,
            &access))
    {
        return false;
    }

    const void* frame = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(access.linear_address));
    if (!IsGuestRangeReadable(context, frame,
                              runtime::kGuestFarReturn16FrameBytes))
    {
        return false;
    }

    std::uint16_t target_offset = 0U;
    std::uint16_t target_selector = 0U;
    std::memcpy(&target_offset, frame, sizeof(target_offset));
    std::memcpy(&target_selector,
                static_cast<const std::uint8_t*>(frame) + 2U,
                sizeof(target_selector));

    runtime::GuestFarReturnResolution resolution;
    if (!runtime::ResolveGuestFarReturn16Frame(
            context->selector_table,
            current_selector,
            target_offset,
            target_selector,
            &resolution))
    {
        return false;
    }
    if (!IsGuestRangeReadable(
            context,
            reinterpret_cast<const void*>(static_cast<std::uintptr_t>(
                resolution.target_linear)),
            1U))
    {
        return false;
    }

    if (std::getenv("REPIU_LINUX_X64_RETURN_TRACE") != nullptr)
    {
        std::fprintf(stderr,
                     "[repiu-mode16-far-return] current_cs=0x%04X "
                     "target_ip=0x%04X target_cs=0x%04X "
                     "target=0x%08X esp=0x%08X new_esp=0x%08X\n",
                     static_cast<unsigned>(current_selector),
                     static_cast<unsigned>(target_offset),
                     static_cast<unsigned>(target_selector),
                     static_cast<unsigned>(resolution.target_linear),
                     static_cast<unsigned>(registers->Esp),
                     static_cast<unsigned>(access.next_esp));
    }
    registers->Eip = resolution.target_linear;
    registers->SegCs = resolution.target_selector;
    registers->Esp = access.next_esp;
    return true;
}

}  // namespace repiu::engine
