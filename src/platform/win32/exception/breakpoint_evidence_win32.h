#pragma once

#include "repiu/platform/win32/execution_trampoline.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace repiu::platform::win32
{

struct ThreadContext;

Win32UnhandledBreakpointEvidence CaptureBreakpointEvidence(
    const EXCEPTION_POINTERS* exception_info,
    const ThreadContext* context);

void CommitUnhandledBreakpointEvidence(
    Win32UnhandledBreakpointEvidence evidence,
    const CONTEXT* final_context,
    ThreadContext* context);

}  // namespace repiu::platform::win32
