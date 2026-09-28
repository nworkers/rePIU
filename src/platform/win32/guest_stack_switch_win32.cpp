// Task 759. The stack switch into the guest and its recovery points, for the
// Win32 host: MSVC inline assembly. They used to live in the engine's
// execution trampoline; the Linux i386 counterparts are in
// src/platform/linux/x86/guest_stack_switch.S, and this file is that file's
// counterpart. The bodies are unchanged.
//
// `state` is the engine's StackSwitchCallState, read here through the offsets
// of repiu/platform/guest_stack_switch.h, which the engine checks against the
// structure with static_assert.
#if !defined(_MSC_VER) || !defined(_M_IX86)
#error "the Win32 host is built as x86 with MSVC only"
#endif

#include "repiu/platform/guest_stack_switch.h"

#include <cstdint>

extern "C" __declspec(naked) std::uint32_t __stdcall
CallGuestEntryWithStack(void* state)
{
    __asm
    {
        push ebp
        mov ebp, esp
        push ebx
        push esi
        push edi

        mov ecx, [ebp + 8]
        mov eax, [ecx + REPIU_STACK_SWITCH_ENTRY_ADDRESS]
        mov edx, [ecx + REPIU_STACK_SWITCH_INITIAL_ESP]

        // Save host stack base/limit
        mov ebx, dword ptr fs:[4]
        mov [ecx + REPIU_STACK_SWITCH_HOST_STACK_BASE], ebx
        mov g_recovery_host_stack_base, ebx
        mov g_repiu_dbt_host_stack_base, ebx
        mov ebx, dword ptr fs:[8]
        mov [ecx + REPIU_STACK_SWITCH_HOST_STACK_LIMIT], ebx
        mov g_recovery_host_stack_limit, ebx
        mov g_repiu_dbt_host_stack_limit, ebx

        // Set guest stack base/limit
        mov ebx, [ecx + REPIU_STACK_SWITCH_GUEST_STACK_BASE]
        mov dword ptr fs:[4], ebx
        mov g_repiu_dbt_guest_stack_base, ebx
        mov ebx, [ecx + REPIU_STACK_SWITCH_GUEST_STACK_LIMIT]
        mov dword ptr fs:[8], ebx
        mov g_repiu_dbt_guest_stack_limit, ebx

        xor ebx, ebx
        mov bx, fs
        mov [ecx + REPIU_STACK_SWITCH_HOST_FS], ebx
        mov g_recovery_host_fs, ebx
        mov bx, ds
        mov [ecx + REPIU_STACK_SWITCH_HOST_DS], ebx
        mov g_recovery_host_ds, ebx
        mov bx, es
        mov [ecx + REPIU_STACK_SWITCH_HOST_ES], ebx
        mov g_recovery_host_es, ebx
        mov bx, gs
        mov [ecx + REPIU_STACK_SWITCH_HOST_GS], ebx
        mov g_recovery_host_gs, ebx
        mov bx, ss
        mov [ecx + REPIU_STACK_SWITCH_HOST_SS], ebx
        mov [ecx + REPIU_STACK_SWITCH_HOST_ESP], esp
        mov g_repiu_dbt_host_esp, esp

        mov esp, edx
        cmp dword ptr [ecx + REPIU_STACK_SWITCH_SINGLE_STEP], 0
        je no_single_step_trace
        pushfd
        or dword ptr [esp], REPIU_STACK_SWITCH_TRAP_FLAG
        popfd
 no_single_step_trace:
        push ecx
        call eax
        pop ecx

        // Restore host stack base/limit
        mov ebx, [ecx + REPIU_STACK_SWITCH_HOST_STACK_BASE]
        mov dword ptr fs:[4], ebx
        mov ebx, [ecx + REPIU_STACK_SWITCH_HOST_STACK_LIMIT]
        mov dword ptr fs:[8], ebx

        mov [ecx + REPIU_STACK_SWITCH_GUEST_RETURN_ESP], esp
        mov esp, [ecx + REPIU_STACK_SWITCH_HOST_ESP]
        mov dword ptr [ecx + REPIU_STACK_SWITCH_RESULT_CODE], 0
        xor eax, eax

        pop edi
        pop esi
        pop ebx
        pop ebp
        ret 4
    }
}

extern "C" __declspec(naked) void __stdcall
RecoverGuestStackException()
{
    __asm
    {
        mov eax, dword ptr cs:[g_recovery_host_stack_base]
        mov dword ptr fs:[4], eax
        mov eax, dword ptr cs:[g_recovery_host_stack_limit]
        mov dword ptr fs:[8], eax

        mov eax, dword ptr cs:[g_recovery_host_fs]
        mov fs, ax
        mov eax, dword ptr cs:[g_recovery_host_gs]
        mov gs, ax
        mov eax, dword ptr cs:[g_recovery_host_es]
        mov es, ax
        mov eax, dword ptr cs:[g_recovery_host_ds]
        mov ds, ax
        pop edi
        pop esi
        pop ebx
        pop ebp
        mov eax, REPIU_STACK_SWITCH_RECOVERED
        ret 4
    }
}

extern "C" __declspec(naked) void
RecoverHostStackException()
{
    __asm
    {
        xor eax, eax
        ret
    }
}
