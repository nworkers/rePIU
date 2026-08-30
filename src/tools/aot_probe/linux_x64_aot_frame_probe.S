// Task 547. Synthetic SysV AMD64 bridge probe. This is not the production
// dispatch thunk; it checks the ABI contract that the production bridge must
// satisfy before it is allowed to cross into a resolver.

#include "repiu/platform/linux_x64_aot_frame.h"

    .intel_syntax noprefix
    .text

    .globl RepiuLinuxX64AotFrameAbiProbe
    .type RepiuLinuxX64AotFrameAbiProbe, @function
RepiuLinuxX64AotFrameAbiProbe:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15
    // Entry RSP is eight-byte misaligned after the return address. Six pushes
    // preserve that phase, so the extra eight-byte pad makes RSP 16-byte
    // aligned for both FXSAVE and the resolver call.
    sub rsp, 536
    mov qword ptr [rsp + 512], rdi
    mov qword ptr [rsp + 520], rsi

    movabs rbx, 0x1122334455667788
    movabs r12, 0x2233445566778899
    movabs r13, 0x33445566778899AA
    movabs r14, 0x445566778899AABB
    movabs r15, 0x5566778899AABBCC

    movdqa xmm0, xmmword ptr [rip + repiu_linux_x64_abi_probe_xmm]
    fxsave64 [rsp]

    // SysV AMD64: RDI, RSI, and RDX carry context, frame, and the aligned
    // call-site stack pointer respectively.
    mov rdx, rsp
    call RepiuLinuxX64AotFrameProbeResolver

    fxrstor64 [rsp]
    movdqa xmm1, xmmword ptr [rip + repiu_linux_x64_abi_probe_xmm]
    pcmpeqb xmm0, xmm1
    pmovmskb eax, xmm0
    cmp eax, 0xFFFF
    jne .Labi_probe_fail

    mov rsi, qword ptr [rsp + 520]
    cmp dword ptr [rsi + REPIU_LINUX_X64_FRAME_GUEST_SOURCE], 0x54700001
    jne .Labi_probe_fail
    cmp dword ptr [rsi + REPIU_LINUX_X64_FRAME_GUEST_CONTINUATION], 0x54700002
    jne .Labi_probe_fail
    cmp dword ptr [rsi + REPIU_LINUX_X64_FRAME_STATUS], 0x54700003
    jne .Labi_probe_fail

    movabs rax, 0x1122334455667788
    cmp rbx, rax
    jne .Labi_probe_fail
    movabs rax, 0x2233445566778899
    cmp r12, rax
    jne .Labi_probe_fail
    movabs rax, 0x33445566778899AA
    cmp r13, rax
    jne .Labi_probe_fail
    movabs rax, 0x445566778899AABB
    cmp r14, rax
    jne .Labi_probe_fail
    movabs rax, 0x5566778899AABBCC
    cmp r15, rax
    jne .Labi_probe_fail

    mov eax, 1
    jmp .Labi_probe_restore

.Labi_probe_fail:
    xor eax, eax

.Labi_probe_restore:
    add rsp, 536
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
    .size RepiuLinuxX64AotFrameAbiProbe, .-RepiuLinuxX64AotFrameAbiProbe

    .section .rodata
    .p2align 4
repiu_linux_x64_abi_probe_xmm:
    .quad 0x0123456789ABCDEF
    .quad 0xFEDCBA9876543210

    .section .note.GNU-stack,"",@progbits
