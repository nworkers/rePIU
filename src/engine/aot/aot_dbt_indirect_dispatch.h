#pragma once

#include "repiu/engine/execution_trampoline.h"

namespace repiu::engine
{

struct ThreadContext;

void RecordAotDbtIndirectFallback(
    ThreadContext* context,
    AotDbtDispatchFallbackReason reason);

void* GetAotDbtIndirectMissThunkAddress();

}  // namespace repiu::engine
