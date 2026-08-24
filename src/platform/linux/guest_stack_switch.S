// Task 503d-16. The three hand-written entries the execution trampoline enters
// guest code through, as GAS counterparts of the MSVC inline assembly in
// `execution/execution_trampoline.cpp`.
//
// `CallGuestEntryWithStack` is the stack switch into the guest itself: it parks
// the host's stack pointer and segment selectors where recovery can find them,
// moves onto the guest's stack, and calls the guest's entry point. The other
// two are where a fault taken inside the guest comes back to -- the engine does
// not return from such a fault, it redirects EIP to one of these and lets the
// normal epilogue finish the job.
//
// Like the five dispatch thunks in `aot_dbt_dispatch_thunks.S`, this is Intel
// syntax so it reads line for line against the MSVC original, and it addresses
// globals directly, which is legal only because the Linux binaries link non-PIE
// with their text segment lifted clear of the guest. See CMakeLists.txt.
//
// Three differences from the MSVC original, each for its own reason:
//
//   * **No `fs:[4]` / `fs:[8]`.** Windows swaps the TIB's StackBase and
//     StackLimit around the guest call because the kernel consults them when it
//     dispatches an exception. Linux keeps no such record -- 3d-12 established
//     that by taking a fault on a switched stack and watching the handler
//     deliver and resume regardless. The four state fields and the four globals
//     that carry those bounds are therefore left as they are, which on Linux
//     means zero. Nothing here reads them: the Linux bridge macro does not, and
//     neither does any C++ in the engine.
//
//   * **No `fxsave`.** The five dispatch thunks save FPU state on Linux even
//     where Windows does not, because they call a C resolver and GCC on i386
//     computes in x87. This entry calls the *guest*, and the i386 System V ABI
//     requires the x87 stack to be empty at a call boundary -- so there is no
//     host FPU state live across it to lose. The guest owns the FPU while it
//     runs, on both hosts.
//
//   * **The segment restores in `RecoverGuestStackException` are load-bearing
//     here.** On Windows the fault handler also writes the selectors into the
//     context it resumes; on Linux 3a's `StoreGuestCpuContext` deliberately
//     does not write segment registers back, so this code is the only thing
//     that puts the host's `%ds`, `%es`, `%fs` and `%gs` back. Getting it wrong
//     does not fault here -- it faults later, in the first host code that
//     touches thread-local storage.

#include "repiu/platform/guest_stack_switch.h"

    .intel_syntax noprefix
    .text

// std::uint32_t __attribute__((stdcall))
// CallGuestEntryWithStack(StackSwitchCallState* state)
//
// Returns REPIU_STACK_SWITCH_RETURNED when the guest returned by itself. It
// does not return at all when the guest faulted: the handler substitutes
// `RecoverGuestStackException` for the rest of this function, and the value the
// caller sees is REPIU_STACK_SWITCH_RECOVERED.
    .globl CallGuestEntryWithStack
    .type CallGuestEntryWithStack, @function
CallGuestEntryWithStack:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov ecx, dword ptr [ebp + 8]
    mov eax, dword ptr [ecx + REPIU_STACK_SWITCH_ENTRY_ADDRESS]
    mov edx, dword ptr [ecx + REPIU_STACK_SWITCH_INITIAL_ESP]

    // The host's selectors, into the call state for the crash report and into
    // the globals for recovery. Recovery cannot read the call state until it
    // has a valid data segment, which is the reason for the second copy.
    xor ebx, ebx
    mov bx, fs
    mov dword ptr [ecx + REPIU_STACK_SWITCH_HOST_FS], ebx
    mov dword ptr [g_recovery_host_fs], ebx
    mov bx, ds
    mov dword ptr [ecx + REPIU_STACK_SWITCH_HOST_DS], ebx
    mov dword ptr [g_recovery_host_ds], ebx
    mov bx, es
    mov dword ptr [ecx + REPIU_STACK_SWITCH_HOST_ES], ebx
    mov dword ptr [g_recovery_host_es], ebx
    mov bx, gs
    mov dword ptr [ecx + REPIU_STACK_SWITCH_HOST_GS], ebx
    mov dword ptr [g_recovery_host_gs], ebx
    mov bx, ss
    mov dword ptr [ecx + REPIU_STACK_SWITCH_HOST_SS], ebx

    // Where the host stack stood at this instant. Recovery returns to it, and
    // every dispatch thunk crosses back onto it.
    mov dword ptr [ecx + REPIU_STACK_SWITCH_HOST_ESP], esp
    mov dword ptr [g_repiu_dbt_host_esp], esp

    mov esp, edx
    cmp dword ptr [ecx + REPIU_STACK_SWITCH_SINGLE_STEP], 0
    je 1f
    // Arming TF here rather than in the guest's own EFLAGS means the very first
    // guest instruction traps.
    pushf
    or dword ptr [esp], REPIU_STACK_SWITCH_TRAP_FLAG
    popf
1:
    // The guest may clobber ECX, and the call state is the one thing that must
    // survive it, so the guest's own stack holds it across the call.
    push ecx
    call eax
    pop ecx

    mov dword ptr [ecx + REPIU_STACK_SWITCH_GUEST_RETURN_ESP], esp
    mov esp, dword ptr [ecx + REPIU_STACK_SWITCH_HOST_ESP]
    mov dword ptr [ecx + REPIU_STACK_SWITCH_RESULT_CODE], 0
    mov eax, REPIU_STACK_SWITCH_RETURNED

    pop edi
    pop esi
    pop ebx
    pop ebp
    ret 4
    .size CallGuestEntryWithStack, .-CallGuestEntryWithStack

// void __attribute__((stdcall)) RecoverGuestStackException()
//
// Never called. The fault handler points EIP here with ECX at the call state
// and ESP back at the host stack, so that what runs is the tail of
// `CallGuestEntryWithStack` -- the same four pops and the same `ret 4`, which
// is what makes the caller's frame come out intact rather than merely alive.
    .globl RecoverGuestStackException
    .type RecoverGuestStackException, @function
RecoverGuestStackException:
    mov eax, dword ptr [g_recovery_host_fs]
    mov fs, ax
    mov eax, dword ptr [g_recovery_host_gs]
    mov gs, ax
    mov eax, dword ptr [g_recovery_host_es]
    mov es, ax
    mov eax, dword ptr [g_recovery_host_ds]
    mov ds, ax
    pop edi
    pop esi
    pop ebx
    pop ebp
    mov eax, REPIU_STACK_SWITCH_RECOVERED
    ret 4
    .size RecoverGuestStackException, .-RecoverGuestStackException

// void RecoverHostStackException()
//
// Also never called. This one is planted in EIP when the fault happened on the
// host stack rather than the guest's, where there is no stack switch to undo
// and the only thing owed is a defined return. cdecl, so nothing is popped.
    .globl RecoverHostStackException
    .type RecoverHostStackException, @function
RecoverHostStackException:
    xor eax, eax
    ret
    .size RecoverHostStackException, .-RecoverHostStackException

    .section .note.GNU-stack,"",@progbits
