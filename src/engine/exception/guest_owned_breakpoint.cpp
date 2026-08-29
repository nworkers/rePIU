#include "guest_owned_breakpoint.h"

#include "execution_internal.h"
#include "repiu/platform/host_error_stream.h"
#include "thread_context.h"

#include <cstdio>

namespace repiu::engine
{
namespace
{

// Enough to name the site and the branch that reached it, not so many that a
// guest which uses INT3 routinely floods the log.
constexpr std::uint32_t kReportLimit = 8;

}  // namespace

bool HandleGuestOwnedBreakpoint(const repiu::platform::FaultEvent& fault,
                                ThreadContext* context)
{
    if (context == nullptr || fault.registers == nullptr ||
        (fault.kind != repiu::platform::FaultKind::kBreakpoint &&
         fault.kind != repiu::platform::FaultKind::kSingleStep))
    {
        return false;
    }

    const std::uint32_t eip = fault.instruction_address;
    // Inside the guest's own image and outside the code cache. A cache address
    // belongs to the engine's own INT3s, which the handlers above own.
    if (!IsGuestInstructionPointer(context, eip) ||
        IsAotCacheAddress(context, eip))
    {
        return false;
    }
    // The tracked execution-trace sentinels are the engine's, planted in guest
    // memory, so they are the one guest-address breakpoint that is not the
    // guest's. HandleSingleStepTrace owns them and runs before this, but only
    // when the fault is a single step or a breakpoint with a reentry pending --
    // a sentinel that fires as a plain breakpoint reaches here instead, so this
    // check carries weight rather than merely repeating that one.
    if (context->execution_trace_configured &&
        (eip == context->runtime_base + context->execution_trace_start_offset ||
         (context->execution_trace_sentinel2_configured &&
          eip == context->runtime_base +
                     context->execution_trace_sentinel2_offset)))
    {
        return false;
    }
    // Read the byte before deciding. Eip names the INT3 itself on every host
    // (the 3c layer rewinds for that), so if this is not 0xCC the breakpoint
    // came from somewhere this function does not understand and must not touch.
    const auto* opcode = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(eip));
    if (*opcode != 0xCCU)
    {
        return false;
    }

    ++context->guest_owned_breakpoint_count;
    if (context->guest_owned_breakpoint_count <= kReportLimit)
    {
        // The registers are the point of the line. A guest that traps this way
        // has usually loaded one of them with which check failed -- pumpit1's
        // glide2x resolver puts a distinct value in EDX on each of its three
        // fatal branches -- so printing them is what makes the trap
        // diagnosable instead of merely survivable.
        char line[256] = {};
        const int length = std::snprintf(
            line, sizeof(line),
            "[repiu-guest-int3] #%u eip=0x%08X eax=0x%08X ebx=0x%08X "
            "ecx=0x%08X edx=0x%08X esi=0x%08X edi=0x%08X esp=0x%08X\n",
            static_cast<unsigned>(context->guest_owned_breakpoint_count),
            static_cast<unsigned>(eip),
            static_cast<unsigned>(fault.registers->Eax),
            static_cast<unsigned>(fault.registers->Ebx),
            static_cast<unsigned>(fault.registers->Ecx),
            static_cast<unsigned>(fault.registers->Edx),
            static_cast<unsigned>(fault.registers->Esi),
            static_cast<unsigned>(fault.registers->Edi),
            static_cast<unsigned>(fault.registers->Esp));
        if (length > 0)
        {
            repiu::platform::WriteHostErrorStream(
                line,
                static_cast<std::size_t>(length) < sizeof(line)
                    ? static_cast<std::size_t>(length)
                    : sizeof(line) - 1U);
        }
    }

    // One byte, which is the whole fix: hardware with no debugger attached
    // continues at the instruction after the INT3, and so does the guest.
    fault.registers->Eip = eip + 1U;
    return true;
}

}  // namespace repiu::engine
