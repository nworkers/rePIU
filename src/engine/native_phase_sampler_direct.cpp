#include "native_phase_sampler_model.h"

// Task 759. Native-phase capture on the direct execution model (Win32 and
// Linux i386): the guest thread is stopped and read from outside, and its
// stack is scanned.
#if !defined(_M_IX86) && !defined(__i386__)
#error "the direct execution model needs a 32-bit x86 host"
#endif

namespace repiu::engine
{
namespace native_phase_sampler_model
{

bool CaptureEnabled()
{
    return true;
}

bool InterruptsWithHostContext()
{
    return false;
}

bool ScansHostStack()
{
    return true;
}

void RecordNativeInstructionPointer(NativePhaseSample* sample, void* host_context)
{
    (void)sample;
    (void)host_context;
}

}  // namespace native_phase_sampler_model

void WriteLinuxX64NativeSampleTraceLine(
    const NativePhaseSample& sample,
    const std::uint32_t elapsed_milliseconds)
{
    (void)sample;
    (void)elapsed_milliseconds;
}

}  // namespace repiu::engine
