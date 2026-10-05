#include "glide_gate_interrupt_exit_probe.h"

#include "repiu/engine/glide_gate_interrupt_exit.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace repiu::tools
{

// #6. The exit a direct-model gate thunk takes into the timer handler: its
// bytes, and the frame rearrangement, replayed here the way the thunk's tail
// and the exit code consume it. The values are pumpit1's on Linux i386.
bool RunGlideGateInterruptExitProbe()
{
    using engine::ArrangeGlideGateInterruptExit;
    using engine::EncodeGlideGateInterruptExit;
    using engine::GlideGateInterruptExitSlots;
    using engine::kGlideGateInterruptExitSize;

    // push cs; push dword ptr [eip slot]; jmp dword ptr [target slot].
    std::array<std::uint8_t, kGlideGateInterruptExitSize + 2U> code{};
    code.fill(0xCCU);
    EncodeGlideGateInterruptExit(code.data(), 0x40A01234U, 0x40A01238U);
    const std::array<std::uint8_t, 13> expected = {
        0x0EU, 0xFFU, 0x35U, 0x34U, 0x12U, 0xA0U, 0x40U,
        0xFFU, 0x25U, 0x38U, 0x12U, 0xA0U, 0x40U};
    const bool encode_ok =
        kGlideGateInterruptExitSize == expected.size() &&
        std::memcmp(code.data(), expected.data(), expected.size()) == 0 &&
        code[13] == 0xCCU && code[14] == 0xCCU;

    // The guest stack around the thunk frame's last three slots, as the
    // injection left it: the interrupt frame over flags, continuation and
    // return address. Slot 3 is the first argument and must not move.
    constexpr std::uint32_t kReturnPad = 0xF3ADD000U;
    constexpr std::uint32_t kFrameEflags = 0x00200216U;
    constexpr std::uint32_t kEntryEflags = 0x00200016U;
    constexpr std::uint32_t kExit = 0xF5F8F000U;
    constexpr std::uint32_t kHandler = 0x01042EAEU;
    constexpr std::uint32_t kRealCs = 0x00000023U;
    constexpr std::uint32_t kArgument = 0x00000001U;
    // The injection wrote CS from a context the thunk path never filled.
    std::array<std::uint32_t, 4> stack = {kReturnPad, 0U, kFrameEflags,
                                          kArgument};
    GlideGateInterruptExitSlots slots;
    ArrangeGlideGateInterruptExit(stack.data(), kEntryEflags, kExit, kHandler,
                                  &slots);
    const bool arrange_ok = stack[0] == kEntryEflags && stack[1] == kExit &&
        stack[2] == kFrameEflags && stack[3] == kArgument &&
        slots.frame_eip == kReturnPad && slots.target == kHandler;

    // The thunk's tail: `popf` reads slot 0, `ret` reads slot 1 and leaves the
    // stack pointer at slot 2. The exit code then pushes CS and the frame's
    // EIP, and jumps to the handler.
    std::size_t sp = 0U;
    const std::uint32_t flags_loaded = stack[sp++];
    const std::uint32_t returned_to = stack[sp++];
    stack[--sp] = kRealCs;
    stack[--sp] = slots.frame_eip;
    const std::uint32_t jumped_to = slots.target;
    const bool replay_ok = flags_loaded == kEntryEflags &&
        returned_to == kExit && jumped_to == kHandler && sp == 0U &&
        // What the handler's `iret` finds: EIP, the real CS, EFLAGS.
        stack[0] == kReturnPad && stack[1] == kRealCs &&
        stack[2] == kFrameEflags && stack[3] == kArgument;

    // Null arguments are refused quietly.
    std::array<std::uint32_t, 3> untouched = {1U, 2U, 3U};
    ArrangeGlideGateInterruptExit(nullptr, 0U, 0U, 0U, &slots);
    ArrangeGlideGateInterruptExit(untouched.data(), 9U, 9U, 9U, nullptr);
    EncodeGlideGateInterruptExit(nullptr, 0U, 0U);
    const bool null_ok = untouched[0] == 1U && untouched[1] == 2U &&
        slots.frame_eip == kReturnPad;

    const bool all = encode_ok && arrange_ok && replay_ok && null_ok;
    std::cout << "glide_gate_interrupt_exit_encode="
              << (encode_ok ? "true" : "false")
              << "\nglide_gate_interrupt_exit_arrange="
              << (arrange_ok ? "true" : "false")
              << "\nglide_gate_interrupt_exit_replay="
              << (replay_ok ? "true" : "false")
              << "\nglide_gate_interrupt_exit_null="
              << (null_ok ? "true" : "false")
              << "\nglide_gate_interrupt_exit_all="
              << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
