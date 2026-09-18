#pragma once

#include "repiu/platform/fault_handler.h"
#include "repiu/platform/guest_cpu_context.h"

namespace repiu::engine
{

struct ThreadContext;

void ArmAotTimerSafePoint(ThreadContext* context);
void ClearAotTimerSafePointRequest(ThreadContext* context);
// Task 503d-4. Takes the fault as 3c reports it rather than as Windows does.
// The registers come from the event, so the separate context argument is gone.
// `handled_guest_source`, when given, receives the guest address of the
// backward edge whose safe point fired. Task 710: a long-mode host that injects
// INT 8 here has to resume in the cache rather than at the raw guest handler,
// and the re-entry helper decodes this address to decide whether it may.
bool HandleAotTimerSafePoint(const repiu::platform::FaultEvent& fault,
                             ThreadContext* context,
                             std::uint32_t* handled_guest_source = nullptr);

// Handles the observed absent predecessor of a guest-installed INT 8 handler.
bool HandleTimerInterruptChainBoundary(repiu::platform::GuestCpuContext* win32_context,
                                       ThreadContext* context);

} // namespace repiu::engine
