#include "repiu/engine/pic_timer_in_service.h"

#include <cstdlib>

namespace repiu::engine
{

bool ResolvePicTimerInServiceEnabled(const char* const value)
{
    return value == nullptr || value[0] != '0' || value[1] != '\0';
}

bool PicTimerInServiceEnabled()
{
    static const bool enabled = ResolvePicTimerInServiceEnabled(
        std::getenv("REPIU_PIC_TIMER_IN_SERVICE"));
    return enabled;
}

void NotePicTimerInjected(PicTimerInService* const state,
                          const std::uint32_t frame_esp, const bool at_sti)
{
    if (state == nullptr)
    {
        return;
    }
    state->active = true;
    state->frame_esp = frame_esp;
    state->last_delivery_at_sti = at_sti;
    state->last_delivery_returned = false;
    if (state->handler_depth < PicTimerInService::kHandlerFrameCapacity)
    {
        state->handler_frames[state->handler_depth++] = frame_esp;
    }
    else
    {
        state->handler_frames[
            PicTimerInService::kHandlerFrameCapacity - 1U] = frame_esp;
    }
    if (at_sti)
    {
        ++state->delivered_at_sti_total;
    }
}

void NoteTimerInjectionTime(PicTimerInService* const state,
                            const std::uint64_t now_ns)
{
    if (state != nullptr && state->handler_depth == 1U)
    {
        state->outer_injection_ns = now_ns;
    }
}

bool PostReturnChainBlocksInjection(PicTimerInService* const state,
                                    const TimerInjectionSite site,
                                    const std::uint64_t now_ns,
                                    const std::uint64_t tick_period_ns)
{
    if (state == nullptr)
    {
        return false;
    }
    if (site == TimerInjectionSite::kOrdinary)
    {
        // The interrupted code's turn: as long as the last handler took.
        if (state->handler_return_seen && state->handler_depth == 0U &&
            now_ns >= state->last_return_ns &&
            now_ns - state->last_return_ns < state->last_handler_ns)
        {
            ++state->blocked_by_turn_total;
            return true;
        }
        state->post_return_chain = 0U;
        return false;
    }
    if (tick_period_ns < kPostReturnChainMinimumPeriodNanoseconds ||
        state->post_return_chain >= kPostReturnChainLimit)
    {
        ++state->blocked_post_return_total;
        return true;
    }
    ++state->post_return_chain;
    ++state->post_return_delivered_total;
    return false;
}

bool ResolveGuestCliHoldEnabled(const char* const value)
{
    return value == nullptr || value[0] != '0' || value[1] != '\0';
}

bool GuestCliHoldEnabled()
{
    static const bool enabled = ResolveGuestCliHoldEnabled(
        std::getenv("REPIU_GUEST_CLI_HOLD"));
    return enabled;
}

namespace
{

void EndGuestCliHold(PicTimerInService* const state,
                     const std::uint64_t now_ns)
{
    if (!state->cli_hold)
    {
        return;
    }
    state->cli_hold = false;
    const std::uint64_t held =
        now_ns > state->cli_hold_since_ns
            ? now_ns - state->cli_hold_since_ns : 0U;
    if (held > state->cli_hold_max_ns)
    {
        state->cli_hold_max_ns = held;
    }
}

}  // namespace

void NoteGuestCli(PicTimerInService* const state, const std::uint64_t now_ns)
{
    if (state == nullptr)
    {
        return;
    }
    ++state->cli_total;
    // A second `cli` inside a hold does not restart its clock.
    if (!state->cli_hold)
    {
        state->cli_hold = true;
        state->cli_hold_since_ns = now_ns;
    }
}

void NoteGuestSti(PicTimerInService* const state, const std::uint64_t now_ns)
{
    if (state == nullptr)
    {
        return;
    }
    ++state->sti_total;
    EndGuestCliHold(state, now_ns);
}

void NoteGuestIret(PicTimerInService* const state,
                   const std::uint32_t frame_esp, const std::uint64_t now_ns)
{
    if (state == nullptr)
    {
        return;
    }
    if (state->handler_depth != 0U &&
        state->handler_frames[state->handler_depth - 1U] == frame_esp)
    {
        --state->handler_depth;
        state->handler_return_seen = true;
        if (state->handler_depth == 0U)
        {
            state->last_return_ns = now_ns;
            state->last_handler_ns =
                now_ns > state->outer_injection_ns
                    ? now_ns - state->outer_injection_ns : 0U;
            if (state->last_handler_ns > state->handler_max_ns)
            {
                state->handler_max_ns = state->last_handler_ns;
            }
        }
        // The interrupted code had IF set, or the tick would not have gone
        // in.
        EndGuestCliHold(state, now_ns);
    }
    if (frame_esp == state->frame_esp)
    {
        state->last_delivery_returned = true;
    }
}

bool GuestCliBlocksInjection(PicTimerInService* const state,
                             const std::uint64_t now_ns)
{
    if (state == nullptr || !state->cli_hold)
    {
        return false;
    }
    if (now_ns > state->cli_hold_since_ns &&
        now_ns - state->cli_hold_since_ns > kGuestCliHoldLimitNanoseconds)
    {
        EndGuestCliHold(state, now_ns);
        ++state->cli_hold_expired_total;
        return false;
    }
    ++state->blocked_by_cli_total;
    return true;
}

bool NotePicCommand(PicTimerInService* const state,
                    const std::uint8_t command)
{
    // OCW2: 0x20 is a non-specific EOI, 0x60 | level a specific EOI.
    const bool ends_irq0 = command == 0x20U || command == 0x60U;
    if (state == nullptr || !ends_irq0)
    {
        return false;
    }
    if (state->active)
    {
        state->active = false;
        ++state->cleared_by_eoi_total;
    }
    return true;
}

bool PicTimerBlocksInjection(PicTimerInService* const state,
                             const std::uint32_t current_esp,
                             const bool at_sti)
{
    if (state == nullptr)
    {
        return false;
    }
    if (current_esp > state->frame_esp)
    {
        state->last_delivery_returned = true;
    }
    // A frame the stack stands above has been popped, seen or not.
    while (state->handler_depth != 0U &&
           current_esp > state->handler_frames[state->handler_depth - 1U])
    {
        --state->handler_depth;
    }
    if (state->handler_return_seen &&
        (state->handler_depth >= 2U ||
         (state->handler_depth == 1U && !at_sti)))
    {
        ++state->blocked_nested_total;
        return true;
    }
    if (state->active && current_esp > state->frame_esp)
    {
        state->active = false;
        ++state->retired_by_stack_total;
    }
    if (state->active)
    {
        ++state->blocked_in_service_total;
        return true;
    }
    if (at_sti && state->last_delivery_at_sti &&
        !state->last_delivery_returned)
    {
        ++state->blocked_sti_chain_total;
        return true;
    }
    return false;
}

}  // namespace repiu::engine
