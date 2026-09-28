#pragma once

#include "native_phase_sampler.h"

// Task 759. Where native-phase capture differs by execution model. On the
// direct model (Win32 and Linux i386) the guest thread is stopped and read from
// outside, and its stack is scanned for the loader's call site. On the cache
// model (Linux x64) the sample is taken inside a signal handler on the guest
// thread, where the stack scan is not allowed, and it is opt-in (Task 721).
// Defined in native_phase_sampler_direct.cpp and native_phase_sampler_cache.cpp.

namespace repiu::engine::native_phase_sampler_model
{

// Whether capture runs at all.
bool CaptureEnabled();
// Whether the interrupt must hand over the host context (the cache model).
bool InterruptsWithHostContext();
// Whether the interrupted stack may be scanned for a module return address.
bool ScansHostStack();
// Records the host instruction pointer where the model keeps one.
void RecordNativeInstructionPointer(NativePhaseSample* sample, void* host_context);

}  // namespace repiu::engine::native_phase_sampler_model
