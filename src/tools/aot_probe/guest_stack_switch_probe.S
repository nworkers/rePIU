// Task 503d-16. The guest's half of the stack switch, for the probe.
//
// These stand where DOS/4G code stands: they are entered by
// `CallGuestEntryWithStack` after it has moved onto the guest stack, with the
// call state pushed beneath the return address, and they leave by `ret`. What
// they do is record enough for the C++ half to tell whether the switch really
// happened, and -- in the second one -- fault where the guest would.
//
// Assembly rather than C++ because the entry is reached with a stack this
// process did not align to the System V ABI, which is exactly the condition
// the real guest is entered under. A compiled function would be within its
// rights to assume otherwise.

#include "repiu/platform/guest_stack_switch.h"

    .intel_syntax noprefix
    .text

    .globl RepiuGuestStackSwitchProbeEntry
    .type RepiuGuestStackSwitchProbeEntry, @function
RepiuGuestStackSwitchProbeEntry:
    // [esp] is the return address, [esp+4] the call state the switch pushed.
    mov eax, dword ptr [esp + 4]
    mov dword ptr [g_repiu_guest_switch_probe_state_arg], eax
    mov dword ptr [g_repiu_guest_switch_probe_esp], esp
    // A guest clobbers what it likes. If the epilogue does not put these back,
    // the C++ that called across the switch is running with three of its
    // registers replaced.
    mov ebx, 0xDEADBEEF
    mov esi, 0xDEADBEEF
    mov edi, 0xDEADBEEF
    ret
    .size RepiuGuestStackSwitchProbeEntry, .-RepiuGuestStackSwitchProbeEntry

    .globl RepiuGuestStackSwitchProbeFaultEntry
    .type RepiuGuestStackSwitchProbeFaultEntry, @function
RepiuGuestStackSwitchProbeFaultEntry:
    mov dword ptr [g_repiu_guest_switch_probe_esp], esp
    mov eax, dword ptr [g_repiu_guest_switch_probe_guarded]
    // The fault, taken on the guest stack. Recovery replaces the rest of this
    // function, so the store below must not happen -- that it stays zero is
    // what says recovery took over rather than the read merely being allowed.
    mov al, byte ptr [eax]
    mov dword ptr [g_repiu_guest_switch_probe_resumed], 1
    ret
    .size RepiuGuestStackSwitchProbeFaultEntry, .-RepiuGuestStackSwitchProbeFaultEntry

    .section .note.GNU-stack,"",@progbits
