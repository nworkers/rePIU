#include "aot_dbt_call_return_trace.h"

#include "thread_context.h"

namespace repiu::platform::win32
{
namespace
{

void AppendTraceEntry(ThreadContext* context,
                      Win32AotCallReturnTraceEntry entry)
{
    const std::uint32_t sequence =
        context->aot_dbt_call_return_trace_count + 1U;
    entry.sequence = sequence;
    const std::uint32_t slot =
        (sequence - 1U) % kWin32AotCallReturnTraceCapacity;
    context->aot_dbt_call_return_trace[slot] = entry;
    context->aot_dbt_call_return_trace_count = sequence;
    if (sequence > kWin32AotCallReturnTraceCapacity)
    {
        ++context->aot_dbt_call_return_overwrite_count;
    }
}

}  // namespace

std::uint32_t RecordAotDbtCallReturnCall(
    ThreadContext* context,
    Win32AotTransferOrigin origin,
    std::uint32_t source,
    std::uint32_t target,
    std::uint32_t return_address,
    std::uint32_t entry_esp)
{
    if (context == nullptr ||
        !context->aot_dbt_call_return_trace_configured)
    {
        return 0;
    }
    Win32AotCallReturnTraceEntry entry;
    entry.kind = Win32AotCallReturnTraceEventKind::kCall;
    entry.origin = origin;
    entry.source = source;
    entry.target = target;
    entry.return_address = return_address;
    entry.esp = entry_esp;
    ++context->aot_dbt_call_return_call_count;
    AppendTraceEntry(context, entry);
    const std::uint32_t sequence =
        context->aot_dbt_call_return_trace_count;
    context->aot_dbt_call_return_trace[
        (sequence - 1U) % kWin32AotCallReturnTraceCapacity]
        .call_sequence = sequence;
    return sequence;
}

void RecordAotDbtCallReturnReturn(
    ThreadContext* context,
    Win32AotTransferOrigin origin,
    std::uint32_t source,
    std::uint32_t target,
    std::uint32_t esp,
    std::uint32_t call_sequence,
    std::uint32_t expected_source,
    std::uint32_t expected_target,
    std::uint32_t expected_return_address,
    std::uint32_t call_entry_esp)
{
    if (context == nullptr ||
        !context->aot_dbt_call_return_trace_configured)
    {
        return;
    }
    Win32AotCallReturnTraceEntry entry;
    entry.kind = Win32AotCallReturnTraceEventKind::kReturn;
    entry.origin = origin;
    entry.call_sequence = call_sequence;
    entry.source = source;
    entry.target = target;
    entry.return_address = target;
    entry.esp = esp;
    entry.expected_source = expected_source;
    entry.expected_target = expected_target;
    entry.expected_return_address = expected_return_address;
    entry.expected_esp =
        call_entry_esp >= sizeof(std::uint32_t)
            ? call_entry_esp - sizeof(std::uint32_t)
            : 0U;
    if (call_sequence != 0U)
    {
        entry.target_matches = target == expected_return_address;
        entry.esp_matches = esp == entry.expected_esp;
        // A matching target uniquely identifies the tracked CALL. An ESP-only
        // match is insufficient because an inline-cache-hit return is outside
        // this observer and a later unrelated call can reuse the same stack
        // depth. Treating that reuse as correlation creates false target
        // divergences in the clean VEH control.
        entry.correlated = entry.target_matches;
    }
    ++context->aot_dbt_call_return_return_count;
    if (!entry.correlated)
    {
        // Count every dispatcher-visible RET, but keep the bounded event ring
        // focused on CALLs and RETs that actually identify one of those CALLs.
        // Otherwise thousands of unrelated return misses overwrite all CALL
        // tuples before the final crash snapshot is copied.
        return;
    }
    if (entry.correlated && entry.target_matches && entry.esp_matches)
    {
        ++context->aot_dbt_call_return_match_count;
    }
    else if (entry.correlated)
    {
        ++context->aot_dbt_call_return_mismatch_count;
        if (!context->aot_dbt_call_return_first_divergence_valid)
        {
            context->aot_dbt_call_return_first_divergence_valid = true;
            context->aot_dbt_call_return_first_divergence = entry;
        }
    }
    AppendTraceEntry(context, entry);
    if (context->aot_dbt_call_return_first_divergence_valid &&
        context->aot_dbt_call_return_first_divergence.sequence == 0U)
    {
        context->aot_dbt_call_return_first_divergence.sequence =
            context->aot_dbt_call_return_trace_count;
    }
}

}  // namespace repiu::platform::win32
