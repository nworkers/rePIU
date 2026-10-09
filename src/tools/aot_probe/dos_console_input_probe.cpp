#include "dos_console_input_probe.h"

#include "cpu_emul/instruction_emulation.h"
#include "dos/dos_int21_services.h"
#include "execution/thread_context.h"
#include "repiu/platform/guest_cpu_context.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>

namespace repiu::tools
{
namespace
{

// The traced dispatcher reads the guest's bytes at EIP, which has to be a
// 32-bit address. On a 64-bit host a local buffer is not one, so that part of
// the probe runs only where the host's pointers are 32 bits wide; the common
// dispatcher never dereferences EIP, so the rest runs everywhere.
constexpr bool kTracedDispatcherReachable = sizeof(void*) == 4U;

// The `int 21h` the guest sits on, so the traced dispatcher's opcode check and
// the readable-range check both have real bytes to look at.
struct ProbeGuest
{
    std::unique_ptr<repiu::engine::ThreadContext> context =
        std::make_unique<repiu::engine::ThreadContext>();
    std::array<std::uint8_t, 16> memory = {};

    ProbeGuest()
    {
        memory[0] = 0xCDU;
        memory[1] = 0x21U;
        context->runtime_base = kTracedDispatcherReachable
            ? static_cast<std::uint32_t>(
                  reinterpret_cast<std::uintptr_t>(memory.data()))
            : 0x00010000U;
        context->runtime_size = static_cast<std::uint32_t>(memory.size());
    }

