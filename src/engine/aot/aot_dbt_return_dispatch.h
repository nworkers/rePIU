#pragma once

#include "repiu/engine/execution_trampoline.h"

namespace repiu::engine
{

struct ThreadContext;

void RecordAotDbtReturnFallback(
    ThreadContext* context,
    AotDbtDispatchFallbackReason reason);

void* GetAotDbtReturnMissThunkAddress();

}  // namespace repiu::engine
