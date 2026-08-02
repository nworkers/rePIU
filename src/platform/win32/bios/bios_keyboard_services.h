#pragma once

// INT 16h BIOS keyboard services. Separate from dos/ because INT 16h is a BIOS
// service, not a DOS or DPMI one, and the guest reaches it through the same
// software-interrupt boundary.

#include "thread_context.h"

namespace repiu::platform::win32
{

bool HandleBiosInterrupt16(CONTEXT* win32_context, ThreadContext* context);

} // namespace repiu::platform::win32
