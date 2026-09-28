// Task 503d-12. The five dispatch thunks the AOT/DBT engine plants in
// generated code, as instantiations of the shared bridge macro.
//
// On Windows each of these is a `__declspec(naked)` function of MSVC inline
// assembly living beside the resolver it calls. GCC has no naked functions on
// x86, so on Linux the bodies come from here and the C++ file keeps only the
// declaration and the address it hands the code generator.
//
// They are five instantiations rather than five copies because the originals
// differ in exactly one respect each: which resolver runs, and where the site
// continues when there is no thread context to cross with. Everything else --
// the frame, the stack switch, the FPU save -- is the same twenty-odd lines, and
// a transcription error in any of them would surface as generated code jumping
// into the middle of an instruction.
//
// The refusal byte counts are the length of the dispatch site each thunk is
// called from, and they are also spelled in the resolver beside it -- 15 in
// `kFallbackFromDispatchBytes`, 16 and 21 in `kFallbackFromMissBytes`. The two
// spellings are the same duplication the MSVC originals already carry, kept
// rather than removed because the assembler cannot see a C++ constant.

#include "stack_bridge.inc.S"

    .text

// aot_dbt_direct_edge_dispatch.cpp
REPIU_STACK_BRIDGE AotDbtDirectEdgeDispatchThunk, ResolveAotDbtDirectEdgeFrame, g_repiu_active_thread_context, g_repiu_dbt_host_esp, 15

// aot_dbt_hle_dispatch.cpp
REPIU_STACK_BRIDGE AotDbtHleDispatchThunk, ResolveAotDbtHleFrame, g_repiu_active_thread_context, g_repiu_dbt_host_esp, 15

// aot_dbt_return_dispatch.cpp
REPIU_STACK_BRIDGE AotDbtReturnMissThunk, ResolveAotDbtReturnMissFrame, g_repiu_active_thread_context, g_repiu_dbt_host_esp, 16

// aot_dbt_indirect_dispatch.cpp
REPIU_STACK_BRIDGE AotDbtIndirectMissThunk, ResolveAotDbtIndirectMissFrame, g_repiu_active_thread_context, g_repiu_dbt_host_esp, 21

// aot_dbt_glide_gate_dispatch.cpp. Alone among the five it traps instead of
// resuming: a gate site is only ever planted while a context exists, so
// arriving without one means the engine's own invariant has already failed and
// there is no address that would be right to continue at.
REPIU_STACK_BRIDGE AotDbtGlideGateDispatchThunk, ResolveAotDbtGlideGateFrame, g_repiu_active_thread_context, g_repiu_dbt_host_esp, trap

    .section .note.GNU-stack,"",@progbits
