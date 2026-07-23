#include "native_linear_span.h"

#include "execution/thread_context.h"
#include "verified_region_analyzer.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace repiu::platform::win32
{

bool NativeLinearSpanEnabled()
{
    static const bool enabled = []() {
        char value[2] = {};
        return GetEnvironmentVariableA(
                   "REPIU_NATIVE_LINEAR_SPAN", value, sizeof(value)) > 0;
    }();
    return enabled;
}

void LeaveNativeLinearSpan(CONTEXT* win32_context,
                           ThreadContext* context,
                           bool reached_boundary)
{
    detail::NativeFastPathState* state = &context->native_fast_path;
    if (!state->linear_span_active)
    {
        return;
    }
    win32_context->Dr0 = state->linear_span_saved_dr0;
    win32_context->Dr6 = state->linear_span_saved_dr6;
    win32_context->Dr7 = state->linear_span_saved_dr7;
    win32_context->EFlags |= 0x00000100U;
    state->linear_span_active = false;
    if (reached_boundary)
    {
        state->linear_span_boundary_count.fetch_add(
            1, std::memory_order_relaxed);
        state->linear_span_instruction_total.fetch_add(
            state->linear_span_instruction_count,
            std::memory_order_relaxed);
    }
    else
    {
        state->linear_span_cancel_count.fetch_add(
            1, std::memory_order_relaxed);
    }
}

bool TryEnterNativeLinearSpan(CONTEXT* win32_context,
                              ThreadContext* context)
{
    detail::NativeFastPathState* state = &context->native_fast_path;
    if (state->active || state->region_active || state->linear_span_active)
    {
        return false;
    }
    detail::NativeLinearSpan span;
    if (!detail::ScanNativeLinearSpanWithZydis(
            static_cast<std::uint32_t>(win32_context->Eip),
            context->runtime_base,
            context->runtime_size,
            &span))
    {
        state->linear_span_reject_count.fetch_add(
            1, std::memory_order_relaxed);
        return false;
    }
    state->linear_span_boundary = span.boundary_address;
    state->linear_span_instruction_count = span.instruction_count;
    state->linear_span_saved_dr0 =
        static_cast<std::uint32_t>(win32_context->Dr0);
    state->linear_span_saved_dr6 =
        static_cast<std::uint32_t>(win32_context->Dr6);
    state->linear_span_saved_dr7 =
        static_cast<std::uint32_t>(win32_context->Dr7);
    win32_context->Dr0 = span.boundary_address;
    win32_context->Dr6 = 0;
    // Override only slot zero. Dr1-Dr3 are left untouched and the entire Dr7
    // value is restored at the boundary or on any unexpected exception.
    win32_context->Dr7 =
        (static_cast<std::uint32_t>(win32_context->Dr7) & ~0x000F0003U) |
        0x1U;
    win32_context->EFlags &= ~0x00000100U;
    state->linear_span_active = true;
    state->linear_span_entry_count.fetch_add(
        1, std::memory_order_relaxed);
    return true;
}

}  // namespace repiu::platform::win32
