#include "bios_keyboard_services.h"

#include "execution_internal.h"

#include <cstdint>
#include <sstream>

namespace repiu::platform::win32
{
namespace
{

constexpr std::uint32_t kEFlagsZero = 0x00000040U;

// Task 401: pumpit3 reads the PC keyboard through the extended INT 16h calls.
// The cabinet's play inputs arrive over the 0x02A0 port family, so the guest
// keyboard is genuinely idle here, and the whole observed routine at
// 0x030114FF is a query: AH=12h for shift flags, AH=11h to peek, and AH=10h
// only when the peek reported a key. Reporting "no key, no shift" is therefore
// the accurate state rather than a stub, and it keeps AH=10h/00h -- which block
// on real hardware -- unreachable in practice.
void ReportNoKeystroke(CONTEXT* win32_context)
{
    win32_context->Eax &= 0xFFFF0000U;
    win32_context->EFlags |= kEFlagsZero;
}

void ReportShiftFlags(CONTEXT* win32_context, std::uint16_t flags)
{
    win32_context->Eax = (win32_context->Eax & 0xFFFF0000U) | flags;
    win32_context->EFlags &= ~kEFlagsZero;
}

} // namespace

bool HandleBiosInterrupt16(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint16_t ax = static_cast<std::uint16_t>(
        win32_context->Eax & 0xFFFFU);
    const std::uint8_t ah = static_cast<std::uint8_t>(
        (win32_context->Eax >> 8) & 0xFFU);

    switch (ah)
    {
        case 0x00:
        case 0x10:
            // Wait for a keystroke. A real BIOS blocks; with no keyboard
            // activity to report, returning an empty keystroke is the only
            // non-hanging answer. The guest only reaches this after AH=01h or
            // AH=11h reports a key waiting, so it stays unreached.
            RecordHandledDosInterrupt(context, 0x16, ax);
            ReportNoKeystroke(win32_context);
            break;
        case 0x01:
        case 0x11:
            // Peek. ZF set means the buffer is empty.
            RecordHandledDosInterrupt(context, 0x16, ax);
            ReportNoKeystroke(win32_context);
            break;
        case 0x02:
            // Shift flags in AL.
            RecordHandledDosInterrupt(context, 0x16, ax);
            ReportShiftFlags(win32_context, 0);
            break;
        case 0x12:
            // Extended shift flags in AX.
            RecordHandledDosInterrupt(context, 0x16, ax);
            ReportShiftFlags(win32_context, 0);
            break;
        default:
        {
            std::ostringstream stream;
            stream << "unsupported BIOS INT 16h AH=0x"
                   << std::hex << static_cast<unsigned>(ah);
            context->hle_message = stream.str();
            return false;
        }
    }

    win32_context->Eip += 2;
    return true;
}

} // namespace repiu::platform::win32
