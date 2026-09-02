// Task 578. The x86-64 guest entry bridge.
//
// Same shape as Task 558's probe bridge, which has been executing emitted bytes
// under this mapping since then. The probe's copy stays where it is: it drives a
// plan and an emitter, this one drives a placement, and two independent
// verifications of one contract is worth more than one shared copy.
//
// On entry (SysV AMD64):
//   RDI  the host address to enter -- inside the placed code cache
//   RSI  a LinuxX64GuestEntryState
//
// The guest's registers go into the host registers of the same number, which is
// Task 558's mapping and is forced rather than chosen: `kIdenticalBytes` copies
// instructions byte for byte, so `add eax, ebx` can only be right when the guest
// registers it names are the host registers of the same number.
//
// Guest ESP is the exception, in R15 rather than host RSP, because host RSP
// stays the SysV stack (Task 546 decision 3). R15's upper half is written as
// zero here for the reason Task 558 gives: an access through guest ESP is
// emitted as `[r15]`, where the whole 64-bit register is the address.

#include "repiu/platform/linux_x64_guest_entry.h"

    .intel_syntax noprefix
    .text

    .globl RepiuLinuxX64GuestEntry
    .type RepiuLinuxX64GuestEntry, @function
RepiuLinuxX64GuestEntry:
    // Every callee-saved register is saved, not only the ones the guest is
    // known to touch. Emitted code holds guest EBX, EBP and guest ESP in
    // RBX, RBP and R15, and a boundary can leave any of them anywhere.
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    // One more push to bring RSP back to 16-byte alignment for the call: six
    // pushes plus the return address leaves it 8 off.
    push rsi

    mov r10, rdi                 // the cache address to enter
    mov r11, rsi                 // the state block

    mov eax, dword ptr [r11 + REPIU_X64_ENTRY_EAX]
    mov ecx, dword ptr [r11 + REPIU_X64_ENTRY_ECX]
    mov edx, dword ptr [r11 + REPIU_X64_ENTRY_EDX]
    mov ebx, dword ptr [r11 + REPIU_X64_ENTRY_EBX]
    mov ebp, dword ptr [r11 + REPIU_X64_ENTRY_EBP]
    mov esi, dword ptr [r11 + REPIU_X64_ENTRY_ESI]
    mov edi, dword ptr [r11 + REPIU_X64_ENTRY_EDI]
    // Guest ESP last, and as a 32-bit move so the upper half is zeroed by the
    // hardware -- the same thing the emitter's `lea r15d, ...` relies on.
    mov r15d, dword ptr [r11 + REPIU_X64_ENTRY_GUEST_ESP]

    call r10

    // The state block address is back off the stack rather than kept in a
    // register: R11 is caller-saved and the guest owns every register the
    // mapping names, so nothing may be assumed to survive the call.
    mov r11, qword ptr [rsp]
    mov dword ptr [r11 + REPIU_X64_ENTRY_EAX], eax
    mov dword ptr [r11 + REPIU_X64_ENTRY_ECX], ecx
    mov dword ptr [r11 + REPIU_X64_ENTRY_EDX], edx
    mov dword ptr [r11 + REPIU_X64_ENTRY_EBX], ebx
    mov dword ptr [r11 + REPIU_X64_ENTRY_EBP], ebp
    mov dword ptr [r11 + REPIU_X64_ENTRY_ESI], esi
    mov dword ptr [r11 + REPIU_X64_ENTRY_EDI], edi
    // All 64 bits of R15, so a run that left rubbish in the upper half is
    // visible to the caller instead of being truncated into looking correct.
    mov qword ptr [r11 + REPIU_X64_ENTRY_GUEST_ESP], r15

    pop rsi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret
    .size RepiuLinuxX64GuestEntry, .-RepiuLinuxX64GuestEntry

    .section .note.GNU-stack,"",@progbits
