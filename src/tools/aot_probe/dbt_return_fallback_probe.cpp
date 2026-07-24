#include "dbt_return_fallback_probe.h"

#include <iostream>

#if defined(_WIN32)
#include "../../platform/win32/aot/aot_dbt_return_dispatch.h"
#include "../../platform/win32/execution/thread_context.h"

#include <memory>
#endif

namespace repiu::tools
{

bool RunAotDbtReturnFallbackProbe()
{
#if !defined(_WIN32)
    std::cout << "dbt_return_fallback_probe_skipped=true\n";
    return true;
#else
    auto context =
        std::make_unique<platform::win32::ThreadContext>();
    for (std::uint32_t index = 0;
         index < platform::win32::kAotDbtDispatchFallbackReasonCount; ++index)
    {
        platform::win32::RecordAotDbtReturnFallback(
            context.get(),
            static_cast<platform::win32::AotDbtDispatchFallbackReason>(index));
    }

    const std::uint32_t total =
        context->aot_dbt_return_fallback_count.load(
            std::memory_order_relaxed);
    std::uint32_t reason_total = 0;
    bool slots = true;
    for (std::uint32_t index = 0;
         index < platform::win32::kAotDbtDispatchFallbackReasonCount; ++index)
    {
        const std::uint32_t count =
            context->aot_dbt_return_fallback_reason_counts[index].load(
                std::memory_order_relaxed);
        reason_total += count;
        slots = slots && count == 1U;
    }
    const bool accounting =
        total == platform::win32::kAotDbtDispatchFallbackReasonCount &&
        total == reason_total;
    std::cout << "dbt_return_fallback_slots="
              << (slots ? "true" : "false")
              << "\ndbt_return_fallback_accounting="
              << (accounting ? "true" : "false")
              << "\ndbt_return_fallback_all="
              << (slots && accounting ? "true" : "false") << "\n";
    return slots && accounting;
#endif
}

}  // namespace repiu::tools
