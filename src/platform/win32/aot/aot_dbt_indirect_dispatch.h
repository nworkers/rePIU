#pragma once

#include "repiu/platform/win32/execution_trampoline.h"

namespace repiu::platform::win32
{

struct ThreadContext;

void RecordAotDbtIndirectFallback(
    ThreadContext* context,
    AotDbtDispatchFallbackReason reason);

void* GetAotDbtIndirectMissThunkAddress();

}  // namespace repiu::platform::win32
