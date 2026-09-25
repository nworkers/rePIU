// Task 720. The x64 Glide gate direct-dispatch thunk.
//
// A patched gate is `E8 rel32` to here, reached from emitted cache code that
// has already put the guest return address at [r15] and jumped to the gate. In
// 64-bit mode that `call` leaves gate+5 on the host stack, so on entry:
//
//   [RSP]  gate address + 5
//   R15D   guest ESP, pointing at the guest return address
//   guest GPRs in host GPRs, per the Task 558 mapping
//
// It records the guest state in the shared dispatch frame, runs the Glide
// export through the engine resolver on a clean FPU state, and then hands the
// guest return address to the return thunk in R14D -- the path every emitted
// `ret` already takes -- rather than repeating its cache lookup, dynamic
// translation and fail-closed handling here. Without this, each Glide call ran
// the gate's `ud2` and went through a signal: 2.42 million of them in Task 719's
// 180-second run.

#include "repiu/platform/linux_x64_aot_frame.h"

    .intel_syntax noprefix
    .text

    .globl RepiuLinuxX64GlideGateThunk
    .type RepiuLinuxX64GlideGateThunk, @function
RepiuLinuxX64GlideGateThunk:
    // The gate's return address. Popping it puts host RSP back where the
    // cache code had it, which is also where the return thunk expects it.
    pop r10
    mov r11, qword ptr [rip + repiu_linux_x64_dispatch_frame]
    mov dword ptr [r11 + 0], edi
    mov dword ptr [r11 + 4], esi
    mov dword ptr [r11 + 8], ebx
    mov dword ptr [r11 + 12], edx
    mov dword ptr [r11 + 16], ecx
    mov dword ptr [r11 + 20], eax
    mov dword ptr [r11 + 24], ebp
    mov dword ptr [r11 + 32], r15d
    pushfq
    pop rax
    mov dword ptr [r11 + 36], eax
    // The gate itself is the guest instruction being executed.
    sub r10d, 5
    mov dword ptr [r11 + 28], r10d
    mov dword ptr [r11 + REPIU_LINUX_X64_FRAME_GUEST_SOURCE], r10d

    // A 512-byte FXSAVE area, 16-byte aligned, with the entry RSP kept above
    // it. The guest's x87 stack may be live across a Glide call and host code
    // is entitled to an empty one, so it is saved, emptied, and put back.
    mov r10, rsp
    and rsp, -16
    sub rsp, 528
    mov qword ptr [rsp + 512], r10
    fxsave64 [rsp]
    fninit
    ldmxcsr dword ptr [rip + .Lglide_gate_default_mxcsr]
    cld

    // SysV AMD64: RDI is the context, RSI the frame. A zero resolver means no
    // guest entry installed one, which is not a state to continue from.
    mov rax, qword ptr [rip + repiu_linux_x64_glide_gate_resolver]
    test rax, rax
    jz .Lglide_gate_failed
    mov rdi, qword ptr [rip + repiu_linux_x64_dispatch_context]
    mov rsi, r11
    call rax

    fxrstor64 [rsp]
    mov rsp, qword ptr [rsp + 512]
    test eax, eax
    jz .Lglide_gate_failed_restored

    mov r11, qword ptr [rip + repiu_linux_x64_dispatch_frame]
    mov edi, dword ptr [r11 + 0]
    mov esi, dword ptr [r11 + 4]
    mov ebx, dword ptr [r11 + 8]
    mov edx, dword ptr [r11 + 12]
    mov ecx, dword ptr [r11 + 16]
    mov eax, dword ptr [r11 + 20]
    mov ebp, dword ptr [r11 + 24]
    mov r15d, dword ptr [r11 + 32]
    mov r14d, dword ptr [r11 + REPIU_LINUX_X64_FRAME_GUEST_SOURCE]
    // Guest flags last, through R10 so no guest register is disturbed; the
    // moves after `popfq` do not touch flags.
    mov r10d, dword ptr [r11 + 36]
    push r10
    popfq
    // Producer tag for the return thunk's provenance diagnostic: zero, which
    // no emitted RET or indirect-call site uses. A `mov`, because `xor` would
    // overwrite the flags just restored.
    mov r10d, 0
    jmp RepiuLinuxX64ReturnThunk

.Lglide_gate_failed:
    fxrstor64 [rsp]
    mov rsp, qword ptr [rsp + 512]
.Lglide_gate_failed_restored:
    int3
    .size RepiuLinuxX64GlideGateThunk, .-RepiuLinuxX64GlideGateThunk

    .section .rodata
    .p2align 2
.Lglide_gate_default_mxcsr:
    .long 0x1F80

    .section .note.GNU-stack,"",@progbits
