#include "low_memory_string_access.h"

#include "guest_memory_access.h"
#include "instruction_emulation.h"

#include <Zydis.h>

#include <cstring>
#include "repiu/platform/guest_cpu_context.h"

namespace repiu::platform::win32
{
namespace
{

constexpr std::uint32_t kEflagsZeroFlag = 0x00000040U;
constexpr std::uint32_t kEflagsDirectionFlag = 0x00000400U;

enum class StringOperation
{
    kScas,
    kLods,
    kCmps,
};

// The width comes from the mnemonic rather than the decoded operand width. A
// 66h prefix selects a different mnemonic outright (SCASW versus SCASD), so
// the mapping is exact and does not depend on how effective operand width is
// reported for byte forms.
bool ClassifyStringOperation(ZydisMnemonic mnemonic,
                             StringOperation* out_operation,
                             std::uint32_t* out_width)
{
    switch (mnemonic)
    {
        case ZYDIS_MNEMONIC_SCASB:
            *out_operation = StringOperation::kScas;
            *out_width = 1U;
            return true;
        case ZYDIS_MNEMONIC_SCASW:
            *out_operation = StringOperation::kScas;
            *out_width = 2U;
            return true;
        case ZYDIS_MNEMONIC_SCASD:
            *out_operation = StringOperation::kScas;
            *out_width = 4U;
            return true;
        case ZYDIS_MNEMONIC_LODSB:
            *out_operation = StringOperation::kLods;
            *out_width = 1U;
            return true;
        case ZYDIS_MNEMONIC_LODSW:
            *out_operation = StringOperation::kLods;
            *out_width = 2U;
            return true;
        case ZYDIS_MNEMONIC_LODSD:
            *out_operation = StringOperation::kLods;
            *out_width = 4U;
            return true;
        case ZYDIS_MNEMONIC_CMPSB:
            *out_operation = StringOperation::kCmps;
            *out_width = 1U;
            return true;
        case ZYDIS_MNEMONIC_CMPSW:
            *out_operation = StringOperation::kCmps;
            *out_width = 2U;
            return true;
        case ZYDIS_MNEMONIC_CMPSD:
            *out_operation = StringOperation::kCmps;
            *out_width = 4U;
            return true;
        default:
            return false;
    }
}

// The whole access must sit inside low memory. A straddling access has no
// defined servicing here, so it is declined rather than half-read.
bool IsWithinLowMemory(std::uint32_t address, std::uint32_t width)
{
    return address < repiu::runtime::kDosLowMemorySize &&
           width <= repiu::runtime::kDosLowMemorySize - address;
}

bool ReadLowMemoryValue(const repiu::runtime::DosLowMemory& memory,
                        std::uint32_t address,
                        std::uint32_t width,
                        std::uint32_t* out)
{
    if (width == 1U)
    {
        std::uint8_t value = 0;
        if (!repiu::runtime::ReadDosLowMemoryUInt8(memory, address, &value))
        {
            return false;
        }
        *out = value;
        return true;
    }
    if (width == 2U)
    {
        std::uint16_t value = 0;
        if (!repiu::runtime::ReadDosLowMemoryUInt16(memory, address, &value))
        {
            return false;
        }
        *out = value;
        return true;
    }
    if (width == 4U)
    {
        std::uint32_t value = 0;
        if (!repiu::runtime::ReadDosLowMemoryUInt32(memory, address, &value))
        {
            return false;
        }
        *out = value;
        return true;
    }
    return false;
}

// Reads one side of the operation, servicing it from the low-memory image when
// it lives there and from the arena otherwise. An address outside both is
// declined so exception handling never reads an arbitrary host address.
bool ReadStringOperand(ThreadContext* context,
                       std::uint32_t address,
                       std::uint32_t width,
                       std::uint32_t* out)
{
    if (address < repiu::runtime::kDosLowMemorySize)
    {
        if (!IsWithinLowMemory(address, width))
        {
            return false;
        }
        return ReadLowMemoryValue(context->dos_low_memory, address, width, out);
    }
    const void* source =
        reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address));
    if (!IsGuestRangeReadable(context, source, width))
    {
        return false;
    }
    std::uint32_t value = 0;
    std::memcpy(&value, source, width);
    *out = value;
    return true;
}

std::uint32_t ReadAccumulator(const repiu::platform::GuestCpuContext& win32_context, std::uint32_t width)
{
    if (width == 1U)
    {
        return win32_context.Eax & 0xFFU;
    }
    if (width == 2U)
    {
        return win32_context.Eax & 0xFFFFU;
    }
    return win32_context.Eax;
}

void WriteAccumulator(repiu::platform::GuestCpuContext* win32_context,
                      std::uint32_t width,
                      std::uint32_t value)
{
    if (width == 1U)
    {
        win32_context->Eax = (win32_context->Eax & 0xFFFFFF00U) | (value & 0xFFU);
        return;
    }
    if (width == 2U)
    {
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | (value & 0xFFFFU);
        return;
    }
    win32_context->Eax = value;
}

}  // namespace

