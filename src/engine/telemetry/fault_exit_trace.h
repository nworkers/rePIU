#pragma once

// Task 582: one line naming where a declined fault left the handler.
//
// Diagnostic instrumentation only. It reads the exit-site tag Task 410 already
// records and prints it; it never touches guest registers, guest memory, or
// control flow.
//
// It exists to answer the question Task 581 ended on. That unit proved the
// fault reaches `DispatchGuestFault` on x64 and that i386 services the same
// fault, which leaves only "where in between does x64 leave". The exit-site
// axis already names almost every such point, so what is printed here is that
// name rather than a new measurement.

#include <cstdint>

namespace repiu::platform
{
struct FaultEvent;
}

namespace repiu::engine
{

struct ThreadContext;

// Unset and empty mean OFF. A decline is an ordinary event on i386 -- the
// callback takes it and recovers through RecoverToHost -- so an ungated line
// would change every ordinary log.
bool ResolveFaultExitTraceEnabled(const char* setting);
bool FaultExitTraceEnabled();

// Prints the exit site of a fault the handler declined, with the two pieces of
// state that decide whether it could have been taken: `use_guest_stack` and
// whether an active call state exists. Those two decompose the guest-stack
// exit's condition and also decide whether the callback can turn the decline
// into a recovery, so one line reads out the whole question.
//
// Call before recovery runs; recovery rewrites the state this reports.
void RecordFaultExit(const ThreadContext* context,
                     const repiu::platform::FaultEvent& fault);

}  // namespace repiu::engine
