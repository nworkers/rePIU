#include "repiu/engine/glide_gate_interrupt_exit.h"

#include "repiu/platform/virtual_memory.h"

#include <cstring>
#include <limits>

namespace repiu::engine
{
namespace
{

GlideGateInterruptExitSlots g_slots;

bool FitsIn32Bits(std::uintptr_t value)
{
    return value <= std::numeric_limits<std::uint32_t>::max();
}

void WriteAbsoluteOperand(std::uint8_t* code, std::uint8_t opcode,
                          std::uint8_t modrm, std::uint32_t address)
{
    code[0] = opcode;
    code[1] = modrm;
    std::memcpy(code + 2, &address, sizeof(address));
}

std::uint32_t PlaceInterruptExit()
{
    const std::uintptr_t eip_address =
        reinterpret_cast<std::uintptr_t>(&g_slots.frame_eip);
    const std::uintptr_t target_address =
        reinterpret_cast<std::uintptr_t>(&g_slots.target);
    if (!FitsIn32Bits(eip_address) || !FitsIn32Bits(target_address))
    {
        return 0U;
    }
    const std::size_t page = repiu::platform::SystemPageSize();
    const repiu::platform::MemoryReservation reservation =
        repiu::platform::ReserveMemory(
            nullptr, page, true,
            repiu::platform::MemoryProtection::kReadWrite);
    if (!reservation.valid || reservation.base == nullptr)
    {
        return 0U;
    }
    const std::uintptr_t base =
        reinterpret_cast<std::uintptr_t>(reservation.base);
    if (!FitsIn32Bits(base + kGlideGateInterruptExitSize))
    {
        repiu::platform::ReleaseMemory(reservation.base, page);
        return 0U;
    }
    EncodeGlideGateInterruptExit(
        static_cast<std::uint8_t*>(reservation.base),
        static_cast<std::uint32_t>(eip_address),
        static_cast<std::uint32_t>(target_address));
    if (!repiu::platform::ProtectMemory(
            reservation.base, page,
            repiu::platform::MemoryProtection::kExecuteRead, nullptr))
    {
        repiu::platform::ReleaseMemory(reservation.base, page);
        return 0U;
    }
    repiu::platform::FlushInstructionCacheRange(reservation.base,
                                                kGlideGateInterruptExitSize);
    return static_cast<std::uint32_t>(base);
}

}  // namespace

void EncodeGlideGateInterruptExit(std::uint8_t* code,
                                  std::uint32_t frame_eip_address,
                                  std::uint32_t target_address)
{
    if (code == nullptr)
    {
        return;
    }
    // push cs.
    code[0] = 0x0EU;
    // push dword ptr [abs32] is FF /6 with mod=00 rm=101: FF 35.
    WriteAbsoluteOperand(code + 1, 0xFFU, 0x35U, frame_eip_address);
    // jmp dword ptr [abs32] is FF /4 with mod=00 rm=101: FF 25.
    WriteAbsoluteOperand(code + 7, 0xFFU, 0x25U, target_address);
}

void ArrangeGlideGateInterruptExit(std::uint32_t* frame8,
                                   std::uint32_t entry_eflags,
                                   std::uint32_t exit_address,
                                   std::uint32_t target,
                                   GlideGateInterruptExitSlots* slots)
{
    if (frame8 == nullptr || slots == nullptr)
    {
        return;
    }
    slots->frame_eip = frame8[0];
    slots->target = target;
    frame8[0] = entry_eflags;
    frame8[1] = exit_address;
}

GlideGateInterruptExitSlots* GlideGateInterruptExitSlotStorage()
{
    return &g_slots;
}

std::uint32_t GlideGateInterruptExitAddress()
{
    static const std::uint32_t address = PlaceInterruptExit();
    return address;
}

}  // namespace repiu::engine
