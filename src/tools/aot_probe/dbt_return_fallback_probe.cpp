#include "dbt_return_fallback_probe.h"

#include <iostream>

#if defined(_WIN32)
#include "../../engine/aot/aot_dbt_return_dispatch.h"
#include "../../engine/execution/thread_context.h"

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
        std::make_unique<engine::ThreadContext>();
    for (std::uint32_t index = 0;
         index < engine::kAotDbtDispatchFallbackReasonCount; ++index)
    {
        engine::RecordAotDbtReturnFallback(
            context.get(),
            static_cast<engine::AotDbtDispatchFallbackReason>(index));
    }

    const std::uint32_t total =
        context->aot_dbt_return_fallback_count.load(
            std::memory_order_relaxed);
    std::uint32_t reason_total = 0;
    bool slots = true;
    for (std::uint32_t index = 0;
         index < engine::kAotDbtDispatchFallbackReasonCount; ++index)
    {
        const std::uint32_t count =
            context->aot_dbt_return_fallback_reason_counts[index].load(
                std::memory_order_relaxed);
        reason_total += count;
        slots = slots && count == 1U;
    }
    const bool accounting =
        total == engine::kAotDbtDispatchFallbackReasonCount &&
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
