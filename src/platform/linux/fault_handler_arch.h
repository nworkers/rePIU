#pragma once

#if !defined(_WIN32)

#include "repiu/platform/fault_handler.h"

#include <csignal>
#include <cstddef>
#include <cstdint>

// Task 757. What the Linux fault handler needs from one architecture. The
// common handler in fault_handler.cpp calls these; x86/ and x64/ each define
// them in fault_handler_arch.cpp. Internal to src/platform/linux/.

namespace repiu::platform::linux_fault
{

// The host stack pointer at the fault: ESP on i386, RSP on x86-64.
std::uintptr_t HostStackPointer(const void* host_context);

// The x64 dispatch registers the unhandled-fault report names. All three are
// zero where they do not exist, which keeps the report line one shape.
void HostDispatchRegisters(const void* host_context,
                           std::uint64_t* r10,
                           std::uint64_t* r14,
                           std::uint64_t* r15);

// A diagnostic trap this architecture reports and resumes on its own, before
// the fault reaches the engine's callback. Returns true when it was one.
bool HandleArchDiagnosticTrap(int signal_number,
                              const siginfo_t& info,
                              void* host_context,
                              const GuestCpuContext& registers);

// Task 673. Remember the boundary of a signal the callback resumed.
void RecordLastResumedSignal(int signal_number,
                             FaultKind fault_kind,
                             const void* host_context,
                             const GuestCpuContext& registers);

// Fields of the unhandled-fault line that only this architecture has, written
// between `rsp=` and `r10=`.
void AppendArchFaultFields(char* line, std::size_t* length);

}  // namespace repiu::platform::linux_fault

#endif
