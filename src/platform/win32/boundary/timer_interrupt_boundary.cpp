#include "timer_interrupt_boundary.h"

#include "execution_internal.h"
#include "guest_memory_access.h"
#include "thread_context.h"

#include <cstdint>

namespace repiu::platform::win32
{
namespace
{

std::uint32_t ReadLittleEndian32(const std::uint8_t* bytes)
{
    return static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1]) << 8U) |
        (static_cast<std::uint32_t>(bytes[2]) << 16U) |
        (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

std::uint16_t ReadLittleEndian16(const std::uint8_t* bytes)
{
    return static_cast<std::uint16_t>(bytes[0]) |
        static_cast<std::uint16_t>(bytes[1] << 8U);
}

} // namespace

bool HandleTimerInterruptChainBoundary(_CONTEXT* win32_context,
                                       ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr ||
        !context->dpmi_interrupt_vectors[0x08].valid)
    {
        return false;
    }

    const auto* instruction = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(win32_context->Eip));
    constexpr std::size_t kFarCallSize = 6U;
    if (!IsGuestRangeReadable(context, instruction - 1, kFarCallSize + 1U) ||
        instruction[-1] != 0x9CU || instruction[0] != 0xFFU ||
        instruction[1] != 0x1DU)
    {
        return false;
    }

    const std::uint32_t pointer_address = ReadLittleEndian32(instruction + 2);
    const auto* pointer = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(pointer_address));
    if (!IsGuestRangeReadable(context, pointer, kFarCallSize))
    {
        return false;
    }

    const std::uint32_t target_offset = ReadLittleEndian32(pointer);
    const std::uint16_t target_selector = ReadLittleEndian16(pointer + 4);
    if (target_offset != 0U || target_selector == 0U ||
        target_selector != static_cast<std::uint16_t>(win32_context->SegDs))
    {
        return false;
    }

    // The guest executed PUSHFD immediately before CALL FAR. A real chained
    // IRQ0 handler returns with IRET, consuming the far-call return frame and
    // that saved EFLAGS. Since the call has not executed, discard only the
    // already-pushed EFLAGS and continue after the call instruction.
    win32_context->Esp += sizeof(std::uint32_t);
    win32_context->Eip += kFarCallSize;

    ++context->timer_interrupt_chain_hle_count;
    context->timer_interrupt_chain_hle_source =
        static_cast<std::uint32_t>(win32_context->Eip - kFarCallSize);
    context->timer_interrupt_chain_hle_pointer = pointer_address;
    context->timer_interrupt_chain_hle_offset = target_offset;
    context->timer_interrupt_chain_hle_selector = target_selector;
    return true;
}

} // namespace repiu::platform::win32
