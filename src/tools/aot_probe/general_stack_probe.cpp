#include "general_stack_probe.h"

#include "instruction_emulation.h"
#include "repiu/platform/virtual_memory.h"

#include <cstdint>
#include <cstring>
#include <iostream>

namespace repiu::tools
{

bool RunGeneralStackProbe()
{
    constexpr std::uintptr_t kRequestedBase = 0x18000000U;
    constexpr std::size_t kArenaSize = 0x1000U;
    constexpr std::uint32_t kCodeOffset = 0x100U;
    constexpr std::uint32_t kStackOffset = 0x900U;
    void* const requested = reinterpret_cast<void*>(kRequestedBase);
    const repiu::platform::MemoryReservation reservation =
        repiu::platform::ReserveMemory(
            requested,
            kArenaSize,
            true,
            repiu::platform::MemoryProtection::kExecuteReadWrite);
    if (!reservation.valid || reservation.base != requested)
    {
        if (reservation.valid && reservation.base != nullptr)
        {
            repiu::platform::ReleaseMemory(reservation.base, kArenaSize);
        }
        std::cout << "general_stack_arena=false\n";
        return false;
    }

    auto* const bytes = static_cast<std::uint8_t*>(reservation.base);
    auto set_opcode = [&](std::uint8_t opcode) {
        bytes[kCodeOffset] = opcode;
    };
    repiu::engine::ThreadContext context;
    context.runtime_base = static_cast<std::uint32_t>(kRequestedBase);
    context.runtime_size = static_cast<std::uint32_t>(kArenaSize);
    repiu::platform::GuestCpuContext cpu{};
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kStackOffset);
    cpu.Ebx = 0x11223344U;

    set_opcode(0x53U);
    const bool push_ebx =
        repiu::engine::HandleGeneralRegisterStackInstruction(&cpu, &context);
    std::uint32_t stored = 0U;
    std::memcpy(&stored, bytes + kStackOffset - 4U, sizeof(stored));
    const bool ordinary_push = push_ebx && stored == 0x11223344U &&
        cpu.Esp == kRequestedBase + kStackOffset - 4U &&
        cpu.Eip == kRequestedBase + kCodeOffset + 1U;

    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    set_opcode(0x59U);
    const bool pop_ecx =
        repiu::engine::HandleGeneralRegisterStackInstruction(&cpu, &context);
    const bool ordinary_pop = pop_ecx && cpu.Ecx == 0x11223344U &&
        cpu.Esp == kRequestedBase + kStackOffset &&
        cpu.Eip == kRequestedBase + kCodeOffset + 1U;

    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kStackOffset);
    set_opcode(0x54U);
    const bool push_esp =
        repiu::engine::HandleGeneralRegisterStackInstruction(&cpu, &context);
    std::memcpy(&stored, bytes + kStackOffset - 4U, sizeof(stored));
    const bool push_esp_order = push_esp &&
        stored == kRequestedBase + kStackOffset &&
        cpu.Esp == kRequestedBase + kStackOffset - 4U;

    const std::uint32_t popped_esp =
        static_cast<std::uint32_t>(kRequestedBase + kStackOffset + 0x40U);
    std::memcpy(bytes + kStackOffset - 4U, &popped_esp, sizeof(popped_esp));
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    set_opcode(0x5CU);
    const bool pop_esp =
        repiu::engine::HandleGeneralRegisterStackInstruction(&cpu, &context);
    const bool pop_esp_order = pop_esp && cpu.Esp == popped_esp;

    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase);
    set_opcode(0x53U);
    const bool rejected =
        !repiu::engine::HandleGeneralRegisterStackInstruction(&cpu, &context) &&
        cpu.Esp == kRequestedBase &&
        cpu.Eip == kRequestedBase + kCodeOffset;

    const std::uint8_t sequence[] = {
        0x53U, 0x06U, 0x0FU, 0xA0U, 0x90U};
    std::memcpy(bytes + kCodeOffset, sequence, sizeof(sequence));
    context.enable_segment_load_hle = true;
    context.guest_es = 0x0024U;
    context.guest_fs = 0x0034U;
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kStackOffset);
    cpu.Ebx = 0x11223344U;
    const std::uint32_t sequence_count =
        repiu::engine::HandleConsecutiveLegacyStackInstructions(
            &cpu, &context, 16U);
    std::uint32_t sequence_values[3]{};
    std::memcpy(sequence_values, bytes + kStackOffset - 12U,
                sizeof(sequence_values));
    const bool mixed_sequence = sequence_count == 3U &&
        cpu.Eip == kRequestedBase + kCodeOffset + 4U &&
        cpu.Esp == kRequestedBase + kStackOffset - 12U &&
        sequence_values[0] == 0x00000034U &&
        sequence_values[1] == 0x00000024U &&
        sequence_values[2] == 0x11223344U;

    const std::uint8_t bounded_sequence[] = {0x53U, 0x53U, 0x53U};
    std::memcpy(bytes + kCodeOffset, bounded_sequence,
                sizeof(bounded_sequence));
    cpu.Eip = static_cast<std::uint32_t>(kRequestedBase + kCodeOffset);
    cpu.Esp = static_cast<std::uint32_t>(kRequestedBase + kStackOffset);
    const std::uint32_t bounded_count =
        repiu::engine::HandleConsecutiveLegacyStackInstructions(
            &cpu, &context, 2U);
    const bool bounded = bounded_count == 2U &&
        cpu.Eip == kRequestedBase + kCodeOffset + 2U &&
        cpu.Esp == kRequestedBase + kStackOffset - 8U;

    const bool released =
        repiu::platform::ReleaseMemory(reservation.base, kArenaSize);
    const bool all = ordinary_push && ordinary_pop && push_esp_order &&
        pop_esp_order && rejected && mixed_sequence && bounded && released;
    std::cout << "general_stack_push=" << (ordinary_push ? "true" : "false")
              << ",pop=" << (ordinary_pop ? "true" : "false")
              << ",push_esp=" << (push_esp_order ? "true" : "false")
              << ",pop_esp=" << (pop_esp_order ? "true" : "false")
              << ",range_rejected=" << (rejected ? "true" : "false")
              << ",mixed_sequence=" << (mixed_sequence ? "true" : "false")
              << ",bounded=" << (bounded ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
