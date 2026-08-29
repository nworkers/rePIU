#pragma once

#include "repiu/engine/execution_trampoline.h"
#include "repiu/platform/guest_cpu_context.h"
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

namespace repiu::engine
{

struct ThreadContext;

UnhandledBreakpointEvidence CaptureBreakpointEvidence(
    const repiu::platform::FaultEvent& fault,
    const ThreadContext* context);

void CommitUnhandledBreakpointEvidence(
    UnhandledBreakpointEvidence evidence,
    const repiu::platform::GuestCpuContext* final_context,
    ThreadContext* context);

}  // namespace repiu::engine
