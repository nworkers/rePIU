#pragma once

// AOT residency proxy sampling, extracted from aot_runtime_dispatch.cpp
// (Task 478). This is diagnostic instrumentation only: it reads guest code and
// updates counters, and never touches guest registers, memory, or control flow.

#include <cstdint>

namespace repiu::platform::win32
{

// Forward-declared rather than included so the host target, which does not carry
// the win32 subsystem include directories, can reach the gate accessor for its
// summary line.
struct ThreadContext;

// Task 478: opt-in, because this is diagnostic instrumentation rather than an
// A/B-promoted optimization. Unset and empty mean OFF; only `1|on|true` enables
// it, matching REPIU_GLIDE_SETTER_CENSUS and REPIU_EXECUTION_TIME_PROFILE.
//
// A pumpit8 cycle profile measured the sampler at 43,004,277,414 cycles over
// 3,076,235 calls -- 13.67% of guest-run, 13,979 cycles per call -- for one
// summary log line. The loop had no gate at all, so that cost was present in
// ordinary runs too, not only when the time profile was enabled.
bool ResolveAotResidencySampleEnabled(const char* setting);
bool AotResidencySampleEnabled();

// Task 263(b): accumulate the AOT residency proxy for a real cache entry --
// straight-line guest instruction count from `guest_entry_eip` to the first
// control transfer (cap 64), honoring readability. Returns immediately when the
// gate above is off.
void AccumulateAotResidency(ThreadContext* context,
                            std::uint32_t guest_entry_eip);

}  // namespace repiu::platform::win32
