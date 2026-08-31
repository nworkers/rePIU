// Task 558. Runs emitted long-mode bytes with the guest register mapping in
// place, and reports what came back.
//
// This is the first thing in the port that executes bytes the x64 emitter
// produced. Everything before it built bytes and inspected them.
//
// The state block is indexed by x86 register number -- offset 4*n -- which is
// the mapping decision written as an address rather than as a table:
//
//   0 EAX   4 ECX   8 EDX  12 EBX  16 ESP  20 EBP  24 ESI  28 EDI
//  32 observed R15 (64-bit)

    .intel_syntax noprefix
    .text

    .globl RepiuLinuxX64GuestRegisterProbe
    .type RepiuLinuxX64GuestRegisterProbe, @function
// void RepiuLinuxX64GuestRegisterProbe(void* code, State* state)
RepiuLinuxX64GuestRegisterProbe:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    // Entry RSP is eight-byte misaligned behind the return address, and six
    // pushes preserve that phase, so one pad word makes the call site aligned.
    sub rsp, 8

    // R13 and R14 hold the probe's own two pointers. They are safe from the
    // code about to run for the same reason R15 is: a 32-bit encoding cannot
    // name an extended register at all.
    mov r14, rdi
    mov r13, rsi

    // Poison R15's upper half first. Decision 3 says a 32-bit load zeroes it,
    // and a register that was already zero could not tell us whether that is
    // true.
    movabs r15, 0xDEADBEEF00000000

    mov eax, dword ptr [r13 + 0]
    mov ecx, dword ptr [r13 + 4]
    mov edx, dword ptr [r13 + 8]
    mov ebx, dword ptr [r13 + 12]
    // Guest ESP into R15D. The 32-bit form is the whole of decision 3.
    mov r15d, dword ptr [r13 + 16]
    mov ebp, dword ptr [r13 + 20]
    mov esi, dword ptr [r13 + 24]
    mov edi, dword ptr [r13 + 28]

    call r14

    mov dword ptr [r13 + 0], eax
    mov dword ptr [r13 + 4], ecx
    mov dword ptr [r13 + 8], edx
    mov dword ptr [r13 + 12], ebx
    mov dword ptr [r13 + 20], ebp
    mov dword ptr [r13 + 24], esi
    mov dword ptr [r13 + 28], edi
    // The full 64 bits, so the caller can see the upper half rather than being
    // told it is fine.
    mov qword ptr [r13 + 32], r15

    add rsp, 8
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret
    .size RepiuLinuxX64GuestRegisterProbe, .-RepiuLinuxX64GuestRegisterProbe

    .section .note.GNU-stack,"",@progbits
