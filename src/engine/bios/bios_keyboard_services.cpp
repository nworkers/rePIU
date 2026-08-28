#include "bios_keyboard_services.h"

#include "execution_internal.h"

#include <cstdint>
#include <sstream>
#include "repiu/platform/guest_cpu_context.h"

namespace repiu::engine
{
namespace
{

constexpr std::uint32_t kEFlagsZero = 0x00000040U;

void ReportNoKeystroke(repiu::platform::GuestCpuContext* win32_context)
{
    win32_context->Eax &= 0xFFFF0000U;
    win32_context->EFlags |= kEFlagsZero;
}

void ReportKeystroke(repiu::platform::GuestCpuContext* win32_context, std::uint16_t ax)
{
    win32_context->Eax = (win32_context->Eax & 0xFFFF0000U) | ax;
    win32_context->EFlags &= ~kEFlagsZero;
}

void ReportShiftFlags(repiu::platform::GuestCpuContext* win32_context, std::uint16_t flags)
{
    win32_context->Eax = (win32_context->Eax & 0xFFFF0000U) | flags;
    win32_context->EFlags &= ~kEFlagsZero;
}

} // namespace

bool HandleBiosInterrupt16(repiu::platform::GuestCpuContext* win32_context, ThreadContext* context)
{
    const std::uint16_t ax = static_cast<std::uint16_t>(
        win32_context->Eax & 0xFFFFU);
    const std::uint8_t ah = static_cast<std::uint8_t>(
        (win32_context->Eax >> 8) & 0xFFU);

    switch (ah)
    {
        case 0x00:
        case 0x10:
        {
            RecordHandledDosInterrupt(context, 0x16, ax);
            std::uint16_t keystroke = 0;
            if (context->bios_keyboard.Pop(ah == 0x10U, &keystroke))
            {
                ReportKeystroke(win32_context, keystroke);
            }
            else
            {
                // The observed guest checks before reading. Keep the existing
                // non-blocking fallback for a defensive empty read.
                ReportNoKeystroke(win32_context);
            }
            break;
        }
        case 0x01:
        case 0x11:
        {
            RecordHandledDosInterrupt(context, 0x16, ax);
            std::uint16_t keystroke = 0;
            if (context->bios_keyboard.Peek(ah == 0x11U, &keystroke))
            {
                ReportKeystroke(win32_context, keystroke);
            }
            else
            {
                ReportNoKeystroke(win32_context);
            }
            break;
        }
        case 0x02:
            // Shift flags in AL.
            RecordHandledDosInterrupt(context, 0x16, ax);
            ReportShiftFlags(
                win32_context, context->bios_keyboard.shift_flags() & 0x00FFU);
            break;
        case 0x12:
            // Extended shift flags in AX.
            RecordHandledDosInterrupt(context, 0x16, ax);
            ReportShiftFlags(win32_context, context->bios_keyboard.shift_flags());
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

} // namespace repiu::engine
