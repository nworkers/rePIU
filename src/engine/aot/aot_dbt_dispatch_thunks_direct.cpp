#include "aot_dbt_direct_edge_dispatch.h"
#include "aot_dbt_glide_gate_dispatch.h"
#include "aot_dbt_hle_dispatch.h"
#include "aot_dbt_return_dispatch.h"

// Task 759. The dispatch thunks of the direct execution model (Win32 and
// Linux i386): generated code calls them on the guest stack, and each crosses
// to the host stack and calls its resolver. The thunks themselves are the
// platform layer's -- MSVC inline assembly in
// src/platform/win32/aot_dbt_dispatch_thunks_win32.cpp, instantiations of one
// bridge macro in src/platform/linux/x86/aot_dbt_dispatch_thunks.S -- and the
// resolvers are beside the code each serves.
#if !defined(_M_IX86) && !defined(__i386__)
#error "the direct execution model needs a 32-bit x86 host"
#endif

extern "C" {

void AotDbtDirectEdgeDispatchThunk();
void AotDbtHleDispatchThunk();
void AotDbtReturnMissThunk();
void AotDbtGlideGateDispatchThunk();

}  // extern "C"

namespace repiu::engine
{

void* GetAotDbtDirectEdgeDispatchThunkAddress()
{
    return reinterpret_cast<void*>(&AotDbtDirectEdgeDispatchThunk);
}

void* GetAotDbtHleDispatchThunkAddress()
{
    return reinterpret_cast<void*>(&AotDbtHleDispatchThunk);
}

void* GetAotDbtReturnMissThunkAddress()
{
    return reinterpret_cast<void*>(&AotDbtReturnMissThunk);
}

void* GetGlideGateDirectDispatchThunkAddress()
{
    return reinterpret_cast<void*>(&AotDbtGlideGateDispatchThunk);
}

// The thunk names its resolver itself; there is nothing to install.
void InstallGlideGateDirectDispatchResolver()
{
}

}  // namespace repiu::engine
