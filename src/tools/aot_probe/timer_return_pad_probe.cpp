#include "timer_return_pad_probe.h"

#include "repiu/engine/timer_return_pad.h"

#include <cstdint>
#include <iostream>

namespace repiu::tools
{

// Task 762. The records behind the timer return pad. The addresses are
// pumpit8's on Win32: the wait loop at 0x0401DB18 in place, a safe point in
// the cache at 0x0E7952A7, frames around 0x051CEA90.
bool RunTimerReturnPadProbe()
{
    using engine::PopTimerReturn;
    using engine::PushTimerReturn;
    using engine::ResolveTimerReturnPadEnabled;
    using engine::TimerReturnPad;
    using engine::TimerReturnRecord;

    constexpr std::uint32_t kLoop = 0x0401DB18U;
    constexpr std::uint32_t kCache = 0x0E7952A7U;
    constexpr std::uint32_t kStack = 0x051CEAA0U;
    constexpr std::uint32_t kFrame = kStack - 12U;
    constexpr std::uint32_t kIsrDepth = 0x40U;

    // On unless it is switched off; anything unknown is off.
    const bool switch_ok = ResolveTimerReturnPadEnabled(nullptr) &&
        ResolveTimerReturnPadEnabled("") &&
        ResolveTimerReturnPadEnabled("1") &&
        !ResolveTimerReturnPadEnabled("0") &&
        !ResolveTimerReturnPadEnabled("off") &&
        !ResolveTimerReturnPadEnabled("false") &&
        !ResolveTimerReturnPadEnabled("maybe");

    // Nothing injected: a fault at the pad has nothing to return to.
    TimerReturnPad empty;
    TimerReturnRecord taken;
    const bool empty_ok = !PopTimerReturn(&empty, kStack, &taken) &&
        empty.returned_total == 0U;

    // One frame and its return, with the interrupted code's state intact.
    TimerReturnPad single;
    const bool pushed =
        PushTimerReturn(&single, {kLoop, kFrame, true, true, false}, kStack);
    const bool single_ok = pushed && single.depth == 1U &&
        PopTimerReturn(&single, kStack, &taken) &&
        taken.return_eip == kLoop && taken.frame_esp == kFrame &&
        taken.single_step_trace && taken.legacy_fallback &&
        !taken.reentry_pending && single.depth == 0U &&
        single.pushed_total == 1U && single.returned_total == 1U &&
        single.unmatched_total == 0U && single.abandoned_total == 0U;

    // A tick taken at the handler's closing `sti` nests a second frame; the
    // inner handler returns first.
    TimerReturnPad nested;
    const std::uint32_t inner_stack = kFrame - kIsrDepth;
    PushTimerReturn(&nested, {kCache, kFrame, false, false, false}, kStack);
    PushTimerReturn(&nested, {kLoop, inner_stack - 12U, false, false, false},
                    inner_stack);
    const bool inner_first = PopTimerReturn(&nested, inner_stack, &taken) &&
        taken.return_eip == kLoop && nested.depth == 1U;
    const bool nested_ok = inner_first &&
        PopTimerReturn(&nested, kStack, &taken) &&
        taken.return_eip == kCache && nested.depth == 0U &&
        nested.depth_max == 2U && nested.abandoned_total == 0U;

    // The inner handler never returns -- the guest dropped its frame -- and
    // the outer one does. The inner record goes with it.
    TimerReturnPad dropped;
    PushTimerReturn(&dropped, {kCache, kFrame, false, false, false}, kStack);
    PushTimerReturn(&dropped, {kLoop, inner_stack - 12U, false, false, false},
                    inner_stack);
    const bool dropped_ok = PopTimerReturn(&dropped, kStack, &taken) &&
        taken.return_eip == kCache && dropped.depth == 0U &&
        dropped.abandoned_total == 1U && dropped.unmatched_total == 0U;

    // Two frames at the same ESP, the older one abandoned: the newer record
    // is the one that returns.
    TimerReturnPad same;
    PushTimerReturn(&same, {kCache, kFrame, false, false, false}, kStack);
    PushTimerReturn(&same, {kLoop, kFrame, true, false, false}, kStack);
    const bool same_ok = PopTimerReturn(&same, kStack, &taken) &&
        taken.return_eip == kLoop && same.depth == 1U;

    // A return on a stack no record names takes the newest record and says
    // so.
    TimerReturnPad unmatched;
    PushTimerReturn(&unmatched, {kLoop, kFrame, false, false, false}, kStack);
    const bool unmatched_ok =
        PopTimerReturn(&unmatched, kStack + 0x100U, &taken) &&
        taken.return_eip == kLoop && unmatched.unmatched_total == 1U &&
        unmatched.depth == 0U;

    // Every slot taken by handlers that are still running: the next injection
    // goes without the pad.
    TimerReturnPad full;
    bool filled = true;
    for (std::uint32_t index = 0U; index < TimerReturnPad::kCapacity; ++index)
    {
        const std::uint32_t stack = kStack - index * kIsrDepth;
        filled = filled &&
            PushTimerReturn(&full, {kLoop, stack - 12U, false, false, false},
                            stack);
    }
    const std::uint32_t deepest =
        kStack - TimerReturnPad::kCapacity * kIsrDepth;
    const bool overflow_ok = filled &&
        !PushTimerReturn(&full, {kLoop, deepest - 12U, false, false, false},
                         deepest) &&
        full.overflow_total == 1U && full.abandoned_total == 0U &&
        full.depth == TimerReturnPad::kCapacity;

    // Every slot taken by frames the stack has since risen above: they are
    // dropped and the injection keeps its record.
    const bool pruned_ok =
        PushTimerReturn(&full, {kCache, kStack + 0x100U - 12U, false, false,
                                false},
                        kStack + 0x100U) &&
        full.abandoned_total == TimerReturnPad::kCapacity &&
        full.depth == 1U && full.overflow_total == 1U;

    const bool ok = switch_ok && empty_ok && single_ok && nested_ok &&
        dropped_ok && same_ok && unmatched_ok && overflow_ok && pruned_ok;
    std::cout << "timer_return_pad=" << (ok ? "true" : "false")
              << ",switch=" << (switch_ok ? "true" : "false")
              << ",empty=" << (empty_ok ? "true" : "false")
              << ",single=" << (single_ok ? "true" : "false")
              << ",nested=" << (nested_ok ? "true" : "false")
              << ",dropped=" << (dropped_ok ? "true" : "false")
              << ",same_frame=" << (same_ok ? "true" : "false")
              << ",unmatched=" << (unmatched_ok ? "true" : "false")
              << ",overflow=" << (overflow_ok ? "true" : "false")
              << ",pruned=" << (pruned_ok ? "true" : "false") << "\n";
    return ok;
}

}  // namespace repiu::tools