bool ServiceGuestLowMemoryStringInstruction(repiu::platform::GuestCpuContext* win32_context,
                                            ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return false;
    }

    const std::uint32_t execute_eip =
        static_cast<std::uint32_t>(win32_context->Eip);
    const void* instruction_ptr =
        reinterpret_cast<const void*>(static_cast<std::uintptr_t>(execute_eip));

    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(
            &decoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32)))
    {
        return false;
    }
    ZydisDecodedInstruction instruction{};
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
            &decoder, instruction_ptr, 15, &instruction, operands)))
    {
        return false;
    }

    // The category gate keeps the SSE `cmpsd` encoding out: it shares the
    // mnemonic with the string form but is not a string operation.
    if (instruction.meta.category != ZYDIS_CATEGORY_STRINGOP)
    {
        return false;
    }

    StringOperation operation = StringOperation::kScas;
    std::uint32_t width = 0;
    if (!ClassifyStringOperation(instruction.mnemonic, &operation, &width))
    {
        return false;
    }

    // A 67h-prefixed form would index with DI/SI instead. None has been
    // observed, so it is declined rather than implemented on speculation.
    if (instruction.address_width != 32U)
    {
        return false;
    }

    const bool has_repe =
        (instruction.attributes & ZYDIS_ATTRIB_HAS_REPE) != 0;
    const bool has_repne =
        (instruction.attributes & ZYDIS_ATTRIB_HAS_REPNE) != 0;
    const bool has_rep =
        (instruction.attributes & ZYDIS_ATTRIB_HAS_REP) != 0 || has_repe ||
        has_repne;

    // A repeat whose count is already zero performs no access, so it cannot be
    // the source of this fault. Decline so the real cause stays visible.
    if (has_rep && win32_context->Ecx == 0)
    {
        return false;
    }

    const bool uses_edi = operation != StringOperation::kLods;
    const bool uses_esi = operation != StringOperation::kScas;
    // Only the comparing forms terminate a repeat on the zero flag; `rep lods`
    // repeats purely on the count.
    const bool zero_flag_terminates = operation != StringOperation::kLods;
    const std::int32_t delta =
        ((win32_context->EFlags & kEflagsDirectionFlag) != 0)
            ? -static_cast<std::int32_t>(width)
            : static_cast<std::int32_t>(width);

    std::uint32_t iterations = 0;
    std::uint32_t last_address = 0;
    bool terminated = false;

    while (iterations < kLowMemoryStringIterationCap)
    {
        const std::uint32_t edi = win32_context->Edi;
        const std::uint32_t esi = win32_context->Esi;

        // Once the operation has walked out of low memory there is nothing left
        // to service. Leaving EIP in place lets the CPU resume the very same
        // instruction from the current ECX/ESI/EDI, which is exactly how a REP
        // interrupted between iterations is defined to continue.
        const bool touches_low_memory =
            (uses_edi && edi < repiu::runtime::kDosLowMemorySize) ||
            (uses_esi && esi < repiu::runtime::kDosLowMemorySize);
        if (!touches_low_memory)
        {
            break;
        }

        // Read both sides before applying any effect so a declined operand
        // leaves the iteration entirely unperformed.
        std::uint32_t destination_value = 0;
        std::uint32_t source_value = 0;
        if (uses_edi && !ReadStringOperand(context, edi, width,
                                           &destination_value))
        {
            break;
        }
        if (uses_esi && !ReadStringOperand(context, esi, width, &source_value))
        {
            break;
        }

        switch (operation)
        {
            case StringOperation::kScas:
                SetCompareFlags(win32_context,
                                ReadAccumulator(*win32_context, width),
                                destination_value, width);
                last_address = edi;
                break;
            case StringOperation::kCmps:
                SetCompareFlags(win32_context, source_value, destination_value,
                                width);
                last_address = edi;
                break;
            case StringOperation::kLods:
                WriteAccumulator(win32_context, width, source_value);
                last_address = esi;
                break;
        }

        if (uses_edi)
        {
            win32_context->Edi =
                static_cast<std::uint32_t>(edi + static_cast<std::uint32_t>(delta));
        }
        if (uses_esi)
        {
            win32_context->Esi =
                static_cast<std::uint32_t>(esi + static_cast<std::uint32_t>(delta));
        }
        ++iterations;

        if (!has_rep)
        {
            terminated = true;
            break;
        }
        win32_context->Ecx -= 1U;
        if (win32_context->Ecx == 0)
        {
            terminated = true;
            break;
        }
        if (zero_flag_terminates)
        {
            const bool zero_flag =
                (win32_context->EFlags & kEflagsZeroFlag) != 0;
            if ((has_repe && !zero_flag) || (has_repne && zero_flag))
            {
                terminated = true;
                break;
            }
        }
    }

    if (iterations == 0)
    {
        return false;
    }

    // Advance only when the repetition actually finished. Otherwise EIP stays
    // on the instruction and the guest resumes it, either faulting again for
    // the next low-memory byte or continuing natively.
    if (terminated)
    {
        win32_context->Eip = execute_eip + instruction.length;
    }

    context->low_memory_string_service_count++;
    context->low_memory_string_iteration_count += iterations;
    context->last_low_memory_string_eip = execute_eip;
    context->last_low_memory_string_address = last_address;
    context->last_low_memory_string_mnemonic =
        static_cast<std::uint32_t>(instruction.mnemonic);
    context->last_low_memory_string_iterations = iterations;
    return true;
}

}  // namespace repiu::platform::win32
