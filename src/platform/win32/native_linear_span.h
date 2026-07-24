#pragma once

#include "repiu/runtime/execution_backend.h"

#include <string_view>

struct _CONTEXT;
using CONTEXT = _CONTEXT;

namespace repiu::platform::win32
{

struct ThreadContext;

bool ResolveNativeLinearSpanEnabled(
    runtime::ExecutionBackend execution_backend,
    std::string_view setting);
bool NativeLinearSpanEnabled(
    runtime::ExecutionBackend execution_backend);
bool TryEnterNativeLinearSpan(CONTEXT* win32_context,
                              ThreadContext* context);
void LeaveNativeLinearSpan(CONTEXT* win32_context,
                           ThreadContext* context,
                           bool reached_boundary);

}  // namespace repiu::platform::win32
