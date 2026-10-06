#include "segment_restore_probe.h"

#include "repiu/platform/fault_handler.h"

#include <cstdint>
#include <cstdio>

namespace repiu::tools
{
namespace
{

using repiu::platform::FaultDisposition;
using repiu::platform::FaultEvent;
using repiu::platform::FaultKind;

// A looping handler is worse than a failing probe; see fault_handler_probe.
constexpr int kMaxHandlerEntries = 8;

// The flat data selector every Win32 x86 user thread runs with. Used as the
// control candidate and as the recovery value when a resume goes wrong.
constexpr std::uint16_t kFlatSelector = 0x002BU;

struct ProbeState
{
    std::uint16_t requested = 0;
    int entries = 0;
    bool recovered = false;
};

ProbeState g_state;

FaultDisposition OnFault(FaultEvent* event, void* user_data)
{
    auto* state = static_cast<ProbeState*>(user_data);
    if (event == nullptr || state == nullptr || event->registers == nullptr)
    {
        return FaultDisposition::kNotHandled;
    }
    ++state->entries;
    if (state->entries > kMaxHandlerEntries)
    {
        return FaultDisposition::kNotHandled;
    }
    if (event->kind == FaultKind::kBreakpoint)
    {
        // Resume one byte past the int3 with the candidate selector in the
        // context, exactly the shape of the engine's HLE segment load.
        event->registers->Eip += 1U;
        event->registers->SegEs = state->requested;
        return FaultDisposition::kResume;
    }
    // Anything else is the resume itself going wrong (a rejected or faulting
    // segment restore). Put the flat selector back and let the run continue
    // so the per-candidate report shows the recovery instead of the process
    // dying mid-probe.
    state->recovered = true;
    event->registers->SegEs = kFlatSelector;
    return FaultDisposition::kResume;
}

std::uint16_t ReadPhysicalEs()
{
    std::uint16_t value = 0;
    __asm {
        mov ax, es
        mov value, ax
    }
    return value;
}

void RestoreFlatEs()
{
    __asm {
        push ds
        pop es
    }
}

// One candidate: trap, have the handler resume with `requested` in SegEs,
// and read what the physical register actually holds afterwards.
std::uint16_t ObserveRestoredEs(const std::uint16_t requested)
{
    g_state.requested = requested;
    g_state.entries = 0;
    g_state.recovered = false;
    __asm {
        int 3
    }
    const std::uint16_t observed = ReadPhysicalEs();
    RestoreFlatEs();
    return observed;
}

}  // namespace

bool RunSegmentRestoreProbe()
{
    if (!repiu::platform::InstallFaultHandler(&OnFault, &g_state))
    {
        std::printf("segment_restore_handler_installed=false\n");
        return false;
    }

    // Safest first, so a candidate that kills the resume still leaves the
    // earlier lines on record. 0x0024/0x002C/0x0090 are the selectors
    // pumpitea actually loads through the HLE path; 0x0053 is the host's FS;
    // 0x0000 is the null selector a real CPU accepts in ES.
    const std::uint16_t candidates[] = {
        kFlatSelector, 0x0000U, 0x0053U, 0x0090U, 0x002CU, 0x0024U};

    bool mechanics_held = true;
    for (const std::uint16_t requested : candidates)
    {
        const std::uint16_t observed = ObserveRestoredEs(requested);
        std::printf(
            "segment_restore requested/observed/entries/recovered: "
            "0x%04X/0x%04X/%d/%s\n",
            static_cast<unsigned>(requested),
            static_cast<unsigned>(observed),
            g_state.entries,
            g_state.recovered ? "true" : "false");
        std::fflush(stdout);
        if (requested == kFlatSelector &&
            (observed != kFlatSelector || g_state.entries != 1))
        {
            mechanics_held = false;
        }
        if (g_state.entries > kMaxHandlerEntries)
        {
            mechanics_held = false;
        }
    }

    repiu::platform::RemoveFaultHandler();
    std::printf("segment_restore_probe=%s\n",
                mechanics_held ? "true" : "false");
    return mechanics_held;
}

}  // namespace repiu::tools
