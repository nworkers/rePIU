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
    if (at_sti)
    {
        ++state->delivered_at_sti_total;
    }
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
    if (at_sti && state->last_delivery_at_sti)
    {
        ++state->blocked_sti_chain_total;
        return true;
    }
    return false;
}

}  // namespace repiu::engine
