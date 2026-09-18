#include "interrupt_return.h"

#include "guest_memory_access.h"
#include "repiu/runtime/selector_table.h"
#include "thread_context.h"

#include <cstdint>
#include <cstring>
#include <limits>

namespace repiu::engine
{
namespace
{

constexpr std::uint32_t kIretdFrameBytes = 12U;

bool ResolveReturnTarget(const runtime::SelectorTable& table,
                         const std::uint32_t target_eip,
                         const std::uint32_t raw_selector,
                         std::uint16_t* const target_selector,
                         std::uint32_t* const target_linear)
{
    if (target_selector == nullptr || target_linear == nullptr ||
        (raw_selector & 0xFFFF0000U) != 0U)
    {
        return false;
    }
    const std::uint16_t selector =
        static_cast<std::uint16_t>(raw_selector);
    const runtime::GuestDescriptor* descriptor =
        runtime::FindDescriptor(table, selector);
    if (descriptor == nullptr || !descriptor->executable ||
        descriptor->code_default_operand_size !=
            runtime::GuestCodeDefaultOperandSize::k32)
    {
        return false;
    }

    std::uint32_t linear = 0U;
    if (!runtime::TranslateSelectorOffset(
            table, selector, target_eip, 1U, &linear))
    {
        const std::uint64_t begin = descriptor->base;
        const std::uint64_t end = begin + descriptor->limit;
        if (target_eip < begin || target_eip > end)
        {
            return false;
        }
        linear = target_eip;
    }
    *target_selector = selector;
    *target_linear = linear;
    return true;
}

}  // namespace

std::optional<bool> HandleIretdInstruction(
    repiu::platform::GuestCpuContext* const registers,
    ThreadContext* const context)
{
    if (registers == nullptr || context == nullptr)
    {
        return std::nullopt;
    }

    std::uint16_t current_selector = 0U;
    if (!runtime::FindSelectorForLinearAddress(
            context->selector_table, registers->Eip, &current_selector))
    {
        return std::nullopt;
    }
    const runtime::GuestDescriptor* current =
        runtime::FindDescriptor(context->selector_table, current_selector);
    if (current == nullptr || !current->executable ||
        current->code_default_operand_size !=
            runtime::GuestCodeDefaultOperandSize::k32)
    {
        return std::nullopt;
    }

    const auto* const instruction = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(registers->Eip));
    if (!IsGuestRangeReadable(context, instruction, 1U))
    {
        return false;
    }
    if (instruction[0] != 0xCFU)
    {
        return std::nullopt;
    }
    if (registers->Esp >
        std::numeric_limits<std::uint32_t>::max() - kIretdFrameBytes)
    {
        return false;
    }

    const void* const frame = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(registers->Esp));
    if (!IsGuestRangeReadable(context, frame, kIretdFrameBytes))
    {
        return false;
    }
    std::uint32_t values[3] = {};
    std::memcpy(values, frame, sizeof(values));

    std::uint16_t target_selector = 0U;
    std::uint32_t target_linear = 0U;
    if (!ResolveReturnTarget(context->selector_table,
                             values[0], values[1],
                             &target_selector, &target_linear) ||
        !IsGuestRangeReadable(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(target_linear)),
            1U))
    {
        return false;
    }

    registers->Eip = target_linear;
    registers->SegCs = target_selector;
    registers->EFlags = values[2] | 0x00000002U;
    registers->Esp += kIretdFrameBytes;
    return true;
}

}  // namespace repiu::engine
