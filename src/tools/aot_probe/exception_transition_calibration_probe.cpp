#include "exception_transition_calibration_probe.h"

#include <cstdint>
#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#endif

namespace repiu::tools
{
namespace
{

#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86)

constexpr std::uint32_t kTrapFlag = 0x00000100U;
constexpr std::uint32_t kInt3Iterations = 20000U;
constexpr std::uint32_t kSingleStepBudget = 20000U;

DWORD g_probe_thread_id = 0;
bool g_int3_active = false;
bool g_single_step_active = false;
std::uint32_t g_int3_hits = 0;
std::uint32_t g_single_step_hits = 0;

// Minimal handler: the point of the calibration is to price the Windows
// exception round trip itself, so this body must stay as small as possible.
LONG CALLBACK CalibrationExceptionHandler(EXCEPTION_POINTERS* info)
{
    if (info == nullptr || info->ExceptionRecord == nullptr ||
        info->ContextRecord == nullptr ||
        GetCurrentThreadId() != g_probe_thread_id)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    if (g_int3_active && code == EXCEPTION_BREAKPOINT)
    {
        ++g_int3_hits;
        // __debugbreak emits a one-byte 0xCC, so resume past it.
        info->ContextRecord->Eip += 1U;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (g_single_step_active && code == EXCEPTION_SINGLE_STEP)
    {
        ++g_single_step_hits;
        if (g_single_step_hits < kSingleStepBudget)
        {
            info->ContextRecord->EFlags |= kTrapFlag;
        }
        else
        {
            info->ContextRecord->EFlags &= ~kTrapFlag;
        }
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

std::uint64_t MeasureBaselineLoop(std::uint32_t iterations)
{
    volatile std::uint32_t sink = 0;
    const std::uint64_t start = __rdtsc();
    for (std::uint32_t index = 0; index < iterations; ++index)
    {
        sink = sink + 1U;
    }
    return __rdtsc() - start;
}

std::uint64_t MeasureInt3Loop(std::uint32_t iterations)
{
    const std::uint64_t start = __rdtsc();
    for (std::uint32_t index = 0; index < iterations; ++index)
    {
        __debugbreak();
    }
    return __rdtsc() - start;
}

std::uint64_t MeasureSingleStepLoop(std::uint32_t iterations)
{
    volatile std::uint32_t sink = 0;
    const std::uint64_t start = __rdtsc();
    // Inline asm cannot reference the constexpr, so the trap-flag bit is
    // written literally here; it must stay equal to kTrapFlag.
    __asm
    {
        pushfd
        or dword ptr [esp], 100h
        popfd
    }
    for (std::uint32_t index = 0; index < iterations; ++index)
    {
        sink = sink + 1U;
    }
    return __rdtsc() - start;
}

#endif

}  // namespace

bool RunExceptionTransitionCalibrationProbe()
{
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86)
    g_probe_thread_id = GetCurrentThreadId();
    PVOID handler = AddVectoredExceptionHandler(
        1, CalibrationExceptionHandler);
    if (handler == nullptr)
    {
        std::cout << "exception_transition_calibration_all=false\n";
        return false;
    }

    const std::uint64_t baseline_cycles =
        MeasureBaselineLoop(kInt3Iterations);

    g_int3_active = true;
    const std::uint64_t int3_cycles = MeasureInt3Loop(kInt3Iterations);
    g_int3_active = false;

    // The single-step loop needs enough instructions to exhaust the budget;
    // each source iteration compiles to several, so the same count suffices.
    g_single_step_active = true;
    const std::uint64_t single_step_cycles =
        MeasureSingleStepLoop(kSingleStepBudget);
    g_single_step_active = false;

    RemoveVectoredExceptionHandler(handler);

    const std::uint64_t int3_net =
        int3_cycles > baseline_cycles ? int3_cycles - baseline_cycles : 0U;
    const std::uint64_t single_step_net =
        single_step_cycles > baseline_cycles
            ? single_step_cycles - baseline_cycles
            : 0U;
    const std::uint64_t int3_per_transition =
        g_int3_hits != 0U ? int3_net / g_int3_hits : 0U;
    const std::uint64_t single_step_per_transition =
        g_single_step_hits != 0U
            ? single_step_net / g_single_step_hits
            : 0U;

    const bool all =
        g_int3_hits == kInt3Iterations &&
        g_single_step_hits >= kSingleStepBudget &&
        int3_per_transition != 0U &&
        single_step_per_transition != 0U;

    std::cout
        << "exception_transition_int3_hits=" << g_int3_hits
        << "\nexception_transition_int3_cycles_per_transition="
        << int3_per_transition
        << "\nexception_transition_single_step_hits=" << g_single_step_hits
        << "\nexception_transition_single_step_cycles_per_transition="
        << single_step_per_transition
        << "\nexception_transition_calibration_all="
        << (all ? "true" : "false") << "\n";
    return all;
#else
    std::cout << "exception_transition_calibration_all=skipped\n";
    return true;
#endif
}

}  // namespace repiu::tools
