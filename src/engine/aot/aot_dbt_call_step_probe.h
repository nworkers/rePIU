#pragma once

#include "repiu/engine/execution_trampoline.h"
#include "repiu/runtime/aot_code_cache.h"

#include "repiu/platform/guest_cpu_context.h"

#include <cstdint>
#include "repiu/platform/fault_handler.h"

// Task 503d-2. This header no longer includes <windows.h>.
//
// It needed two things from it, both only as pointers in declarations. CONTEXT
// becomes GuestCpuContext, which is an alias for CONTEXT on Windows, so the
// definitions and callers are untouched. EXCEPTION_POINTERS is forward declared
// by its underlying tag: a pointer to an incomplete type is all a declaration
// needs, and on Windows it resolves to the very same type.
//
// Handing these functions a FaultEvent instead belongs with migrating the
// dispatcher itself, not here.
struct _EXCEPTION_POINTERS;

namespace repiu::engine
{

struct ThreadContext;

void ConfigureAotDbtCallStepProbe(
    ThreadContext* context, const char* sequence_list);

bool MaybeArmAotDbtCallStepProbe(
    ThreadContext* context,
    Win32AotTransferOrigin origin,
    const runtime::AotDbtIndirectDispatchSite& site,
    std::uint32_t call_sequence,
    std::uint32_t guest_target,
    std::uint32_t cache_target,
    std::uint32_t guest_return,
    std::uint32_t entry_esp,
    std::uint32_t* saved_eflags);

bool HandleAotDbtCallStepProbe(const repiu::platform::FaultEvent& fault,
                               ThreadContext* context);

bool AotDbtCallStepReturnWatchActive(const ThreadContext* context);

}  // namespace repiu::engine
