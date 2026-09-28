// Task 503d. The bridge every AOT/DBT dispatch thunk crosses, as a macro.
//
// Generated guest code enters a thunk with no arguments and with the guest's
// stack still current. The thunk has to park every register where a C function
// can read and edit it, move onto the host stack, run that function, and then
// put everything back so precisely that the guest cannot tell it was gone.
//
// Five dispatch thunks do exactly this on Windows in MSVC inline assembly,
// differing only in which resolver they call. GCC has no `__declspec(naked)`
// on x86, so they move to hand-written assembly here -- and since they differ
// only in a name, they become one macro rather than five near-copies.
//
// Two things about this file are deliberate:
//
//   * Intel syntax, so it reads line for line against the MSVC originals it is
//     derived from. A transcription error in a thunk is not the kind of bug
//     that announces itself.
//
//   * Globals addressed directly, again as on Windows. That is only legal
//     because the Linux binaries link non-PIE with their text segment lifted
//     out of the guest's address range; in a PIE it would force a text
//     relocation, and a writable text segment defeats the page protection this
//     engine detects self-modifying code with. See CMakeLists.txt.
//
// What is NOT here is the pair of writes to `fs:[4]` and `fs:[8]`. On Windows
// each thunk swaps the TIB's StackBase and StackLimit to the host stack before
// the call and back afterwards, 28 sites in all, because the kernel consults
// them when it dispatches an exception and would otherwise refuse to deliver
// one taken while the guest stack is current. Linux keeps no such record: the
// kernel delivers a signal onto the alternate stack installed with
// `sigaltstack`, and asks nothing about where the interrupted stack lives. The
// probe that instantiates this macro takes a fault from inside the resolver,
// on the switched stack, to hold that claim to evidence.

    .intel_syntax noprefix

// entry         -- symbol to define
// resolver      -- C function to call as (context, frame)
// context_ptr   -- global holding the thread context, or zero to do nothing
// host_esp_ptr  -- global holding the host stack pointer to switch to
// refusal       -- what the thunk does when there is no context to cross with:
//                    return  -- hand control back to the caller untouched
//                    trap    -- int3, because the site has nowhere to continue
//                    <bytes> -- resume that many bytes past the dispatch
//                               address the site pushed before calling
//
// The refusal is not an afterthought. A dispatch site pushes its own address
// and then calls; the resolver rewrites the return address to say where the
// guest goes next, and when there is no resolver to run the thunk still has to
// leave the site somewhere it can continue from. Returning as if nothing
// happened would land in the metadata the site pushed. The five thunks answer
// this with 15, 15, 16, 21 bytes and one int3 -- one per site shape -- so the
// answer is a parameter and not a shared constant.
.macro REPIU_STACK_BRIDGE entry, resolver, context_ptr, host_esp_ptr, refusal=return
    .globl \entry
    .type \entry, @function
\entry:
    // EFLAGS first, then the eight general registers. The resolver is handed a
    // pointer to this frame and may edit any of it; `popa` below is what makes
    // an edit take effect.
    pushf
    pusha
    // The C ABI requires the direction flag clear, and the guest's value has
    // already been saved by the pushf above.
    cld
    mov esi, esp

    mov ecx, dword ptr [\context_ptr]
    test ecx, ecx
    jz 9f
    mov eax, dword ptr [\host_esp_ptr]
    test eax, eax
    jz 9f

    // Onto the host stack, aligned for fxsave, which needs sixteen.
    mov esp, eax
    sub esp, 512
    and esp, -16
    fxsave [esp]
    mov edi, esp

    // System V wants esp sixteen-byte aligned at the call, and the two
    // arguments below move it by eight. Windows asks for four, which is why the
    // MSVC originals do not have this line.
    sub esp, 8

    // stdcall, arguments right to left: (context, frame). The resolver pops
    // them, and restoring esp below discards the whole host frame regardless.
    push esi
    push ecx
    call \resolver

    fxrstor [edi]
    mov esp, esi
    popa
    popf
    ret

9:
    // The bridge was not crossed. esp is still at the frame, and where the site
    // goes from here is the one thing the five thunks do not share.
    .ifc \refusal, return
    .else
    .ifc \refusal, trap
    int3
    .else
    // [esp+36] is the address `call` pushed, which the `ret` below returns to;
    // [esp+40] is the dispatch address the site pushed before calling.
    mov eax, dword ptr [esp + 40]
    add eax, \refusal
    mov dword ptr [esp + 36], eax
    .endif
    .endif
    popa
    popf
    ret
    .size \entry, .-\entry
.endm
