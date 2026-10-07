// Task 759. The four dispatch thunks the AOT/DBT engine plants in generated
// code, for the Win32 host: MSVC inline assembly. They used to live in the
// engine beside the resolver each one calls; the Linux i386 counterparts are
// the instantiations of one bridge macro in
// src/platform/linux/x86/aot_dbt_dispatch_thunks.S, and this file is that
// file's counterpart. The bodies are unchanged.
//
// Each thunk crosses from the guest stack to the host stack, calls its
// resolver with the thread context and the saved frame, and crosses back. The
// resolvers and the active thread context are the engine's, named here by
// their C symbols as the assembler file names them.
#if !defined(_MSC_VER) || !defined(_M_IX86)
#error "the Win32 host is built as x86 with MSVC only"
#endif

#include "repiu/platform/guest_stack_switch.h"
#include "repiu/platform/thunk_calling_convention.h"

#include <cstdint>

extern "C" {

// The engine's thread context; only its address is read here.
extern void* g_repiu_active_thread_context;

void REPIU_THUNK_RESOLVER_CALL ResolveAotDbtDirectEdgeFrame(
    void* context, std::uint32_t* frame);
void REPIU_THUNK_RESOLVER_CALL ResolveAotDbtHleFrame(
    void* context, std::uint32_t* frame);
void REPIU_THUNK_RESOLVER_CALL ResolveAotDbtReturnMissFrame(
    void* context, std::uint32_t* frame);
void REPIU_THUNK_RESOLVER_CALL ResolveAotDbtGlideGateFrame(
    void* context, std::uint32_t* frame);

}  // extern "C"

// aot_dbt_direct_edge_dispatch.cpp
extern "C" __declspec(naked) void AotDbtDirectEdgeDispatchThunk()
{
    __asm
    {
        pushfd
        pushad
        cld
        mov esi, esp
        mov ecx, dword ptr [g_repiu_active_thread_context]
        test ecx, ecx
        jz fail_without_host
        mov eax, dword ptr [g_repiu_dbt_host_esp]
        test eax, eax
        jz fail_without_host

        mov edx, dword ptr [g_repiu_dbt_host_stack_base]
        mov dword ptr fs:[4], edx
        mov edx, dword ptr [g_repiu_dbt_host_stack_limit]
        mov dword ptr fs:[8], edx
        mov esp, eax
        sub esp, 512
        and esp, -16
        fxsave [esp]
        mov edi, esp
        push esi
        push ecx
        call ResolveAotDbtDirectEdgeFrame
        fxrstor [edi]

        mov eax, dword ptr [g_repiu_dbt_guest_stack_base]
        mov dword ptr fs:[4], eax
        mov eax, dword ptr [g_repiu_dbt_guest_stack_limit]
        mov dword ptr fs:[8], eax
        mov esp, esi
        popad
        popfd
        ret

    fail_without_host:
        mov eax, dword ptr [esp + 40]
        add eax, 15
        mov dword ptr [esp + 36], eax
        popad
        popfd
        ret
    }
}

// aot_dbt_hle_dispatch.cpp
extern "C" __declspec(naked) void AotDbtHleDispatchThunk()
{
    __asm
    {
        pushfd
        pushad
        // Host C/C++ ABI requires forward string operations. The saved guest
        // EFLAGS still carries DF and popfd restores it on either continuation.
        cld

        mov esi, esp
        mov ecx, dword ptr [g_repiu_active_thread_context]
        test ecx, ecx
        jz fail_without_host
        mov eax, dword ptr [g_repiu_dbt_host_esp]
        test eax, eax
        jz fail_without_host

        mov edx, dword ptr [g_repiu_dbt_host_stack_base]
        mov dword ptr fs:[4], edx
        mov edx, dword ptr [g_repiu_dbt_host_stack_limit]
        mov dword ptr fs:[8], edx
        mov esp, eax
        sub esp, 512
        and esp, -16
        fxsave [esp]
        mov edi, esp
        push esi
        push ecx
        call ResolveAotDbtHleFrame
        fxrstor [edi]

        mov eax, dword ptr [g_repiu_dbt_guest_stack_base]
        mov dword ptr fs:[4], eax
        mov eax, dword ptr [g_repiu_dbt_guest_stack_limit]
        mov dword ptr fs:[8], eax
        mov esp, esi
        popad
        popfd
        ret

    fail_without_host:
        mov eax, dword ptr [esp + 40]
        add eax, 15
        mov dword ptr [esp + 36], eax
        popad
        popfd
        ret
    }
}

// aot_dbt_return_dispatch.cpp
extern "C" __declspec(naked) void AotDbtReturnMissThunk()
{
    __asm
    {
        pushfd
        pushad
        mov esi, esp
        mov ecx, dword ptr [g_repiu_active_thread_context]
        test ecx, ecx
        jz fail_without_host
        mov eax, dword ptr [g_repiu_dbt_host_esp]
        test eax, eax
        jz fail_without_host

        mov edx, dword ptr [g_repiu_dbt_host_stack_base]
        mov dword ptr fs:[4], edx
        mov edx, dword ptr [g_repiu_dbt_host_stack_limit]
        mov dword ptr fs:[8], edx
        mov esp, eax
        push esi
        push ecx
        call ResolveAotDbtReturnMissFrame

        mov eax, dword ptr [g_repiu_dbt_guest_stack_base]
        mov dword ptr fs:[4], eax
        mov eax, dword ptr [g_repiu_dbt_guest_stack_limit]
        mov dword ptr fs:[8], eax
        mov esp, esi
        popad
        popfd
        ret

    fail_without_host:
        mov eax, dword ptr [esp + 40]
        add eax, 16
        mov dword ptr [esp + 36], eax
        popad
        popfd
        ret
    }
}

// aot_dbt_glide_gate_dispatch.cpp
extern "C" __declspec(naked) void AotDbtGlideGateDispatchThunk()
{
    __asm
    {
        pushfd
        pushad
        cld

        mov esi, esp
        mov ecx, dword ptr [g_repiu_active_thread_context]
        test ecx, ecx
        jz fail_without_host
        mov eax, dword ptr [g_repiu_dbt_host_esp]
        test eax, eax
        jz fail_without_host

        mov edx, dword ptr [g_repiu_dbt_host_stack_base]
        mov dword ptr fs:[4], edx
        mov edx, dword ptr [g_repiu_dbt_host_stack_limit]
        mov dword ptr fs:[8], edx
        mov esp, eax
        sub esp, 512
        and esp, -16
        fxsave [esp]
        mov edi, esp
        push esi
        push ecx
        call ResolveAotDbtGlideGateFrame
        fxrstor [edi]

        mov eax, dword ptr [g_repiu_dbt_guest_stack_base]
        mov dword ptr fs:[4], eax
        mov eax, dword ptr [g_repiu_dbt_guest_stack_limit]
        mov dword ptr fs:[8], eax
        mov esp, esi
        popad
        popfd
        ret

    fail_without_host:
        int 3
    }
}
