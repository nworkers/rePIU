#pragma once

struct _CONTEXT;
using CONTEXT = _CONTEXT;

namespace repiu::platform::win32
{

struct ThreadContext;

bool NativeLinearSpanEnabled();
bool TryEnterNativeLinearSpan(CONTEXT* win32_context,
                              ThreadContext* context);
void LeaveNativeLinearSpan(CONTEXT* win32_context,
                           ThreadContext* context,
                           bool reached_boundary);

}  // namespace repiu::platform::win32
