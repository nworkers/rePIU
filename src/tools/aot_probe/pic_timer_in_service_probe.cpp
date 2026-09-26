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

    const bool ok = idle_ok && inside_ok && other_command_ignored && eoi_ok &&
        sti_ok && specific_ok && chain_ok && sti_again_ok && retired_ok;
    std::cout << "pic_timer_in_service=" << (ok ? "true" : "false")
              << ",idle=" << (idle_ok ? "true" : "false")
              << ",inside=" << (inside_ok ? "true" : "false")
              << ",other_command=" << (other_command_ignored ? "true" : "false")
              << ",eoi=" << (eoi_ok ? "true" : "false")
              << ",sti=" << (sti_ok ? "true" : "false")
              << ",specific_eoi=" << (specific_ok ? "true" : "false")
              << ",sti_chain=" << (chain_ok ? "true" : "false")
              << ",sti_again=" << (sti_again_ok ? "true" : "false")
              << ",stack_retire=" << (retired_ok ? "true" : "false") << "\n";
    return ok;
}

}  // namespace repiu::tools
