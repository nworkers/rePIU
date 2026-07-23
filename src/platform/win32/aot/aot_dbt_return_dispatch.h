#pragma once

#include "repiu/platform/win32/execution_trampoline.h"

namespace repiu::platform::win32
{

struct ThreadContext;

void RecordAotDbtReturnFallback(
    ThreadContext* context,
    AotDbtReturnFallbackReason reason);

void* GetAotDbtReturnMissThunkAddress();

}  // namespace repiu::platform::win32
