#include "pic_timer_in_service_probe.h"

#include "repiu/engine/pic_timer_in_service.h"

#include <cstdint>
#include <iostream>

namespace repiu::tools
{

// Task 735. When an owed timer tick may be injected. The stack addresses are
// pumpitea's: its first injected frame sat at 0x01110FF0 and each nested frame
// 60 bytes below the one before.
bool RunPicTimerInServiceProbe()
{
    using engine::NotePicCommand;
    using engine::NotePicTimerInjected;
    using engine::PicTimerBlocksInjection;
    using engine::PicTimerInService;

    constexpr std::uint32_t kFrameEsp = 0x01110FF0U;
    constexpr std::uint32_t kIsrDepth = 0x3CU;

    // Nothing injected yet: nothing to wait for, at a safe point or at `sti`.
    PicTimerInService idle;
    const bool idle_ok = !PicTimerBlocksInjection(&idle, kFrameEsp, false) &&
        !PicTimerBlocksInjection(&idle, kFrameEsp, true);

    // Inside the handler before its EOI the next tick waits, however often it
    // is asked -- the case that nested pumpitea's handler until overflow.
    PicTimerInService inside;
    NotePicTimerInjected(&inside, kFrameEsp, false);
    const bool inside_ok =
        PicTimerBlocksInjection(&inside, kFrameEsp, false) &&
        PicTimerBlocksInjection(&inside, kFrameEsp - kIsrDepth, false) &&
        PicTimerBlocksInjection(&inside, kFrameEsp - kIsrDepth, true) &&
        inside.blocked_in_service_total == 3U && inside.active;

    // The EOI ends service -- the non-specific form and the specific EOI for
    // IRQ0; other OCW2 values do not.
    PicTimerInService eoi;
    NotePicTimerInjected(&eoi, kFrameEsp, false);
    const bool other_command_ignored =
        !NotePicCommand(&eoi, 0x0BU) && eoi.active;
    const bool eoi_ok = NotePicCommand(&eoi, 0x20U) && !eoi.active &&
        eoi.cleared_by_eoi_total == 1U;

    // The handler's closing `sti` may then take the next tick...
    const bool sti_ok = !PicTimerBlocksInjection(&eoi, kFrameEsp - kIsrDepth,
                                                 true);
    NotePicTimerInjected(&eoi, kFrameEsp - kIsrDepth - 12U, true);
    const bool specific_ok = NotePicCommand(&eoi, 0x60U) && !eoi.active &&
        eoi.cleared_by_eoi_total == 2U && eoi.delivered_at_sti_total == 1U;
    // ...but the nested handler's own closing `sti` may not chain a third, or
    // a timer faster than the emulated handler would never let the outer one
    // return. The interrupted code's next safe point may.
    const bool chain_ok =
        PicTimerBlocksInjection(&eoi, kFrameEsp - 2U * kIsrDepth, true) &&
        eoi.blocked_sti_chain_total == 1U &&
        !PicTimerBlocksInjection(&eoi, kFrameEsp - 0x200U, false);
    NotePicTimerInjected(&eoi, kFrameEsp - 0x200U - 12U, false);
    NotePicCommand(&eoi, 0x20U);
    const bool sti_again_ok =
        !PicTimerBlocksInjection(&eoi, kFrameEsp - 0x200U - kIsrDepth, true);

    // A handler that never writes an EOI still ends: once the guest stack is
    // above the injected frame, the handler has returned.
    PicTimerInService no_eoi;
    NotePicTimerInjected(&no_eoi, kFrameEsp, false);
    const bool retired_ok =
        PicTimerBlocksInjection(&no_eoi, kFrameEsp, false) &&
        !PicTimerBlocksInjection(&no_eoi, kFrameEsp + 12U, false) &&
        !no_eoi.active && no_eoi.retired_by_stack_total == 1U;

    // Task 751. The guest's own `cli`. Times are nanoseconds on one clock.
    using engine::GuestCliBlocksInjection;
    using engine::kGuestCliHoldLimitNanoseconds;
    using engine::NoteGuestCli;
    using engine::NoteGuestIret;
    using engine::NoteGuestSti;
    constexpr std::uint64_t kStart = 1'000'000'000ULL;
    constexpr std::uint64_t kMillisecond = 1'000'000ULL;

    // No `cli`, no hold.
    PicTimerInService no_hold;
    const bool no_hold_ok = !GuestCliBlocksInjection(&no_hold, kStart) &&
        no_hold.blocked_by_cli_total == 0U;

    // `cli` ... `sti`: held in between, free afterwards, and the longest hold
    // is remembered. A second `cli` inside the hold does not restart it.
    PicTimerInService pair;
    NoteGuestCli(&pair, kStart);
    const bool held = GuestCliBlocksInjection(&pair, kStart + kMillisecond);
    NoteGuestCli(&pair, kStart + 2U * kMillisecond);
    const bool still_held =
        GuestCliBlocksInjection(&pair, kStart + 8U * kMillisecond);
    NoteGuestSti(&pair, kStart + 9U * kMillisecond);
    const bool pair_ok = held && still_held && !pair.cli_hold &&
        !GuestCliBlocksInjection(&pair, kStart + 10U * kMillisecond) &&
        pair.cli_total == 2U && pair.sti_total == 1U &&
        pair.blocked_by_cli_total == 2U &&
        pair.cli_hold_max_ns == 9U * kMillisecond &&
        pair.cli_hold_expired_total == 0U;

    // The `iret` of the injected frame ends the hold its handler left; the
    // `iret` of any other frame -- the previous INT 8 handler the guest's
    // chains to, returning inside the handler -- does not.
    PicTimerInService by_iret;
    NotePicTimerInjected(&by_iret, kFrameEsp, false);
    NoteGuestCli(&by_iret, kStart);
    NoteGuestIret(&by_iret, kFrameEsp - kIsrDepth, kStart + kMillisecond);
    const bool other_iret_keeps =
        by_iret.cli_hold && !by_iret.last_delivery_returned;
    NoteGuestIret(&by_iret, kFrameEsp, kStart + 2U * kMillisecond);
    const bool iret_ok = other_iret_keeps && !by_iret.cli_hold &&
        by_iret.last_delivery_returned &&
        !GuestCliBlocksInjection(&by_iret, kStart + 3U * kMillisecond);

    // A hold the guest ended unseen (`popfd`, a native `iret`) expires at the
    // limit instead of stopping the clock: held up to it, free just past it.
    PicTimerInService unseen;
    NoteGuestCli(&unseen, kStart);
    const bool held_at_limit = GuestCliBlocksInjection(
        &unseen, kStart + kGuestCliHoldLimitNanoseconds);
    const bool free_past_limit = !GuestCliBlocksInjection(
        &unseen, kStart + kGuestCliHoldLimitNanoseconds + 1U);
    const bool expiry_ok = held_at_limit && free_past_limit &&
        !unseen.cli_hold && unseen.cli_hold_expired_total == 1U &&
        unseen.blocked_by_cli_total == 1U;

    // The switch: only an exact "0" turns it off.
    const bool switch_ok = engine::ResolveGuestCliHoldEnabled(nullptr) &&
        engine::ResolveGuestCliHoldEnabled("1") &&
        engine::ResolveGuestCliHoldEnabled("") &&
        !engine::ResolveGuestCliHoldEnabled("0");

    // The chain rule ends with the handler. A tick delivered at a `sti` whose
    // handler has returned -- seen by its `iret`, or by the stack standing
    // above the injected frame -- does not hold back the next `sti`.
    PicTimerInService sections;
    NotePicTimerInjected(&sections, kFrameEsp, true);
    NotePicCommand(&sections, 0x20U);
    const bool own_sti_blocked =
        PicTimerBlocksInjection(&sections, kFrameEsp - kIsrDepth, true);
    // The chained handler's `iret`, inside the body, changes nothing.
    NoteGuestIret(&sections, kFrameEsp - kIsrDepth, kStart);
    const bool chained_iret_still_blocked =
        PicTimerBlocksInjection(&sections, kFrameEsp - kIsrDepth, true);
    NoteGuestIret(&sections, kFrameEsp, kStart);
    const bool after_iret_free =
        !PicTimerBlocksInjection(&sections, kFrameEsp - kIsrDepth, true);
    NotePicTimerInjected(&sections, kFrameEsp, true);
    NotePicCommand(&sections, 0x20U);
    const bool after_stack_free =
        !PicTimerBlocksInjection(&sections, kFrameEsp + 12U, true);
    const bool sections_ok = own_sti_blocked &&
        chained_iret_still_blocked && after_iret_free && after_stack_free &&
        sections.blocked_sti_chain_total == 2U;

    // The chain after a handler's return is bounded, and an ordinary
    // delivery -- the interrupted code having run -- starts it again.
    using engine::kPostReturnChainLimit;
    using engine::PostReturnChainBlocksInjection;
    using engine::TimerInjectionSite;
    // 240 Hz and the 51.9 kHz stage, as tick periods.
    constexpr std::uint64_t kGamePeriod = 4'166'000ULL;
    constexpr std::uint64_t kFastPeriod = 19'276ULL;
    PicTimerInService chain;
    bool chain_allowed = true;
    for (std::uint32_t index = 0; index < kPostReturnChainLimit; ++index)
    {
        chain_allowed = chain_allowed &&
            !PostReturnChainBlocksInjection(
                &chain, TimerInjectionSite::kAfterHandlerReturn, kStart,
                kGamePeriod);
    }
    const bool chain_stops = PostReturnChainBlocksInjection(
        &chain, TimerInjectionSite::kAfterHandlerReturn, kStart, kGamePeriod);
    const bool ordinary_free = !PostReturnChainBlocksInjection(
        &chain, TimerInjectionSite::kOrdinary, kStart, kGamePeriod);
    const bool chain_again = !PostReturnChainBlocksInjection(
        &chain, TimerInjectionSite::kAfterHandlerReturn, kStart, kGamePeriod);
    // At the fast stage's period no tick goes in right after a return.
    PicTimerInService fast;
    const bool fast_no_chain = PostReturnChainBlocksInjection(
        &fast, TimerInjectionSite::kAfterHandlerReturn, kStart, kFastPeriod);
    const bool post_return_ok = chain_allowed && chain_stops &&
        ordinary_free && chain_again && fast_no_chain &&
        chain.post_return_delivered_total == kPostReturnChainLimit + 1U &&
        chain.blocked_post_return_total == 1U &&
        fast.blocked_post_return_total == 1U;

    // The interrupted code's turn: after a handler that took 55 us, no tick
    // for the next 55 us, then one.
    PicTimerInService turn;
    NotePicTimerInjected(&turn, kFrameEsp, false);
    engine::NoteTimerInjectionTime(&turn, kStart);
    NotePicCommand(&turn, 0x20U);
    NoteGuestIret(&turn, kFrameEsp, kStart + 55'000ULL);
    const bool turn_held = PostReturnChainBlocksInjection(
        &turn, TimerInjectionSite::kOrdinary, kStart + 55'000ULL + 54'999ULL,
        kFastPeriod);
    const bool turn_over = !PostReturnChainBlocksInjection(
        &turn, TimerInjectionSite::kOrdinary, kStart + 110'000ULL,
        kFastPeriod);
    const bool turn_ok = turn_held && turn_over &&
        turn.last_handler_ns == 55'000ULL && turn.blocked_by_turn_total == 1U;

    // Nesting. Where a handler's `iret` is seen, a tick goes into interrupted
    // code or, one level deep, at a handler's closing `sti`; never at a safe
    // point inside a handler, and never two levels deep.
    PicTimerInService nest;
    NotePicTimerInjected(&nest, kFrameEsp, false);
    NotePicCommand(&nest, 0x20U);
    // Before any return has been seen the rule is off (a host whose `iret`
    // runs natively), so a safe point inside the handler still takes one.
    const bool unseen_allows =
        !PicTimerBlocksInjection(&nest, kFrameEsp - kIsrDepth, false);
    NoteGuestIret(&nest, kFrameEsp, kStart);
    const bool depth_zero = nest.handler_depth == 0U &&
        nest.handler_return_seen;
    NotePicTimerInjected(&nest, kFrameEsp, false);
    NotePicCommand(&nest, 0x20U);
    const bool inside_safe_point_blocked =
        PicTimerBlocksInjection(&nest, kFrameEsp - kIsrDepth, false);
    const bool inside_sti_allowed =
        !PicTimerBlocksInjection(&nest, kFrameEsp - kIsrDepth, true);
    NotePicTimerInjected(&nest, kFrameEsp - kIsrDepth - 12U, true);
    NotePicCommand(&nest, 0x20U);
    const bool two_deep_blocked =
        PicTimerBlocksInjection(&nest, kFrameEsp - 2U * kIsrDepth, false) &&
        PicTimerBlocksInjection(&nest, kFrameEsp - 2U * kIsrDepth, true);
    NoteGuestIret(&nest, kFrameEsp - kIsrDepth - 12U, kStart);
    NoteGuestIret(&nest, kFrameEsp, kStart);
    const bool returned_allows = nest.handler_depth == 0U &&
        !PicTimerBlocksInjection(&nest, kFrameEsp - 0x200U, false);
    const bool nesting_ok = unseen_allows && depth_zero &&
        inside_safe_point_blocked && inside_sti_allowed && two_deep_blocked &&
        returned_allows && nest.blocked_nested_total == 3U;

    const bool cli_hold_ok =
        no_hold_ok && pair_ok && iret_ok && expiry_ok && switch_ok &&
        sections_ok && post_return_ok && nesting_ok && turn_ok;

    const bool ok = idle_ok && inside_ok && other_command_ignored && eoi_ok &&
        sti_ok && specific_ok && chain_ok && sti_again_ok && retired_ok &&
        cli_hold_ok;
    std::cout << "pic_timer_in_service=" << (ok ? "true" : "false")
              << ",idle=" << (idle_ok ? "true" : "false")
              << ",inside=" << (inside_ok ? "true" : "false")
              << ",other_command=" << (other_command_ignored ? "true" : "false")
              << ",eoi=" << (eoi_ok ? "true" : "false")
              << ",sti=" << (sti_ok ? "true" : "false")
              << ",specific_eoi=" << (specific_ok ? "true" : "false")
              << ",sti_chain=" << (chain_ok ? "true" : "false")
              << ",sti_again=" << (sti_again_ok ? "true" : "false")
              << ",stack_retire=" << (retired_ok ? "true" : "false")
              << ",cli_hold=" << (cli_hold_ok ? "true" : "false")
              << ",cli_pair=" << (pair_ok ? "true" : "false")
              << ",cli_iret=" << (iret_ok ? "true" : "false")
              << ",cli_expiry=" << (expiry_ok ? "true" : "false")
              << ",sti_sections=" << (sections_ok ? "true" : "false")
              << ",post_return_chain=" << (post_return_ok ? "true" : "false")
              << ",nesting=" << (nesting_ok ? "true" : "false")
              << ",turn=" << (turn_ok ? "true" : "false") << "\n";
    return ok;
}

}  // namespace repiu::tools
