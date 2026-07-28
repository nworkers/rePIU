#pragma once

struct _CONTEXT;
struct _EXCEPTION_POINTERS;

namespace repiu::platform::win32
{

struct ThreadContext;

void ArmAotTimerSafePoint(ThreadContext* context);
void ClearAotTimerSafePointRequest(ThreadContext* context);
bool HandleAotTimerSafePoint(_EXCEPTION_POINTERS* exception_info,
                             _CONTEXT* win32_context,
                             ThreadContext* context);

// Handles the observed absent predecessor of a guest-installed INT 8 handler.
bool HandleTimerInterruptChainBoundary(_CONTEXT* win32_context,
                                       ThreadContext* context);

} // namespace repiu::platform::win32
