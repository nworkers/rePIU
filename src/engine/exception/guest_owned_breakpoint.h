#pragma once

#include "repiu/platform/fault_handler.h"

#include <cstdint>

namespace repiu::engine
{

struct ThreadContext;

// Task 523. A breakpoint the engine did not plant.
//
// The engine plants INT3 in the AOT code cache, and for execution tracing at
// two tracked guest addresses. Anything else that raises one is the guest's own
// 0xCC byte, and DOS programs place those deliberately: Watcom's fatal-error
// stub is `int3; push edx; call report; hlt`, where the INT3 means "break into
// a debugger if one is attached" and execution is expected to carry on past it
// when none is.
//
// Before this, such a breakpoint was claimed by nobody. Every AOT handler
// declines it because its address is not a cache address, so nothing advanced
// Eip and the guest resumed on the same byte forever. Measured on a real Ubuntu
// host, 814,138 of 814,683 single-step samples sat on one such address: the
// guest had reached its own fatal-error path and the engine turned a clean
// report into a busy hang.
//
// Handling it costs one byte. Report the first few, then step over -- which is
// what the hardware does with no debugger attached, and what lets the guest
// reach its own diagnostic instead of spinning.
//
// **This is a last resort.** It must run only after every handler that could
// own the breakpoint has declined, because stepping over an engine-planted INT3
// would skip the work that INT3 exists to trigger.
[[nodiscard]] bool HandleGuestOwnedBreakpoint(
    const repiu::platform::FaultEvent& fault, ThreadContext* context);

}  // namespace repiu::engine