    repiu::platform::GuestCpuContext MakeCpu(std::uint8_t ah) const
    {
        repiu::platform::GuestCpuContext cpu = {};
        cpu.Eax = static_cast<std::uint32_t>(ah) << 8U | 0xFFU;
        cpu.Eip = context->runtime_base;
        cpu.EFlags = 1U;
        return cpu;
    }
};

bool Push(repiu::engine::ThreadContext* context, std::uint16_t legacy_ax,
          std::uint16_t enhanced_ax)
{
    repiu::hle::BiosKeystroke keystroke;
    keystroke.legacy_ax = legacy_ax;
    keystroke.enhanced_ax = enhanced_ax;
    return context->bios_keyboard.Push(keystroke);
}

std::uint8_t Al(const repiu::platform::GuestCpuContext& cpu)
{
    return static_cast<std::uint8_t>(cpu.Eax & 0xFFU);
}

}  // namespace

bool RunDosConsoleInputProbe()
{
    // An ordinary key: 'a' with scan code 0x1E. AL gets the character, the
    // upper bytes of EAX are left alone, and EIP moves past the `int 21h`.
    bool ordinary = false;
    {
        ProbeGuest guest;
        auto cpu = guest.MakeCpu(0x08U);
        cpu.Eax = 0x12340800U | 0xFFU;
        ordinary = Push(guest.context.get(), 0x1E61U, 0x1E61U) &&
            repiu::engine::HandleDosInterrupt21(&cpu, guest.context.get()) &&
            Al(cpu) == 0x61U && (cpu.Eax & 0xFFFFFF00U) == 0x12340800U &&
            cpu.Eip == guest.context->runtime_base + 2U &&
            guest.context->dos_console_input_count == 1U &&
            guest.context->dos_console_input_wait_count == 0U &&
            !guest.context->dos_console_pending_scan_code_valid;
    }

    // An extended key: up arrow, legacy AX 0x4800. The first call returns 0
    // and holds the scan code; the second returns 0x48 without touching the
    // buffer; the third finds the buffer empty.
    bool extended = false;
    {
        ProbeGuest guest;
        auto first = guest.MakeCpu(0x08U);
        extended = Push(guest.context.get(), 0x4800U, 0x48E0U) &&
            repiu::engine::HandleDosInterrupt21(&first, guest.context.get()) &&
            Al(first) == 0U &&
            first.Eip == guest.context->runtime_base + 2U &&
            guest.context->dos_console_pending_scan_code_valid &&
            guest.context->dos_console_pending_scan_code == 0x48U;
        auto second = guest.MakeCpu(0x08U);
        extended = extended &&
            repiu::engine::HandleDosInterrupt21(&second, guest.context.get()) &&
            Al(second) == 0x48U &&
            second.Eip == guest.context->runtime_base + 2U &&
            !guest.context->dos_console_pending_scan_code_valid &&
            guest.context->dos_console_input_count == 2U;
        auto third = guest.MakeCpu(0x08U);
        extended = extended &&
            repiu::engine::HandleDosInterrupt21(&third, guest.context.get()) &&
            third.Eip == guest.context->runtime_base &&
            guest.context->dos_console_input_wait_count == 1U;
    }

    // Nothing queued: the call succeeds, EIP stays on the `int 21h` so the
    // guest asks again, AL is untouched and the wait is counted. Then a key
    // arrives and the same call site is satisfied.
    bool waits = false;
    {
        ProbeGuest guest;
        auto cpu = guest.MakeCpu(0x08U);
        waits = repiu::engine::HandleDosInterrupt21(&cpu, guest.context.get()) &&
            cpu.Eip == guest.context->runtime_base && Al(cpu) == 0xFFU &&
            guest.context->dos_console_input_wait_count == 1U &&
            guest.context->dos_console_input_count == 0U;
        waits = waits && Push(guest.context.get(), 0x1C0DU, 0x1C0DU) &&
            repiu::engine::HandleDosInterrupt21(&cpu, guest.context.get()) &&
            Al(cpu) == 0x0DU && cpu.Eip == guest.context->runtime_base + 2U &&
            guest.context->dos_console_input_count == 1U;
    }

    // AH=07h is the same service without the Ctrl-C check, which this HLE
    // does not perform for either.
    bool direct_input = false;
    {
        ProbeGuest guest;
        auto cpu = guest.MakeCpu(0x07U);
        direct_input = Push(guest.context.get(), 0x3920U, 0x3920U) &&
            repiu::engine::HandleDosInterrupt21(&cpu, guest.context.get()) &&
            Al(cpu) == 0x20U && cpu.Eip == guest.context->runtime_base + 2U;
    }

    // The dynamic backend reaches the traced INT 21h dispatcher first, and
    // that dispatcher keeps its own allow-list (Task 487). Both functions
    // have to pass through it.
    bool traced = !kTracedDispatcherReachable;
    if (kTracedDispatcherReachable)
    {
        ProbeGuest guest;
        auto eight = guest.MakeCpu(0x08U);
        auto seven = guest.MakeCpu(0x07U);
        traced = Push(guest.context.get(), 0x1E61U, 0x1E61U) &&
            Push(guest.context.get(), 0x3062U, 0x3062U) &&
            repiu::engine::HandleTracedDosInterrupt21(
                &eight, guest.context.get()) &&
            Al(eight) == 0x61U &&
            eight.Eip == guest.context->runtime_base + 2U &&
            repiu::engine::HandleTracedDosInterrupt21(
                &seven, guest.context.get()) &&
            Al(seven) == 0x62U &&
            seven.Eip == guest.context->runtime_base + 2U;
        auto empty = guest.MakeCpu(0x08U);
        traced = traced &&
            repiu::engine::HandleTracedDosInterrupt21(
                &empty, guest.context.get()) &&
            empty.Eip == guest.context->runtime_base;
    }

    const bool all = ordinary && extended && waits && direct_input && traced;
    std::cout << "dos_console_input_ordinary=" << (ordinary ? "true" : "false")
              << "\ndos_console_input_extended="
              << (extended ? "true" : "false")
              << "\ndos_console_input_waits=" << (waits ? "true" : "false")
              << "\ndos_console_input_direct=" << (direct_input ? "true" : "false")
              << "\ndos_console_input_traced=" << (traced ? "true" : "false")
              << "\ndos_console_input_traced_skipped="
              << (kTracedDispatcherReachable ? "false" : "true")
              << "\ndos_console_input_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
