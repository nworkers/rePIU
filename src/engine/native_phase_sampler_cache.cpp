#include "native_phase_sampler_model.h"

// Task 759. Native-phase capture on the cache execution model (Linux x64).
#if !defined(__x86_64__)
#error "the cache execution model needs an x86-64 host"
#endif

#include "repiu/platform/host_error_stream.h"

#include <cstdio>
#include <cstdlib>

namespace repiu::engine
{
namespace native_phase_sampler_model
{

// Task 721. Linux x64 can interrupt the guest thread too (Task 705), so the
// capture is built there as well, behind an opt-in: every sample is a signal
// delivered to the guest thread, and a default run should not start taking
// them without a reason.
bool CaptureEnabled()
{
    static const bool x64_capture_enabled = [] {
        const char* const value = std::getenv("REPIU_LINUX_X64_NATIVE_SAMPLE");
        return value != nullptr && value[0] == '1' && value[1] == '\0';
    }();
    return x64_capture_enabled;
}

bool InterruptsWithHostContext()
{
    return true;
}

    // The Linux x64 callback runs inside a signal handler. The census needs the
    // interrupted guest/cache EIP, not a host stack walk; keep process_vm_readv
    // and module-range scanning out of that handler.
bool ScansHostStack()
{
    return false;
}

void RecordNativeInstructionPointer(NativePhaseSample* sample, void* host_context)
{
    sample->native_instruction_pointer =
        repiu::platform::ReadHostInstructionPointer(host_context);
}

}  // namespace native_phase_sampler_model

void WriteLinuxX64NativeSampleTraceLine(
    const NativePhaseSample& sample,
    const std::uint32_t elapsed_milliseconds)
{
    static const bool enabled = [] {
        const char* const value =
            std::getenv("REPIU_LINUX_X64_NATIVE_SAMPLE_TRACE");
        return value != nullptr && value[0] == '1' && value[1] == '\0';
    }();
    if (!enabled || !sample.captured ||
        sample.native_instruction_pointer == 0U)
    {
        return;
    }
    char buffer[192] = {};
    const int length = std::snprintf(
        buffer, sizeof(buffer),
        "[repiu-x64-sample] elapsed_ms=%lu native_rip=0x%llX "
        "guest_eip=0x%08X mapped=%u\n",
        static_cast<unsigned long>(elapsed_milliseconds),
        static_cast<unsigned long long>(sample.native_instruction_pointer),
        sample.guest_eip, sample.mapped ? 1U : 0U);
    if (length <= 0)
    {
        return;
    }
    const std::size_t byte_count = static_cast<std::size_t>(
        length < static_cast<int>(sizeof(buffer)) ? length
                                                  : sizeof(buffer) - 1U);
    repiu::platform::WriteHostErrorStream(buffer, byte_count);
}

}  // namespace repiu::engine
