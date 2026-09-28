#ifndef REPIU_ENGINE_X87_CONTEXT_H_
#define REPIU_ENGINE_X87_CONTEXT_H_

#include "repiu/platform/guest_cpu_context.h"

namespace repiu::engine
{

// Pushes `value` on the x87 register stack held in the context's save area.
// Nothing calls it today (Task 759 found no caller); it is built on Win32
// only, as it was.
bool PushX87Float(repiu::platform::GuestCpuContext* context, float value);

}  // namespace repiu::engine

#endif  // REPIU_ENGINE_X87_CONTEXT_H_
