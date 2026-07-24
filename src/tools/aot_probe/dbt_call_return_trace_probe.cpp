#include "dbt_call_return_trace_probe.h"

#include <iostream>

#if defined(_WIN32)
#include "../../platform/win32/aot/aot_dbt_call_return_trace.h"
#include "../../platform/win32/execution/thread_context.h"

#include <memory>
#endif

namespace repiu::tools
{

bool RunAotDbtCallReturnTraceProbe()
{
#if !defined(_WIN32)
    std::cout << "dbt_call_return_trace_probe_skipped=true\n";
    return true;
#else
    using platform::win32::RecordAotDbtCallReturnCall;
    using platform::win32::RecordAotDbtCallReturnReturn;
    using platform::win32::ThreadContext;
    using platform::win32::Win32AotCallReturnTraceEventKind;
    using platform::win32::Win32AotTransferOrigin;

    auto context = std::make_unique<ThreadContext>();
    const bool disabled =
        RecordAotDbtCallReturnCall(
            context.get(), Win32AotTransferOrigin::kHost,
            0x1000U, 0x2000U, 0x1002U, 0x4000U) == 0U &&
        context->aot_dbt_call_return_trace_count == 0U;

    context->aot_dbt_call_return_trace_configured = true;
    const std::uint32_t call_sequence = RecordAotDbtCallReturnCall(
        context.get(), Win32AotTransferOrigin::kHost,
        0x1000U, 0x2000U, 0x1002U, 0x4000U);
    RecordAotDbtCallReturnReturn(
        context.get(), Win32AotTransferOrigin::kHost,
        0x2100U, 0x1002U, 0x3FFCU, call_sequence,
        0x1000U, 0x2000U, 0x1002U, 0x4000U);
    const auto& call = context->aot_dbt_call_return_trace[0];
    const auto& matching_return = context->aot_dbt_call_return_trace[1];
    const bool matching =
        call_sequence == 1U &&
        call.kind == Win32AotCallReturnTraceEventKind::kCall &&
        call.origin == Win32AotTransferOrigin::kHost &&
        call.call_sequence == 1U &&
        call.source == 0x1000U &&
        call.target == 0x2000U &&
        call.return_address == 0x1002U &&
        call.esp == 0x4000U &&
        matching_return.kind ==
            Win32AotCallReturnTraceEventKind::kReturn &&
        matching_return.origin == Win32AotTransferOrigin::kHost &&
        matching_return.call_sequence == call_sequence &&
        matching_return.correlated &&
        matching_return.target_matches &&
        matching_return.esp_matches &&
        context->aot_dbt_call_return_match_count == 1U &&
        context->aot_dbt_call_return_mismatch_count == 0U;

    const std::uint32_t mismatch_call = RecordAotDbtCallReturnCall(
        context.get(), Win32AotTransferOrigin::kVeh,
        0x3000U, 0x5000U, 0x3002U, 0x6000U);
    RecordAotDbtCallReturnReturn(
        context.get(), Win32AotTransferOrigin::kVeh,
        0x5100U, 0x3002U, 0x5FF8U, mismatch_call,
        0x3000U, 0x5000U, 0x3002U, 0x6000U);
    const bool mismatch =
        context->aot_dbt_call_return_mismatch_count == 1U &&
        context->aot_dbt_call_return_first_divergence_valid &&
        context->aot_dbt_call_return_first_divergence.sequence == 4U &&
        context->aot_dbt_call_return_first_divergence.origin ==
            Win32AotTransferOrigin::kVeh &&
        context->aot_dbt_call_return_first_divergence.call_sequence ==
            mismatch_call &&
        context->aot_dbt_call_return_first_divergence.target_matches &&
        !context->aot_dbt_call_return_first_divergence.esp_matches;
    RecordAotDbtCallReturnReturn(
        context.get(), Win32AotTransferOrigin::kVeh,
        0x5200U, 0xDEADBEEFU, 0x5FF8U, mismatch_call,
        0x3000U, 0x5000U, 0x3002U, 0x6000U);
    const bool uncorrelated_filtered =
        context->aot_dbt_call_return_trace_count == 4U &&
        context->aot_dbt_call_return_return_count == 3U &&
        context->aot_dbt_call_return_mismatch_count == 1U;

    auto wrap_context = std::make_unique<ThreadContext>();
    wrap_context->aot_dbt_call_return_trace_configured = true;
    for (std::uint32_t index = 0;
         index < platform::win32::kWin32AotCallReturnTraceCapacity + 4U;
         ++index)
    {
        RecordAotDbtCallReturnCall(
            wrap_context.get(), Win32AotTransferOrigin::kHost,
            0x1000U + index, 0x2000U + index,
            0x1002U + index, 0x4000U);
    }
    bool wrap = wrap_context->aot_dbt_call_return_overwrite_count == 4U;
    const std::uint32_t begin =
        wrap_context->aot_dbt_call_return_trace_count -
        platform::win32::kWin32AotCallReturnTraceCapacity;
    for (std::uint32_t sequence = begin;
         wrap && sequence <
             wrap_context->aot_dbt_call_return_trace_count;
         ++sequence)
    {
        const auto& entry = wrap_context->aot_dbt_call_return_trace[
            sequence %
            platform::win32::kWin32AotCallReturnTraceCapacity];
        wrap = entry.sequence == sequence + 1U;
    }

    const bool passed =
        disabled && matching && mismatch && uncorrelated_filtered && wrap;
    std::cout << "dbt_call_return_trace="
              << (passed ? "true" : "false") << "\n";
    return passed;
#endif
}

}  // namespace repiu::tools
