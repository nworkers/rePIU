#pragma once

struct _CONTEXT;

namespace repiu::platform::win32
{

struct ThreadContext;

// Handles the observed absent predecessor of a guest-installed INT 8 handler.
bool HandleTimerInterruptChainBoundary(_CONTEXT* win32_context,
                                       ThreadContext* context);

} // namespace repiu::platform::win32
