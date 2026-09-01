// Task 562. The x64 return-dispatch thunk.
//
// Reached by `jmp` from an emitted return slot, with:
//
//   R14D  the guest return address the slot popped
//   R15D  guest ESP, already advanced past it
//   guest GPRs in host GPRs, per the Task 558 mapping
//
// It fills the named frame, asks the resolver where that guest address lives in
// the cache, and jumps there. A resolver that answers zero gets an INT3, which
// is Task 553's rule: what long mode cannot complete reaches a boundary rather
// than being guessed at.

#include "repiu/platform/linux_x64_aot_frame.h"

    .intel_syntax noprefix
    .text

    .globl RepiuLinuxX64ReturnThunk
    .type RepiuLinuxX64ReturnThunk, @function
RepiuLinuxX64ReturnThunk:
    // R11 and R10 are caller-saved and are not guest registers, so they are
    // free here and free to lose across the call below.
    mov r11, qword ptr [rip + repiu_linux_x64_dispatch_frame]

    // Guest state into the frame.
    //
    // Only EAX, ECX, EDX, ESI and EDI have to survive this way: the resolver is
    // a SysV C function, so RBX, RBP and R12-R15 come back on their own, which
    // covers guest EBX, EBP and guest ESP in R15D. Those three are written
    // anyway, at three stores, so the resolver sees whole guest state rather
    // than the half that happened to need saving.
    mov dword ptr [r11 + 0], edi
    mov dword ptr [r11 + 4], esi
    mov dword ptr [r11 + 8], ebx
    mov dword ptr [r11 + 12], edx
    mov dword ptr [r11 + 16], ecx
    mov dword ptr [r11 + 20], eax
    mov dword ptr [r11 + 24], ebp
    mov dword ptr [r11 + 32], r15d
    // The guest address being asked about, and the same value as the frame's
    // guest EIP: the resolver is being asked where the guest would be.
    mov dword ptr [r11 + 28], r14d
    mov dword ptr [r11 + REPIU_LINUX_X64_FRAME_GUEST_SOURCE], r14d
    mov dword ptr [r11 + REPIU_LINUX_X64_FRAME_STATUS], 0

    // SysV AMD64: RDI is the context, RSI the frame.
    mov rdi, qword ptr [rip + repiu_linux_x64_dispatch_context]
    mov rsi, r11

    // Alignment is forced rather than computed. This thunk is reached by `jmp`,
    // so nothing promises RSP's phase on entry; a `sub rsp, 8` would be right
    // only while there is exactly one path in.
    mov r10, rsp
    and rsp, -16
    push r10
    push r10                    // padding, and keeps the call site 16-aligned

    call qword ptr [rip + repiu_linux_x64_dispatch_resolver]

    pop r10
    pop r10
    mov rsp, r10

    // The answer, moved out of RAX before RAX becomes guest EAX again.
    mov r10, rax
    mov r11, qword ptr [rip + repiu_linux_x64_dispatch_frame]
    mov edi, dword ptr [r11 + 0]
    mov esi, dword ptr [r11 + 4]
    mov ebx, dword ptr [r11 + 8]
    mov edx, dword ptr [r11 + 12]
    mov ecx, dword ptr [r11 + 16]
    mov eax, dword ptr [r11 + 20]
    mov ebp, dword ptr [r11 + 24]
    // Guest ESP is reloaded too, because a resolver is allowed to move it --
    // and the 32-bit form is what keeps R15's upper half zero.
    mov r15d, dword ptr [r11 + 32]

    test r10, r10
    jz .Lreturn_thunk_unresolved
    jmp r10

.Lreturn_thunk_unresolved:
    int3
    .size RepiuLinuxX64ReturnThunk, .-RepiuLinuxX64ReturnThunk

    .section .note.GNU-stack,"",@progbits
