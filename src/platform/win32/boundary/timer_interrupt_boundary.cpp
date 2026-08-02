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

void ArmAotTimerSafePoint(ThreadContext* context)
{
    if (context == nullptr || context->aot_placement == nullptr ||
        !context->aot_placement->timer_safe_points_enabled ||
        context->aot_placement->timer_safe_point_cache_offsets.empty())
    {
        return;
    }
    InterlockedExchange(
        reinterpret_cast<volatile LONG*>(
            &context->aot_placement->timer_safe_point_request),
        1L);
}

void ClearAotTimerSafePointRequest(ThreadContext* context)
{
    if (context == nullptr || context->aot_placement == nullptr)
    {
        return;
    }
    InterlockedExchange(
        reinterpret_cast<volatile LONG*>(
            &context->aot_placement->timer_safe_point_request),
        0L);
}

bool HandleAotTimerSafePoint(_EXCEPTION_POINTERS* exception_info,
                             _CONTEXT* win32_context,
                             ThreadContext* context)
{
    if (exception_info == nullptr || exception_info->ExceptionRecord == nullptr ||
        win32_context == nullptr || context == nullptr ||
        context->aot_placement == nullptr ||
        exception_info->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT)
    {
        return false;
    }
    Win32AotCodeCachePlacement* placement = context->aot_placement;
    const std::uintptr_t exception_address = reinterpret_cast<std::uintptr_t>(
        exception_info->ExceptionRecord->ExceptionAddress);
    if (exception_address < placement->base_address ||
        exception_address >= placement->base_address + placement->size)
    {
        return false;
    }
    const std::uint32_t cache_offset = static_cast<std::uint32_t>(
        exception_address - placement->base_address);
    if (placement->timer_safe_point_cache_offsets.find(cache_offset) ==
        placement->timer_safe_point_cache_offsets.end())
    {
        return false;
    }
    const auto source =
        placement
            ->timer_safe_point_guest_source_by_breakpoint_offset.find(
                cache_offset);
    const std::uint32_t guest_source =
        source !=
                placement
                    ->timer_safe_point_guest_source_by_breakpoint_offset.end()
            ? source->second
            : 0U;

    ClearAotTimerSafePointRequest(context);
    InterlockedIncrement(reinterpret_cast<volatile LONG*>(
        &placement->timer_safe_point_trap_count));
    // Win32 reports the breakpoint at the INT3 byte and leaves EIP there for
    // this cache-origin trap. Resume at the translated branch immediately
    // after it; if INT 8 is injected, this cache address becomes the IRETD
    // return address instead.
    const std::uint32_t resume_eip =
        static_cast<std::uint32_t>(exception_address + 1U);
    win32_context->Eip = resume_eip;
    const std::uint32_t attributed_ticks =
        InjectPendingInterrupts(win32_context, context);
    const bool injected =
        static_cast<std::uint32_t>(win32_context->Eip) != resume_eip;
    if (injected)
    {
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(
            &placement->timer_safe_point_injected_count));
    }
    else
    {
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(
            &placement->timer_safe_point_deferred_count));
    }
    RecordAotTimerSourceEvent(
        &placement->timer_source_profile,
        guest_source,
        context->last_timer_injection_ticks.load(
            std::memory_order_acquire),
        injected,
        attributed_ticks);
    return true;
}

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
    // The saved "previous handler" only chains to real code when its selector is
    // the guest code selector. What the guest actually saved for a vector it
    // never installed is whatever AH=35h reported, and that shape differs per
    // title: pumpit1 stored DS:00000000, while pumpit3's wrapper widens EBX to
    // 32 bits and stores 0000:<stale high half>. Neither designates executable
    // code, so match on the selector alone rather than on a per-title offset.
    if (target_selector == static_cast<std::uint16_t>(win32_context->SegCs))
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
