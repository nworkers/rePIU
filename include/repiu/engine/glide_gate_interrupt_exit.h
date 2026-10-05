#ifndef REPIU_ENGINE_GLIDE_GATE_INTERRUPT_EXIT_H_
#define REPIU_ENGINE_GLIDE_GATE_INTERRUPT_EXIT_H_

#include <cstddef>
#include <cstdint>

namespace repiu::engine
{

// #6. How a direct-model Glide gate leaves into the guest's timer handler.
//
// The gate thunk keeps its frame on the guest stack: eight registers, the
// flags, its own return address and, above them, the game's return address.
// A tick injected while the swap gate waits (Task 750) writes its interrupt
// frame -- EIP, CS, EFLAGS -- over the last three of those, and the thunk's
// tail (`popa; popf; ret`, then the gate's `ret n`) cannot enter a handler
// with the arguments kept anyway. So the resolver points that tail at the exit code below, which puts
// the two overwritten words of the interrupt frame back and jumps:
//
//   push cs                  ; 0E
//   push dword ptr [eip]     ; FF 35 abs32
//   jmp  dword ptr [target]  ; FF 25 abs32
//
// It touches no register and no flag. `push cs` stores the code segment the
// guest really runs in, which is what the fault path's injection records and
// what the handler's native `iret` needs; the thunk path has no register
// context to read it from. The jump is near: the handler's selector is a
// logical one, and no host changes CS for it on the fault path either. See
// docs/design/20261005-i006-direct-model-swap-wait-ticks.md.

// What the exit code reads. Written by the resolver, read at once by the exit
// code on the same thread.
struct GlideGateInterruptExitSlots
{
    std::uint32_t frame_eip = 0;
    std::uint32_t target = 0;
};

constexpr std::size_t kGlideGateInterruptExitSize = 13U;

// Writes the exit code for slots that live at the given 32-bit addresses.
void EncodeGlideGateInterruptExit(std::uint8_t* code,
                                  std::uint32_t frame_eip_address,
                                  std::uint32_t target_address);

// `frame8` points at the thunk frame's flags slot, where the injection left
// the interrupt frame: [0] EIP, [1] CS, [2] EFLAGS. Moves EIP into its slot
// and stores what the thunk's tail consumes instead: the handler's entry
// flags for `popf` and the exit code's address for `ret`. The exit code then
// pushes the real CS and the EIP back over them. [2] stays.
void ArrangeGlideGateInterruptExit(std::uint32_t* frame8,
                                   std::uint32_t entry_eflags,
                                   std::uint32_t exit_address,
                                   std::uint32_t target,
                                   GlideGateInterruptExitSlots* slots);

// The process's slots, and the address of the exit code that reads them. The
// code is placed in an executable page at first use. Zero when the host
// refused the page or an address does not fit a 32-bit operand, which is
// every host that is not the direct model.
GlideGateInterruptExitSlots* GlideGateInterruptExitSlotStorage();
std::uint32_t GlideGateInterruptExitAddress();

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_GLIDE_GATE_INTERRUPT_EXIT_H_
