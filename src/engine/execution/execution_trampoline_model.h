#pragma once

#include "thread_context.h"

#include "repiu/engine/aot_code_cache.h"
#include "repiu/engine/shutdown_recovery_policy.h"
#include "repiu/platform/fault_handler.h"
#include "repiu/platform/guest_cpu_context.h"

#include <cstdint>

// Task 759. Where the execution trampoline differs by execution model
// (repiu/runtime/execution_model.h). Implemented in
// execution_trampoline_direct.cpp and execution_trampoline_cache.cpp; CMake
// builds the one that matches the host.
//
// Each function belongs to one model and is reached only on it; the other
// model's implementation is an explicit "nothing to do".

namespace repiu::engine::trampoline_model
{

// The direct model. Switches to the guest stack and runs the guest from its
// entry; returns when the guest returns or a recovery point is reached.
void EnterGuestWithStack(StackSwitchCallState* state, ThreadContext* context);

// The direct model. Runs the guest from its entry on the host stack.
void EnterGuestDirect(ThreadContext* context);

// The cache model. Installs the dispatch frame and its resolver and enters
// the emitted cache at the guest's entry; returns when the cache is left.
void EnterGuestCache(ThreadContext* context);

// The cache model. Where a thread is sent to leave the cache: the host return
// address left by the entry's call. Zero on the direct model.
std::uintptr_t CacheExitAddress();

// Task 730. Where the interrupted thread was, for the shutdown recovery's
// decision. On the cache model `eip` is only the low half of the host's
// instruction pointer, so the full address is read from the host context and
// is required; on the direct model `eip` is the address.
void ReadShutdownRecoveryPosition(std::uint32_t eip,
                                  void* host_context,
                                  ShutdownRecoveryPosition* position);

// The cache model. Points the interrupted thread at the cache's exit, which
// is the only valid unwind while the host stack is in RSP. False when the
// host context could not be written. False on the direct model, which
// recovers through the stack switch's recovery point instead.
bool RedirectToCacheExit(repiu::platform::GuestCpuContext* registers,
                         void* host_context);

// A diagnostic of the cache model (REPIU_LINUX_X64_GUEST_ENTRY_TRACE): one
// line per fault whose entry or exit falls in the traced range.
void TraceGuestEntry(ThreadContext* context,
                     std::uint32_t fault_eip,
                     repiu::platform::FaultKind fault_kind,
                     std::uint32_t entry_eip,
                     std::uint32_t exit_eip,
                     std::uint32_t entry_esp,
                     std::uint32_t exit_esp,
                     std::uint32_t exit_eflags);

}  // namespace repiu::engine::trampoline_model

namespace repiu::engine
{

// The map dump of the trampoline (REPIU_AOT_GUEST_MAP_TRACE and its kin),
// for the cache model's return trace. Defined in execution_trampoline.cpp.
void TraceAotGuestMapForModel(const AotCodeCachePlacement& placement,
                              std::uint32_t runtime_base,
                              const char* phase);

}  // namespace repiu::engine
