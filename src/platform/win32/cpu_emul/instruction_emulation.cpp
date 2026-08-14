#include "instruction_emulation.h"
#include "execution_internal.h"
#include "guest_memory_access.h"
#include "bios_keyboard_services.h"
#include "dos_int21_services.h"
#include "dpmi_mscdex_services.h"
#include "aot_runtime_dispatch.h"

#include <Zydis.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace repiu::platform::win32
{

std::uint16_t ReadRegister16(const CONTEXT& win32_context,
                             std::uint8_t register_id)
{
    switch (register_id & 0x07)
    {
        case 0:
            return static_cast<std::uint16_t>(win32_context.Eax & 0xFFFFU);
        case 1:
            return static_cast<std::uint16_t>(win32_context.Ecx & 0xFFFFU);
        case 2:
            return static_cast<std::uint16_t>(win32_context.Edx & 0xFFFFU);
        case 3:
            return static_cast<std::uint16_t>(win32_context.Ebx & 0xFFFFU);
        case 4:
            return static_cast<std::uint16_t>(win32_context.Esp & 0xFFFFU);
        case 5:
            return static_cast<std::uint16_t>(win32_context.Ebp & 0xFFFFU);
        case 6:
            return static_cast<std::uint16_t>(win32_context.Esi & 0xFFFFU);
        case 7:
            return static_cast<std::uint16_t>(win32_context.Edi & 0xFFFFU);
        default:
            return 0;
    }
}

void WriteRegister16(CONTEXT* win32_context,
                     std::uint8_t register_id,
                     std::uint16_t value)
{
    switch (register_id & 0x07)
    {
        case 0:
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | value;
            break;
        case 1:
            win32_context->Ecx =
                (win32_context->Ecx & 0xFFFF0000U) | value;
            break;
        case 2:
            win32_context->Edx =
                (win32_context->Edx & 0xFFFF0000U) | value;
            break;
        case 3:
            win32_context->Ebx =
                (win32_context->Ebx & 0xFFFF0000U) | value;
            break;
        case 4:
            win32_context->Esp =
                (win32_context->Esp & 0xFFFF0000U) | value;
            break;
        case 5:
            win32_context->Ebp =
                (win32_context->Ebp & 0xFFFF0000U) | value;
            break;
        case 6:
            win32_context->Esi =
                (win32_context->Esi & 0xFFFF0000U) | value;
            break;
        case 7:
            win32_context->Edi =
                (win32_context->Edi & 0xFFFF0000U) | value;
            break;
        default:
            break;
    }
}

std::uint8_t ReadRegister8(const CONTEXT& win32_context,
                           std::uint8_t register_index)
{
    const std::uint32_t registers[4] = {
        win32_context.Eax,
        win32_context.Ecx,
        win32_context.Edx,
        win32_context.Ebx,
    };
    const std::uint8_t base = register_index & 0x03U;
    const std::uint32_t shift = register_index < 4 ? 0U : 8U;
    return static_cast<std::uint8_t>((registers[base] >> shift) & 0xFFU);
}

void WriteRegister8(CONTEXT* win32_context,
                    std::uint8_t register_index,
                    std::uint8_t value)
{
    DWORD* registers[4] = {
        &win32_context->Eax,
        &win32_context->Ecx,
        &win32_context->Edx,
        &win32_context->Ebx,
    };
    const std::uint8_t base = register_index & 0x03U;
    const std::uint32_t shift = register_index < 4 ? 0U : 8U;
    const std::uint32_t mask = 0xFFU << shift;
    *registers[base] =
        (*registers[base] & ~mask) |
        (static_cast<std::uint32_t>(value) << shift);
}

void SetCompareFlags(CONTEXT* win32_context,
                     std::uint32_t lhs,
                     std::uint32_t rhs,
                     std::uint32_t width_bytes)
{
    constexpr std::uint32_t kArithmeticFlags =
        0x000008D5U;
    if (width_bytes != 1U && width_bytes != 2U && width_bytes != 4U)
    {
        return;
    }
    // Mask to the operand width so the sign bit, the zero test, and the
    // unsigned borrow are all evaluated at that width rather than at 32 bits.
    const std::uint32_t mask = (width_bytes == 4U)
                                   ? 0xFFFFFFFFU
                                   : ((1U << (width_bytes * 8U)) - 1U);
    const std::uint32_t sign = 1U << (width_bytes * 8U - 1U);
    lhs &= mask;
    rhs &= mask;
    const std::uint32_t result = (lhs - rhs) & mask;
    std::uint32_t flags = win32_context->EFlags & ~kArithmeticFlags;
    if (lhs < rhs)
    {
        flags |= 0x00000001U;
    }
    // Parity is defined over the low byte only, at every operand width.
    std::uint8_t parity = static_cast<std::uint8_t>(result);
    parity ^= static_cast<std::uint8_t>(parity >> 4U);
    parity ^= static_cast<std::uint8_t>(parity >> 2U);
    parity ^= static_cast<std::uint8_t>(parity >> 1U);
    if ((parity & 1U) == 0)
    {
        flags |= 0x00000004U;
    }
    if (((lhs ^ rhs ^ result) & 0x10U) != 0)
    {
        flags |= 0x00000010U;
    }
    if (result == 0)
    {
        flags |= 0x00000040U;
    }
    if ((result & sign) != 0)
    {
        flags |= 0x00000080U;
    }
    if (((lhs ^ rhs) & (lhs ^ result) & sign) != 0)
    {
        flags |= 0x00000800U;
    }
    win32_context->EFlags = flags;
}

void SetCompareFlags8(CONTEXT* win32_context,
                      std::uint8_t lhs,
                      std::uint8_t rhs)
{
    SetCompareFlags(win32_context, lhs, rhs, 1U);
}

void RecordGuestSegmentLoad(CONTEXT* win32_context,
                            ThreadContext* context,
                            std::uint8_t segment_register,
                            std::uint16_t selector,
                            std::uint32_t source)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return;
    }

    if (segment_register == 5 && selector == kDos4gwLinexeDataSelector)
    {
        ++context->linexe_data_gs_load_count;
    }

    ++context->handled_segment_load_count;
    context->last_segment_load_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    context->last_segment_load_opcode = 0x8E;
    context->last_segment_load_register = segment_register;
    context->last_segment_load_selector = selector;
    context->last_segment_load_source = source;
    if (segment_register < 6U) { ++context->handled_segment_load_register_counts[segment_register]; }
    Win32SegmentLoadObservation& observation = context->segment_load;
    const std::uint32_t sequence = observation.observed_count + 1;
    const std::uint32_t slot =
        (sequence - 1) % kWin32SegmentLoadTraceCapacity;
    Win32SegmentLoadTraceEntry& entry = observation.trace[slot];
    entry.valid = true;
    entry.sequence = sequence;
    entry.eip_offset =
        static_cast<std::uint32_t>(win32_context->Eip) >=
                context->runtime_base
            ? static_cast<std::uint32_t>(win32_context->Eip) -
                  context->runtime_base
            : static_cast<std::uint32_t>(win32_context->Eip);
    entry.segment_register = segment_register;
    entry.selector = selector;
    entry.source = source;
    observation.observed_count = sequence;
    if (observation.trace_stored_count < kWin32SegmentLoadTraceCapacity)
    {
        ++observation.trace_stored_count;
    }
    else
    {
        observation.trace_wrapped = true;
    }
    if (selector != 0 &&
        repiu::runtime::FindDescriptor(
            context->selector_table, selector) == nullptr)
    {
        repiu::runtime::RegisterDescriptor(
            &context->selector_table,
            repiu::runtime::GuestDescriptor{
                selector,
                0,
                repiu::runtime::kDosLowMemorySize - 1U,
                0,
                true,
            });
    }

    switch (segment_register)
    {
        case 0:
            context->guest_es = selector;
            win32_context->SegEs = selector;
            break;
        case 2:
            context->guest_ss = selector;
            break;
        case 3:
            context->guest_ds = selector;
            win32_context->SegDs = selector;
            break;
        case 4:
            context->guest_fs = selector;
            win32_context->SegFs = selector;
            break;
        case 5:
            context->guest_gs = selector;
            win32_context->SegGs = selector;
            break;
        default:
            break;
    }
    // Task 264 Phase 3a: the guest just (re)configured a segment register, so
    // re-fold selectors and bases into the natively-translated segment-override
    // sites (self-gated on an actual change; no-op without an AOT placement).
    ReResolveAotSegmentOverrides(context);
}


void RecordGuestSegmentStore(CONTEXT* win32_context,
                             ThreadContext* context,
                             std::uint8_t segment_register,
                             std::uint16_t selector,
                             std::uint32_t destination)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return;
    }

    ++context->handled_segment_store_count;
    context->last_segment_store_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    context->last_segment_store_opcode = 0x8C;
    context->last_segment_store_register = segment_register;
    context->last_segment_store_selector = selector;
    context->last_segment_store_destination = destination;
    if (segment_register < 6U) { ++context->handled_segment_store_register_counts[segment_register]; }
}

void RecordGuestSegmentMemoryLoad(CONTEXT* win32_context,
                                  ThreadContext* context,
                                  std::uint8_t opcode,
                                  std::uint8_t segment_register,
                                  std::uint16_t selector,
                                  std::uint32_t offset,
                                  std::uint32_t byte_width,
                                  std::uint32_t value)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return;
    }

    ++context->handled_segment_memory_load_count;
    context->last_segment_memory_load_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    context->last_segment_memory_load_opcode = opcode;
    context->last_segment_memory_load_register = segment_register;
    context->last_segment_memory_load_selector = selector;
    context->last_segment_memory_load_offset = offset;
    context->last_segment_memory_load_width = byte_width;
    context->last_segment_memory_load_value = value;
}

void RecordGuestMemoryStore(CONTEXT* win32_context,
                            ThreadContext* context,
                            std::uint32_t opcode,
                            std::uint32_t destination,
                            std::uint32_t value,
                            std::uint32_t byte_width,
                            const char* source_kind,
                            bool applied)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return;
    }

    ++context->handled_memory_store_count;
    context->diagnostic_progress_count.fetch_add(
        1,
        std::memory_order_relaxed);
    context->last_memory_store_address =
        static_cast<std::uint32_t>(win32_context->Eip);
    context->last_memory_store_opcode = opcode;
    context->last_memory_store_destination = destination;
    context->last_memory_store_value = value;
    context->last_memory_store_width = byte_width;
    context->last_memory_store_source_kind =
        source_kind != nullptr ? source_kind : "unknown";
    context->last_memory_store_applied = applied;
}

std::uint32_t ReadGeneralRegister32(const CONTEXT* win32_context,
                                    std::uint8_t register_index)
{
    switch (register_index & 0x07U)
    {
        case 0:
            return win32_context->Eax;
        case 1:
            return win32_context->Ecx;
        case 2:
            return win32_context->Edx;
        case 3:
            return win32_context->Ebx;
        case 4:
            return win32_context->Esp;
        case 5:
            return win32_context->Ebp;
        case 6:
            return win32_context->Esi;
        case 7:
            return win32_context->Edi;
        default:
            return 0;
    }
}

void WriteGeneralRegister32(CONTEXT* win32_context,
                            std::uint8_t register_index,
                            std::uint32_t value)
{
    switch (register_index & 0x07U)
    {
        case 0:
            win32_context->Eax = value;
            break;
        case 1:
            win32_context->Ecx = value;
            break;
        case 2:
            win32_context->Edx = value;
            break;
        case 3:
            win32_context->Ebx = value;
            break;
        case 4:
            win32_context->Esp = value;
            break;
        case 5:
            win32_context->Ebp = value;
            break;
        case 6:
            win32_context->Esi = value;
            break;
        case 7:
            win32_context->Edi = value;
            break;
        default:
            break;
    }
}

bool DecodeModRmMemoryAddress(
    const CONTEXT* win32_context,
    const std::uint8_t* instruction,
    std::uint32_t* destination,
    std::uint32_t* instruction_size)
{
    const std::uint8_t modrm = instruction[1];
    const std::uint8_t mod = (modrm >> 6) & 0x03U;
    const std::uint8_t rm = modrm & 0x07U;
    if (mod == 0x03)
    {
        return false;
    }

    std::uint32_t base = 0;
    std::uint32_t index = 0;
    std::uint32_t displacement = 0;
    std::uint32_t size = 2;
    std::uint32_t displacement_offset = 2;
    if (rm == 0x04)
    {
        const std::uint8_t sib = instruction[2];
        const std::uint8_t scale = (sib >> 6) & 0x03U;
        const std::uint8_t index_register = (sib >> 3) & 0x07U;
        const std::uint8_t base_register = sib & 0x07U;
        if (index_register != 0x04)
        {
            index = ReadGeneralRegister32(win32_context, index_register)
                    << scale;
        }
        if (!(mod == 0x00 && base_register == 0x05))
        {
            base = ReadGeneralRegister32(win32_context, base_register);
        }
        displacement_offset = 3;
        size = 3;
    }
    else if (!(mod == 0x00 && rm == 0x05))
    {
        base = ReadGeneralRegister32(win32_context, rm);
    }

    const bool absolute_displacement =
        mod == 0x00 &&
        (rm == 0x05 ||
         (rm == 0x04 && (instruction[2] & 0x07U) == 0x05));
    if (absolute_displacement || mod == 0x02)
    {
        displacement =
            static_cast<std::uint32_t>(instruction[displacement_offset]) |
            (static_cast<std::uint32_t>(instruction[displacement_offset + 1]) << 8) |
            (static_cast<std::uint32_t>(instruction[displacement_offset + 2]) << 16) |
            (static_cast<std::uint32_t>(instruction[displacement_offset + 3]) << 24);
        size = displacement_offset + 4;
    }
    else if (mod == 0x01)
    {
        displacement = static_cast<std::uint32_t>(
            static_cast<std::int32_t>(
                static_cast<std::int8_t>(instruction[displacement_offset])));
        size = displacement_offset + 1;
    }

    *destination = base + index + displacement;
    *instruction_size = size;
    return true;
}

bool HandleSegmentLoadInstruction(CONTEXT* win32_context,
                                  ThreadContext* context);
bool HandleSegmentPopInstruction(CONTEXT* win32_context,
                                 ThreadContext* context);

bool HandleSegmentStoreInstruction(CONTEXT* win32_context,
                                   ThreadContext* context);

bool HandleSegmentOverrideByteLoadInstruction(CONTEXT* win32_context,
                                              ThreadContext* context);

bool HandleSegmentMemoryLoadInstruction(CONTEXT* win32_context,
                                        ThreadContext* context);

bool HandleSegmentMemoryCompareInstruction(CONTEXT* win32_context,
                                           ThreadContext* context);

bool HandleTracedMemoryStoreInstruction(CONTEXT* win32_context,
                                        ThreadContext* context);

bool HandleTracedMemoryTestInstruction(CONTEXT* win32_context,
                                       ThreadContext* context);

bool HandleTracedFpuMemoryInstruction(CONTEXT* win32_context,
                                      ThreadContext* context);

bool HandleTracedMemoryLoadInstruction(CONTEXT* win32_context,
                                       ThreadContext* context);

bool HandleSegmentLoadInstruction(CONTEXT* win32_context,
                                  ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    std::uint32_t prefix_length = 0;
    while (instruction[prefix_length] == 0x26 ||
           instruction[prefix_length] == 0x2E ||
           instruction[prefix_length] == 0x36 ||
           instruction[prefix_length] == 0x3E ||
           instruction[prefix_length] == 0x64 ||
           instruction[prefix_length] == 0x65 ||
           instruction[prefix_length] == 0x66 ||
           instruction[prefix_length] == 0x67)
    {
        ++prefix_length;
    }

    if (instruction[prefix_length] != 0x8E)
    {
        return false;
    }

    const std::uint8_t modrm = instruction[prefix_length + 1];
    const std::uint8_t mod = static_cast<std::uint8_t>((modrm >> 6) & 0x03);
    const std::uint8_t segment_register =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);
    const std::uint8_t source_register =
        static_cast<std::uint8_t>(modrm & 0x07);

    if (segment_register == 1 || segment_register > 5)
    {
        return false;
    }

    std::uint16_t selector = 0;
    std::uint32_t source = 0;
    std::uint32_t instruction_length = prefix_length + 2;
    if (mod == 0x03)
    {
        selector = ReadRegister16(*win32_context, source_register);
    }
    else
    {
        std::uint32_t unprefixed_length = 0;
        if (!DecodeModRmMemoryAddress(win32_context,
                                      instruction + prefix_length,
                                      &source,
                                      &unprefixed_length))
        {
            return false;
        }
        const void* source_pointer = reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(source));
        if (!IsGuestRangeReadable(context, source_pointer, 2))
        {
            return false;
        }

        std::memcpy(&selector, source_pointer, sizeof(selector));
        instruction_length = prefix_length + unprefixed_length;
    }

    RecordGuestSegmentLoad(win32_context,
                           context,
                           segment_register,
                           selector,
                           source);
    win32_context->Eip += instruction_length;
    HandleSegmentLoadInstruction(win32_context, context);
    HandleSegmentStoreInstruction(win32_context, context);
    return true;
}

bool HandleSegmentPopInstruction(CONTEXT* win32_context,
                                 ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    std::uint8_t segment_register = 0;
    std::uint32_t instruction_size = 1;
    if (instruction[0] == 0x1F)
    {
        segment_register = 3;  // DS
    }
    else if (instruction[0] == 0x07)
    {
        segment_register = 0;  // ES
    }
    else if (instruction[0] == 0x0F && instruction[1] == 0xA1)
    {
        segment_register = 4;  // FS
        instruction_size = 2;
    }
    else if (instruction[0] == 0x0F && instruction[1] == 0xA9)
    {
        segment_register = 5;  // GS
        instruction_size = 2;
    }
    else
    {
        return false;
    }

    const std::uint32_t source =
        static_cast<std::uint32_t>(win32_context->Esp);
    const void* source_pointer = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(source));
    if (!IsGuestRangeReadable(context, source_pointer, 2))
    {
        return false;
    }

    std::uint16_t selector = 0;
    std::memcpy(&selector, source_pointer, sizeof(selector));
    RecordGuestSegmentLoad(win32_context,
                           context,
                           segment_register,
                           selector,
                           source);

    win32_context->Esp += 4;
    win32_context->Eip += instruction_size;
    return true;
}

bool HandleRepStosdInstruction(CONTEXT* win32_context,
                               ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xF3 || instruction[1] != 0xAB ||
        win32_context->Eax != 0 ||
        (win32_context->EFlags & 0x00000400U) != 0)
    {
        return false;
    }

    const std::uint64_t byte_count =
        static_cast<std::uint64_t>(win32_context->Ecx) * 4U;
    if (byte_count > 0xFFFFFFFFULL)
    {
        return false;
    }
    void* destination = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(win32_context->Edi));
    if (byte_count != 0 &&
        !IsGuestRangeWritable(context,
                              destination,
                              static_cast<std::uint32_t>(byte_count)))
    {
        return false;
    }

    if (byte_count != 0)
    {
        std::memset(destination, 0, static_cast<std::size_t>(byte_count));
    }
    win32_context->Edi += static_cast<std::uint32_t>(byte_count);
    win32_context->Ecx = 0;
    win32_context->Eip += 2;
    context->diagnostic_progress_count.fetch_add(
        1,
        std::memory_order_relaxed);
    return true;
}

bool HandleLodsbInstruction(CONTEXT* win32_context,
                            ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xAC)
    {
        return false;
    }
    std::uint8_t value = 0;
    if (!ReadSegmentByte(context, 3, context->guest_ds,
                         win32_context->Esi, &value))
    {
        return false;
    }
    win32_context->Eax =
        (win32_context->Eax & 0xFFFFFF00U) | value;
    win32_context->Esi +=
        (win32_context->EFlags & 0x00000400U) != 0 ? -1 : 1;
    ++win32_context->Eip;
    return true;
}

bool HandleSegmentStoreInstruction(CONTEXT* win32_context,
                                   ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] == 0x66 && instruction[1] == 0x8C)
    {
        const std::uint8_t modrm = instruction[2];
        const std::uint8_t mod = (modrm >> 6) & 0x03U;
        const std::uint8_t segment_register = (modrm >> 3) & 0x07U;
        const std::uint8_t destination_register = modrm & 0x07U;
        if (mod != 0x03 || segment_register == 1 ||
            segment_register > 5)
        {
            return false;
        }
        const std::uint16_t selector =
            ReadGuestSegmentSelector(*context, segment_register, win32_context);
        WriteRegister16(win32_context, destination_register, selector);
        RecordGuestSegmentStore(win32_context,
                                context,
                                segment_register,
                                selector,
                                destination_register);
        win32_context->Eip += 3;
        return true;
    }
    if (instruction[0] == 0x8C)
    {
        const std::uint8_t modrm = instruction[1];
        const std::uint8_t mod =
            static_cast<std::uint8_t>((modrm >> 6) & 0x03);
        const std::uint8_t segment_register =
            static_cast<std::uint8_t>((modrm >> 3) & 0x07);
        if (segment_register == 1 || segment_register > 5)
        {
            return false;
        }

        const std::uint16_t selector =
            ReadGuestSegmentSelector(*context, segment_register, win32_context);
        if (mod == 0x03)
        {
            const std::uint8_t destination_register =
                static_cast<std::uint8_t>(modrm & 0x07);
            WriteRegister16(win32_context,
                            destination_register,
                            selector);
            RecordGuestSegmentStore(win32_context,
                                    context,
                                    segment_register,
                                    selector,
                                    destination_register);
            win32_context->Eip += 2;
            return true;
        }

        std::uint32_t destination = 0;
        std::uint32_t instruction_size = 0;
        if (!DecodeModRmMemoryAddress(win32_context,
                                      instruction,
                                      &destination,
                                      &instruction_size))
        {
            return false;
        }
        void* destination_pointer = reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(destination));
        if (!IsGuestRangeWritable(context, destination_pointer, 2) ||
            !WriteGuestUInt16(context, destination_pointer, selector))
        {
            return false;
        }
        RecordGuestSegmentStore(win32_context,
                                context,
                                segment_register,
                                selector,
                                destination);
        win32_context->Eip += instruction_size;
        return true;
    }
    if (instruction[0] != 0x66 || instruction[1] != 0x26 ||
        instruction[2] != 0x8C)
    {
        return false;
    }

    const std::uint8_t modrm = instruction[3];
    const std::uint8_t mod = static_cast<std::uint8_t>((modrm >> 6) & 0x03);
    const std::uint8_t segment_register =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);
    const std::uint8_t rm = static_cast<std::uint8_t>(modrm & 0x07);
    if (mod != 0x00 || rm != 0x05)
    {
        return false;
    }

    if (segment_register == 1 || segment_register > 5)
    {
        return false;
    }

    const std::uint32_t destination =
        static_cast<std::uint32_t>(instruction[4]) |
        (static_cast<std::uint32_t>(instruction[5]) << 8) |
        (static_cast<std::uint32_t>(instruction[6]) << 16) |
        (static_cast<std::uint32_t>(instruction[7]) << 24);
    void* destination_pointer = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(destination));
    if (!IsGuestRangeWritable(context, destination_pointer, 2))
    {
        return false;
    }

    const std::uint16_t selector =
        ReadGuestSegmentSelector(*context, segment_register, win32_context);
    if (!WriteGuestUInt16(context, destination_pointer, selector))
    {
        return false;
    }

    RecordGuestSegmentStore(win32_context,
                            context,
                            segment_register,
                            selector,
                            destination);
    win32_context->Eip += 8;
    return true;
}

bool ReadSegmentOverrideByte(ThreadContext* context,
                             std::uint8_t segment_register,
                             std::uint16_t selector,
                             std::uint32_t offset,
                             std::uint8_t* value)
{
    if (context == nullptr || value == nullptr)
    {
        return false;
    }

    if (segment_register == 0 && selector == context->guest_es &&
        selector != 0 && offset == 0x80)
    {
        *value = 0;
        return true;
    }

    return false;
}

bool HandleSegmentOverrideByteLoadInstruction(CONTEXT* win32_context,
                                              ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if ((instruction[0] == 0x64 || instruction[0] == 0x65) &&
        instruction[1] == 0x8A)
    {
        std::uint32_t offset = 0;
        std::uint32_t unprefixed_size = 0;
        if (!DecodeModRmMemoryAddress(win32_context,
                                      instruction + 1,
                                      &offset,
                                      &unprefixed_size))
        {
            return false;
        }
        const std::uint8_t segment_register =
            instruction[0] == 0x64 ? 4 : 5;
        const std::uint16_t selector =
            ReadGuestSegmentSelector(*context, segment_register, win32_context);
        std::uint8_t value = 0;
        if (!ReadSegmentByte(context,
                             segment_register,
                             selector,
                             offset,
                             &value))
        {
            return false;
        }
        if (segment_register == 5)
        {
            if (context->linexe_gs_byte_load_count == 0)
            {
                context->linexe_first_gs_byte_offset = offset;
                context->linexe_first_gs_byte_value = value;
            }
            ++context->linexe_gs_byte_load_count;
        }
        const std::uint8_t destination_register =
            (instruction[2] >> 3) & 0x07U;
        WriteRegister8(win32_context, destination_register, value);
        RecordGuestSegmentMemoryLoad(win32_context,
                                     context,
                                     instruction[1],
                                     segment_register,
                                     selector,
                                     offset,
                                     1,
                                     value);
        win32_context->Eip += 1 + unprefixed_size;
        return true;
    }
    if (instruction[0] == 0x26 &&
        (instruction[1] == 0x8A || instruction[1] == 0x3A))
    {
        const std::uint8_t modrm = instruction[2];
        const std::uint8_t mod =
            static_cast<std::uint8_t>((modrm >> 6) & 0x03);
        const std::uint8_t register_index =
            static_cast<std::uint8_t>((modrm >> 3) & 0x07);
        const std::uint8_t base_register =
            static_cast<std::uint8_t>(modrm & 0x07);
        if (mod == 0 && base_register == 0)
        {
            const std::uint8_t segment_register = 0;
            const std::uint16_t selector =
                ReadGuestSegmentSelector(*context, segment_register, win32_context);
            const std::uint32_t offset = win32_context->Eax;
            std::uint8_t value = 0;
            if (!ReadSegmentByte(
                    context,
                    segment_register,
                    selector,
                    offset,
                    &value))
            {
                return false;
            }

            if (instruction[1] == 0x8A)
            {
                WriteRegister8(win32_context, register_index, value);
            }
            else
            {
                SetCompareFlags8(
                    win32_context,
                    ReadRegister8(*win32_context, register_index),
                    value);
            }
            RecordGuestSegmentMemoryLoad(win32_context,
                                         context,
                                         instruction[1],
                                         segment_register,
                                         selector,
                                         offset,
                                         1,
                                         value);
            win32_context->Eip += 3;
            return true;
        }
    }
    if (instruction[0] == 0x26 && instruction[1] == 0x80 &&
        instruction[2] == 0x38)
    {
        const std::uint8_t segment_register = 0;
        const std::uint16_t selector =
            ReadGuestSegmentSelector(*context, segment_register, win32_context);
        const std::uint32_t offset = win32_context->Eax;
        std::uint8_t value = 0;
        if (!ReadSegmentByte(context,
                             segment_register,
                             selector,
                             offset,
                             &value))
        {
            return false;
        }
        SetCompareFlags8(win32_context, value, instruction[3]);
        RecordGuestSegmentMemoryLoad(win32_context,
                                     context,
                                     instruction[1],
                                     segment_register,
                                     selector,
                                     offset,
                                     1,
                                     value);
        win32_context->Eip += 4;
        return true;
    }
    if (instruction[0] != 0x26 || instruction[1] != 0x8A ||
        instruction[2] != 0x4F)
    {
        return false;
    }

    const std::uint8_t modrm = instruction[2];
    const std::uint8_t mod = static_cast<std::uint8_t>((modrm >> 6) & 0x03);
    const std::uint8_t destination_register =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);
    const std::uint8_t base_register =
        static_cast<std::uint8_t>(modrm & 0x07);
    if (mod != 0x01 || destination_register != 0x01 ||
        base_register != 0x07)
    {
        return false;
    }

    const std::int8_t displacement =
        static_cast<std::int8_t>(instruction[3]);
    const std::uint32_t offset =
        static_cast<std::uint32_t>(win32_context->Edi + displacement);
    const std::uint8_t segment_register = 0;
    const std::uint16_t selector =
        ReadGuestSegmentSelector(*context, segment_register, win32_context);

    std::uint8_t value = 0;
    if (!ReadSegmentOverrideByte(
            context, segment_register, selector, offset, &value))
    {
        return false;
    }

    win32_context->Ecx =
        (win32_context->Ecx & 0xFFFFFF00U) | value;
    RecordGuestSegmentMemoryLoad(win32_context,
                                 context,
                                 0x8A,
                                 segment_register,
                                 selector,
                                 offset,
                                 1,
                                 value);
    win32_context->Eip += 4;
    return true;
}

void RecordDosEnvironmentAccess(ThreadContext* context, std::uint32_t offset)
{
    if (context == nullptr ||
        offset >= context->dos_environment_block.size())
    {
        return;
    }

    const auto& block = context->dos_environment_block;
    std::size_t cursor = 0;
    while (cursor < block.size() && block[cursor] != 0)
    {
        const std::size_t entry_begin = cursor;
        while (cursor < block.size() && block[cursor] != 0)
        {
            ++cursor;
        }
        const std::size_t entry_end = cursor;

        if (offset >= entry_begin && offset <= entry_end)
        {
            std::size_t equals = entry_begin;
            while (equals < entry_end && block[equals] != '=')
            {
                ++equals;
            }

            const std::size_t name_end =
                equals < entry_end ? equals : entry_end;
            const std::size_t value_begin =
                equals < entry_end ? equals + 1 : entry_end;
            context->last_dos_environment_access_valid = true;
            context->last_dos_environment_access_offset = offset;
            context->last_dos_environment_entry_offset =
                static_cast<std::uint32_t>(entry_begin);
            context->last_dos_environment_value_length =
                static_cast<std::uint32_t>(entry_end - value_begin);
            if (name_end > entry_begin)
            {
                context->last_dos_environment_entry_name.assign(
                    reinterpret_cast<const char*>(&block[entry_begin]),
                    name_end - entry_begin);
            }
            else
            {
                context->last_dos_environment_entry_name = "<unnamed>";
            }
            context->diagnostic_progress_count.fetch_add(
                1,
                std::memory_order_relaxed);
            return;
        }

        if (cursor < block.size())
        {
            ++cursor;
        }
    }
}

bool ReadSegmentDword(ThreadContext* context,
                      std::uint8_t segment_register,
                      std::uint16_t selector,
                      std::uint32_t offset,
                      std::uint32_t* value)
{
    if (context == nullptr || value == nullptr)
    {
        return false;
    }
    if (selector == 0U &&
        offset <= repiu::runtime::kDosLowMemorySize - sizeof(*value))
    {
        return repiu::runtime::ReadDosLowMemoryUInt32(
            context->dos_low_memory, offset, value);
    }

    if (segment_register == 3 && selector == context->guest_ds &&
        selector != 0 && offset < 0x10000)
    {
        RecordDosEnvironmentAccess(context, offset);
        std::uint32_t result = 0;
        for (std::uint32_t index = 0; index < 4; ++index)
        {
            std::uint8_t byte = 0;
            const std::uint32_t byte_offset = offset + index;
            if (byte_offset >= offset &&
                byte_offset < context->dos_environment_block.size())
            {
                byte = context->dos_environment_block[byte_offset];
            }
            result |= static_cast<std::uint32_t>(byte) << (index * 8);
        }
        *value = result;
        return true;
    }

    std::uint32_t linear_address = 0;
    if (!repiu::runtime::TranslateSelectorOffset(
            context->selector_table,
            selector,
            offset,
            sizeof(*value),
            &linear_address))
    {
        return false;
    }
    if (linear_address < repiu::runtime::kDosLowMemorySize)
    {
        return repiu::runtime::ReadDosLowMemoryUInt32(
            context->dos_low_memory,
            linear_address,
            value);
    }
    const void* source = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(linear_address));
    if (!IsGuestRangeReadable(context, source, sizeof(*value)))
    {
        return false;
    }
    std::memcpy(value, source, sizeof(*value));
    return true;
}

bool ReadSegmentByte(ThreadContext* context,
                     std::uint8_t segment_register,
                     std::uint16_t selector,
                     std::uint32_t offset,
                     std::uint8_t* value)
{
    if (context == nullptr || value == nullptr)
    {
        return false;
    }

    if (selector == 0U && offset < repiu::runtime::kDosLowMemorySize)
    {
        return repiu::runtime::ReadDosLowMemoryUInt8(
            context->dos_low_memory, offset, value);
    }

    if (segment_register == 3 && selector == context->guest_ds &&
        selector != 0 && offset < 0x10000)
    {
        RecordDosEnvironmentAccess(context, offset);
        if (offset < context->dos_environment_block.size())
        {
            *value = context->dos_environment_block[offset];
        }
        else
        {
            *value = 0;
        }
        return true;
    }

    std::uint32_t linear_address = 0;
    if (!repiu::runtime::TranslateSelectorOffset(
            context->selector_table,
            selector,
            offset,
            sizeof(*value),
            &linear_address))
    {
        return false;
    }
    if (linear_address < repiu::runtime::kDosLowMemorySize)
    {
        return repiu::runtime::ReadDosLowMemoryUInt8(
            context->dos_low_memory,
            linear_address,
            value);
    }
    const void* source = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(linear_address));
    if (!IsGuestRangeReadable(context, source, sizeof(*value)))
    {
        return false;
    }
    std::memcpy(value, source, sizeof(*value));
    return true;
}

bool ReadSegmentWord(ThreadContext* context,
                     std::uint16_t selector,
                     std::uint32_t offset,
                     std::uint16_t* value)
{
    if (context == nullptr || value == nullptr)
    {
        return false;
    }

    if (selector == 0U &&
        offset <= repiu::runtime::kDosLowMemorySize - sizeof(*value))
    {
        return repiu::runtime::ReadDosLowMemoryUInt16(
            context->dos_low_memory, offset, value);
    }

    std::uint32_t linear_address = 0;
    if (repiu::runtime::TranslateSelectorOffset(
            context->selector_table,
            selector,
            offset,
            sizeof(*value),
            &linear_address))
    {
        if (linear_address < repiu::runtime::kDosLowMemorySize)
        {
            return repiu::runtime::ReadDosLowMemoryUInt16(
                context->dos_low_memory,
                linear_address,
                value);
        }
        const void* source = reinterpret_cast<const void*>(
            static_cast<std::uintptr_t>(linear_address));
        if (!IsGuestRangeReadable(context, source, sizeof(*value)))
        {
            return false;
        }
        std::memcpy(value, source, sizeof(*value));
        return true;
    }

    return false;
}

bool HandleSegmentOverrideMemoryLoadInstruction(CONTEXT* win32_context,
                                                ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    std::uint32_t prefix_length = 0;
    std::uint8_t segment_register = 0xFFU;
    bool operand_word = false;
    while (true)
    {
        const std::uint8_t prefix = instruction[prefix_length];
        if (prefix == 0x66)
        {
            operand_word = true;
        }
        else if (prefix == 0x26)
        {
            segment_register = 0;
        }
        else if (prefix == 0x36)
        {
            segment_register = 2;
        }
        else if (prefix == 0x3E)
        {
            segment_register = 3;
        }
        else if (prefix == 0x64)
        {
            segment_register = 4;
        }
        else if (prefix == 0x65)
        {
            segment_register = 5;
        }
        else if (prefix == 0x2E || prefix == 0x67)
        {
            return false;
        }
        else
        {
            break;
        }
        ++prefix_length;
    }

    const std::uint8_t opcode = instruction[prefix_length];
    if (segment_register == 0xFFU ||
        (opcode != 0x8A && opcode != 0x8B && opcode != 0x3A))
    {
        return false;
    }
    ++context->linexe_shared_load_entry_count;
    const std::uint8_t modrm = instruction[prefix_length + 1];
    if (((modrm >> 6) & 0x03U) == 0x03U)
    {
        return false;
    }
    std::uint32_t offset = 0;
    std::uint32_t unprefixed_size = 0;
    if (!DecodeModRmMemoryAddress(win32_context,
                                  instruction + prefix_length,
                                  &offset,
                                  &unprefixed_size))
    {
        return false;
    }
    const std::uint8_t destination_register = (modrm >> 3) & 0x07U;
    const std::uint16_t selector =
        ReadGuestSegmentSelector(*context, segment_register, win32_context);
    context->linexe_shared_load_selector = selector;
    context->linexe_shared_load_offset = offset;
    std::uint32_t value = 0;
    std::uint32_t width = 0;
    if (opcode == 0x8A || opcode == 0x3A)
    {
        std::uint8_t byte = 0;
        if (!ReadSegmentByte(context, segment_register, selector,
                             offset, &byte))
        {
            return false;
        }
        value = byte;
        width = 1;
        if (opcode == 0x3A)
        {
            const std::uint8_t left =
                ReadGeneralRegister8(win32_context, destination_register);
            UpdateSubtract8Flags(
                win32_context,
                left,
                byte,
                static_cast<std::uint8_t>(left - byte));
        }
        else
        {
            WriteRegister8(win32_context, destination_register, byte);
        }
    }
    else if (operand_word)
    {
        std::uint16_t word = 0;
        if (!ReadSegmentWord(context, selector, offset, &word))
        {
            return false;
        }
        value = word;
        width = 2;
        WriteRegister16(win32_context, destination_register, word);
    }
    else
    {
        if (!ReadSegmentDword(context, segment_register, selector,
                              offset, &value))
        {
            return false;
        }
        width = 4;
        WriteGeneralRegister32(win32_context, destination_register, value);
    }
    RecordGuestSegmentMemoryLoad(win32_context,
                                 context,
                                 opcode,
                                 segment_register,
                                 selector,
                                 offset,
                                 width,
                                 value);
    ++context->linexe_shared_load_read_count;
    context->linexe_shared_load_value = value;
    const std::uint32_t instruction_offset =
        static_cast<std::uint32_t>(win32_context->Eip) -
        context->runtime_base;
    if (instruction_offset == 0x000F380FU)
    {
        context->linexe_root_offset_load_value = value;
        ++context->linexe_root_offset_load_success;
    }
    else if (instruction_offset == 0x000F3813U)
    {
        context->linexe_root_selector_load_value = value;
        ++context->linexe_root_selector_load_success;
    }
    else if (instruction_offset == 0x000F38C7U)
    {
        context->linexe_export_entry_name_offset_value = value;
    }
    else if (instruction_offset == 0x000F38CBU)
    {
        context->linexe_export_entry_name_selector_value = value;
    }
    else if (instruction_offset == 0x000F393AU)
    {
        context->linexe_export_value_load_selector = selector;
        context->linexe_export_value_load_offset = offset;
        context->linexe_export_value_load_value = value;
    }
    if (segment_register == 5 && width == 1)
    {
        if (context->linexe_gs_byte_load_count == 0)
        {
            context->linexe_first_gs_byte_offset = offset;
            context->linexe_first_gs_byte_value = value;
        }
        ++context->linexe_gs_byte_load_count;
    }
    win32_context->Eip += prefix_length + unprefixed_size;
    HandleSegmentOverrideMemoryLoadInstruction(win32_context, context);
    return true;
}

bool HandleFsSegmentWordLoadInstruction(CONTEXT* win32_context,
                                        ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x66 ||
        (instruction[1] != 0x64 && instruction[1] != 0x65) ||
        instruction[2] != 0x8B)
    {
        return false;
    }

    const std::uint8_t modrm = instruction[3];
    const std::uint8_t mod = static_cast<std::uint8_t>((modrm >> 6) & 0x03);
    const std::uint8_t destination_register =
        static_cast<std::uint8_t>((modrm >> 3) & 0x07);
    const std::uint8_t base_register =
        static_cast<std::uint8_t>(modrm & 0x07);
    if (mod == 0x03 || base_register == 0x04)
    {
        return false;
    }

    std::uint32_t instruction_length = 4;
    std::uint32_t offset = 0;
    if (mod == 0x00)
    {
        if (base_register == 0x05)
        {
            return false;
        }
        offset = ReadGeneralRegister32(win32_context, base_register);
    }
    else if (mod == 0x01)
    {
        const std::int8_t displacement =
            static_cast<std::int8_t>(instruction[4]);
        offset = ReadGeneralRegister32(win32_context, base_register) +
            static_cast<std::uint32_t>(
                static_cast<std::int32_t>(displacement));
        instruction_length = 5;
    }
    else
    {
        return false;
    }

    const std::uint8_t segment_register =
        instruction[1] == 0x64 ? 4 : 5;
    const std::uint16_t selector =
        ReadGuestSegmentSelector(*context, segment_register, win32_context);
    std::uint16_t value = 0;
    if (!ReadSegmentWord(context, selector, offset, &value))
    {
        return false;
    }

    WriteRegister16(win32_context, destination_register, value);
    RecordGuestSegmentMemoryLoad(win32_context,
                                 context,
                                 0x8B,
                                 segment_register,
                                 selector,
                                 offset,
                                 2,
                                 value);
    win32_context->Eip += instruction_length;
    return true;
}

bool HandleSegmentMemoryLoadInstruction(CONTEXT* win32_context,
                                        ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    const std::uint8_t segment_register = 3;
    const std::uint16_t selector =
        ReadGuestSegmentSelector(*context, segment_register, win32_context);

    if (instruction[0] == 0x8B && instruction[1] == 0x06)
    {
        const std::uint32_t offset = win32_context->Esi;

        std::uint32_t value = 0;
        if (!ReadSegmentDword(
                context, segment_register, selector, offset, &value))
        {
            return false;
        }

        win32_context->Eax = value;
        RecordGuestSegmentMemoryLoad(win32_context,
                                     context,
                                     0x8B,
                                     segment_register,
                                     selector,
                                     offset,
                                     4,
                                     value);
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[0],
                              offset,
                              value);
        win32_context->Eip += 2;
        return true;
    }

    if (instruction[0] == 0xAC)
    {
        const std::uint32_t offset = win32_context->Esi;

        std::uint8_t value = 0;
        if (!ReadSegmentByte(
                context, segment_register, selector, offset, &value))
        {
            return false;
        }

        win32_context->Eax =
            (win32_context->Eax & 0xFFFFFF00U) | value;
        if ((win32_context->EFlags & 0x400U) != 0)
        {
            --win32_context->Esi;
        }
        else
        {
            ++win32_context->Esi;
        }
        RecordGuestSegmentMemoryLoad(win32_context,
                                     context,
                                     0xAC,
                                     segment_register,
                                     selector,
                                     offset,
                                     1,
                                     value);
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[0],
                              offset,
                              value);
        ++win32_context->Eip;
        return true;
    }

    if (instruction[0] == 0xA4)
    {
        const std::uint32_t offset = win32_context->Esi;

        std::uint8_t value = 0;
        if (!ReadSegmentByte(
                context, segment_register, selector, offset, &value))
        {
            return false;
        }

        const std::uint32_t destination_address = win32_context->Edi;
        void* destination = reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(destination_address));
        if (!WriteGuestUInt8(context, destination, value))
        {
            return false;
        }

        if ((win32_context->EFlags & 0x400U) != 0)
        {
            --win32_context->Esi;
            --win32_context->Edi;
        }
        else
        {
            ++win32_context->Esi;
            ++win32_context->Edi;
        }
        RecordGuestSegmentMemoryLoad(win32_context,
                                     context,
                                     0xA4,
                                     segment_register,
                                     selector,
                                     offset,
                                     1,
                                     value);
        RecordLowMemoryAccess(win32_context,
                              context,
                              instruction[0],
                              destination_address,
                              value);
        ++win32_context->Eip;
        return true;
    }

    return false;
}

bool HandleSegmentMemoryCompareInstruction(CONTEXT* win32_context,
                                           ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x80 || instruction[1] != 0x3E)
    {
        return false;
    }

    const std::uint8_t segment_register = 3;
    const std::uint16_t selector =
        ReadGuestSegmentSelector(*context, segment_register, win32_context);
    const std::uint32_t offset = win32_context->Esi;

    std::uint8_t value = 0;
    if (!ReadSegmentByte(
            context, segment_register, selector, offset, &value))
    {
        return false;
    }

    const std::uint8_t immediate = instruction[2];
    if (value == immediate)
    {
        win32_context->EFlags |= 0x40U;
    }
    else
    {
        win32_context->EFlags &= ~0x40U;
    }
    win32_context->EFlags &= ~1U;
    RecordGuestSegmentMemoryLoad(win32_context,
                                 context,
                                 0x80,
                                 segment_register,
                                 selector,
                                 offset,
                                 1,
                                 value);
    RecordLowMemoryAccess(win32_context,
                          context,
                          instruction[0],
                          offset,
                          value);
    win32_context->Eip += 3;
    return true;
}

bool HandleTracedMemoryStoreInstruction(CONTEXT* win32_context,
                                        ThreadContext* context)
{
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->guest_handler_phase,
            20);
    }
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    const std::uint32_t modrm_offset = instruction[0] == 0x66 ? 2U : 1U;
    if ((instruction[modrm_offset] & 0xC0U) == 0xC0U)
    {
        return false;
    }
    std::uint32_t destination = 0;
    std::uint32_t value = 0;
    std::uint32_t instruction_size = 0;
    std::uint32_t value_width = 4;
    std::uint32_t store_opcode = instruction[0];
    const char* source_kind = "unknown";

    if (instruction[0] == 0xC7)
    {
        source_kind = "mov-imm32";
        const std::uint8_t operation = (instruction[1] >> 3) & 0x07U;
        if (operation != 0 ||
            !DecodeModRmMemoryAddress(win32_context,
                                               instruction,
                                               &destination,
                                               &instruction_size))
        {
            return false;
        }
        const std::uint32_t immediate_offset = instruction_size;
        value =
            static_cast<std::uint32_t>(instruction[immediate_offset]) |
            (static_cast<std::uint32_t>(
                 instruction[immediate_offset + 1]) << 8) |
            (static_cast<std::uint32_t>(
                 instruction[immediate_offset + 2]) << 16) |
            (static_cast<std::uint32_t>(
                 instruction[immediate_offset + 3]) << 24);
        instruction_size += 4;
    }
    else if (instruction[0] == 0xC6)
    {
        source_kind = "mov-imm8";
        const std::uint8_t operation = (instruction[1] >> 3) & 0x07U;
        if (operation != 0 ||
            !DecodeModRmMemoryAddress(win32_context,
                                      instruction,
                                      &destination,
                                      &instruction_size))
        {
            return false;
        }
        value = instruction[instruction_size];
        ++instruction_size;
        value_width = 1;
    }
    else if (instruction[0] == 0x66 && instruction[1] == 0xC7)
    {
        store_opcode = 0x66C7;
        source_kind = "mov-imm16";
        const std::uint8_t operation = (instruction[2] >> 3) & 0x07U;
        std::uint32_t unprefixed_instruction_size = 0;
        if (operation != 0 ||
            !DecodeModRmMemoryAddress(
                win32_context,
                instruction + 1,
                &destination,
                &unprefixed_instruction_size))
        {
            return false;
        }
        const std::uint32_t immediate_offset =
            1 + unprefixed_instruction_size;
        value =
            static_cast<std::uint32_t>(instruction[immediate_offset]) |
            (static_cast<std::uint32_t>(
                 instruction[immediate_offset + 1]) << 8);
        instruction_size = immediate_offset + 2;
        value_width = 2;
    }
    else if (instruction[0] == 0x89)
    {
        source_kind = "mov-reg32";
        if (!DecodeModRmMemoryAddress(win32_context,
                                               instruction,
                                               &destination,
                                               &instruction_size))
        {
            return false;
        }
        const std::uint8_t source_register = (instruction[1] >> 3) & 0x07U;
        value = ReadGeneralRegister32(win32_context, source_register);
    }
    else if (instruction[0] == 0x88)
    {
        source_kind = "mov-reg8";
        if (!DecodeModRmMemoryAddress(win32_context,
                                      instruction,
                                      &destination,
                                      &instruction_size))
        {
            return false;
        }
        const std::uint8_t source_register = (instruction[1] >> 3) & 0x07U;
        value = ReadRegister8(*win32_context, source_register);
        value_width = 1;
    }
    else if (instruction[0] == 0x66 && instruction[1] == 0x89)
    {
        store_opcode = 0x6689;
        source_kind = "mov-reg16";
        std::uint32_t unprefixed_instruction_size = 0;
        if (!DecodeModRmMemoryAddress(win32_context,
                                      instruction + 1,
                                      &destination,
                                      &unprefixed_instruction_size))
        {
            return false;
        }
        const std::uint8_t source_register =
            (instruction[2] >> 3) & 0x07U;
        value = ReadRegister16(*win32_context, source_register);
        instruction_size = 1U + unprefixed_instruction_size;
        value_width = 2;
    }
    else
    {
        return false;
    }

    const std::uint32_t instruction_offset =
        static_cast<std::uint32_t>(win32_context->Eip) -
        context->runtime_base;
    if (instruction_offset == 0x000F393FU)
    {
        ++context->linexe_export_result_store_count;
        context->linexe_export_result_store_destination = destination;
        context->linexe_export_result_store_value = value;
    }

    void* destination_pointer = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(destination));
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->guest_handler_phase,
            21);
    }

    if (IsGuestRangeWritable(context, destination_pointer, value_width))
    {
        if (context->shared_live_telemetry != nullptr)
        {
            InterlockedExchange(
                &context->shared_live_telemetry->guest_handler_phase,
                22);
        }
        const bool written = value_width == 1
            ? WriteGuestBytes(context,
                              destination_pointer,
                              &value,
                              value_width)
            : value_width == 2
                ? WriteGuestUInt16(context,
                                   destination_pointer,
                                   static_cast<std::uint16_t>(value))
                : WriteGuestUInt32(context, destination_pointer, value);
        if (!written)
        {
            return false;
        }
        RecordGuestMemoryStore(win32_context,
                               context,
                               store_opcode,
                               destination,
                               value,
                               value_width,
                               source_kind,
                               true);
        win32_context->Eip += instruction_size;
        return true;
    }

    constexpr std::uint32_t kAllocatorSentinelShadowLimit = 0x00100000;
    const std::uint64_t runtime_end =
        static_cast<std::uint64_t>(context->runtime_base) +
        context->runtime_size;
    const std::uint64_t sentinel_limit =
        runtime_end + kAllocatorSentinelShadowLimit;
    const bool allocator_failure_sentinel =
        instruction[0] == 0xC7 && value == 0xFFFFFFFFU &&
        destination >= runtime_end && destination < sentinel_limit;
    const std::uint64_t destination_end =
        static_cast<std::uint64_t>(destination) + value_width;
    const bool begins_allocator_metadata =
        instruction[0] == 0x89 && context->shadow_memory_range_valid &&
        destination < context->shadow_memory_min_address &&
        context->shadow_memory_min_address - destination == value;
    const bool extends_allocator_metadata =
        instruction[0] == 0x89 && context->shadow_memory_range_valid &&
        destination >= context->shadow_memory_min_address &&
        static_cast<std::uint64_t>(destination) <=
            static_cast<std::uint64_t>(
                context->shadow_memory_max_address) + 1;
    const bool allocator_metadata_store =
        destination >= runtime_end && destination_end <= sentinel_limit &&
        (begins_allocator_metadata || extends_allocator_metadata);
    constexpr std::uint32_t kBoundaryObjectWindow = 64;
    const std::uint8_t boundary_modrm =
        instruction[0] == 0x66 ? instruction[2] : instruction[1];
    const std::uint8_t mod =
        static_cast<std::uint8_t>((boundary_modrm >> 6) & 0x03U);
    const std::uint8_t rm =
        static_cast<std::uint8_t>(boundary_modrm & 0x07U);
    const std::uint64_t base =
        ReadGeneralRegister32(win32_context, rm);
    const bool supported_boundary_store =
        instruction[0] == 0xC7 || instruction[0] == 0x89 || instruction[0] == 0x88 ||
        (instruction[0] == 0x66 && instruction[1] == 0xC7);
    const bool arena_boundary_object_store =
        supported_boundary_store && (mod == 0x01 || mod == 0x02) &&
        base < runtime_end && base + kBoundaryObjectWindow >= runtime_end &&
        destination >= runtime_end &&
        destination_end <= runtime_end + kBoundaryObjectWindow;
    const bool chained_boundary_object_store =
        supported_boundary_store && context->boundary_object_chain_valid &&
        (mod == 0x00 || mod == 0x01 || mod == 0x02) &&
        (base == context->boundary_object_chain_base ||
         base == context->boundary_object_chain_frontier) &&
        destination >= base &&
        destination_end <= base + kBoundaryObjectWindow &&
        destination_end <= context->boundary_object_chain_limit;

    const bool is_legitimate_shadow_store =
        allocator_failure_sentinel || allocator_metadata_store ||
        arena_boundary_object_store || chained_boundary_object_store;

    if (!is_legitimate_shadow_store)
    {
        if (destination >= context->runtime_base &&
            static_cast<std::uint64_t>(destination) < runtime_end)
        {
            return false;
        }
        if (context->last_dos_open_success ||
            context->last_dos_open_guest_path.empty())
        {
            return false;
        }
    }

    RecordGuestMemoryStore(win32_context,
                           context,
                           store_opcode,
                           destination,
                           value,
                           value_width,
                           source_kind,
                           false);
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->guest_handler_phase,
            23);
    }
    WriteShadowMemory(context, destination, value, value_width);
    if (arena_boundary_object_store || chained_boundary_object_store)
    {
        if (!context->boundary_object_chain_valid ||
            base == context->boundary_object_chain_frontier)
        {
            if (!context->boundary_object_chain_valid)
            {
                constexpr std::uint64_t kFallbackSpan = 4096;
                constexpr std::uint64_t kMaximumSpan = 0x00100000;
                const std::uint64_t observed_span =
                    static_cast<std::uint64_t>(win32_context->Esi) *
                    win32_context->Edx;
                const std::uint64_t chain_span =
                    observed_span >= kBoundaryObjectWindow &&
                            observed_span <= kMaximumSpan
                        ? observed_span
                        : kFallbackSpan;
                context->boundary_object_chain_limit =
                    runtime_end + chain_span;
            }
            context->boundary_object_chain_valid = true;
            context->boundary_object_chain_base =
                static_cast<std::uint32_t>(base);
        }
        context->boundary_object_chain_frontier = std::max(
            context->boundary_object_chain_frontier,
            static_cast<std::uint32_t>(destination_end));
    }
    win32_context->Eip += instruction_size;
    return true;
}

void AttachAllocatorReadProvenance(ThreadContext* context,
                                   std::uint32_t eip_offset,
                                   std::uint32_t source,
                                   std::uint32_t value)
{
    if (context == nullptr ||
        context->allocator_control_flow.observed_count == 0)
    {
        return;
    }

    const std::uint32_t sequence =
        context->allocator_control_flow.observed_count;
    const std::uint32_t slot =
        (sequence - 1) % kWin32AllocatorControlFlowTraceCapacity;
    Win32AllocatorControlFlowTraceEntry& entry =
        context->allocator_control_flow.trace[slot];
    if (!entry.valid || entry.sequence != sequence ||
        entry.eip_offset != eip_offset)
    {
        return;
    }

    entry.read_valid = true;
    entry.read_address = source;
    entry.read_value = value;
    entry.read_explicit_shadow = true;
    for (std::uint32_t index = 0; index < 4; ++index)
    {
        if (context->shadow_memory.find(source + index) ==
            context->shadow_memory.end())
        {
            entry.read_explicit_shadow = false;
            break;
        }
    }
    entry.read_zero_backed = !entry.read_explicit_shadow &&
        context->shadow_zero_payload_valid &&
        source >= context->shadow_zero_payload_begin &&
        static_cast<std::uint64_t>(source) + 4U <=
            context->shadow_zero_payload_end;

    const std::uint32_t stored_count = std::min(
        context->shadow_write_provenance_count,
        kShadowWriteProvenanceCapacity);
    const ShadowWriteProvenance* writer = nullptr;
    for (std::uint32_t reverse_index = 0;
         reverse_index < stored_count;
         ++reverse_index)
    {
        const std::uint32_t sequence =
            context->shadow_write_provenance_count - reverse_index;
        const std::uint32_t slot =
            (sequence - 1) % kShadowWriteProvenanceCapacity;
        const ShadowWriteProvenance& candidate =
            context->shadow_write_provenance[slot];
        const std::uint64_t candidate_end =
            static_cast<std::uint64_t>(candidate.destination) +
            candidate.width;
        if (candidate.sequence == sequence &&
            source >= candidate.destination &&
            static_cast<std::uint64_t>(source) + 4U <= candidate_end)
        {
            writer = &candidate;
            break;
        }
    }
    if (writer != nullptr)
    {
        entry.writer_valid = true;
        entry.writer_sequence = writer->sequence;
        entry.writer_eip_offset = writer->eip >= context->runtime_base
            ? writer->eip - context->runtime_base
            : writer->eip;
        entry.writer_opcode = writer->opcode;
        entry.writer_destination = writer->destination;
        entry.writer_value = writer->value;
        entry.writer_width = writer->width;
    }

    if (eip_offset == 0x000F7A62U && value == 0 &&
        !context->allocator_control_flow.root_transition_valid)
    {
        context->allocator_control_flow.root_transition_valid = true;
        context->allocator_control_flow.root_transition = entry;
    }
    if (eip_offset == 0x000F7A83U && source != 8U)
    {
        Win32AllocatorControlFlowObservation& observation =
            context->allocator_control_flow;
        if (value == 0 && !observation.null_link_transition_valid)
        {
            observation.null_link_transition_valid = true;
            observation.null_link_transition = entry;
        }
        else if (value == 0xFF000000U &&
                 !observation.poison_link_transition_valid)
        {
            observation.poison_link_transition_valid = true;
            observation.poison_link_transition = entry;
        }
    }
}

bool HandleTracedMemoryLoadInstruction(CONTEXT* win32_context,
                                       ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x8B)
    {
        return false;
    }
    if ((instruction[1] & 0xC0U) == 0xC0U)
    {
        return false;
    }

    std::uint32_t source = 0;
    std::uint32_t instruction_size = 0;
    if (!DecodeModRmMemoryAddress(win32_context,
                                           instruction,
                                           &source,
                                           &instruction_size))
    {
        return false;
    }

    const std::uint64_t instruction_offset =
        static_cast<std::uint64_t>(win32_context->Eip) -
        context->runtime_base;
    const bool allocator_probe = instruction_offset == 0x000F7A71ULL;
    const bool pending_before = context->pending_shadow_allocation_valid;
    const std::uint32_t pending_size_before =
        context->pending_shadow_allocation_size;
    auto record_allocator_probe = [&](const char* result) {
        if (!allocator_probe)
        {
            return;
        }
        Win32AllocatorProbeObservation& observation =
            context->allocator_probe;
        const std::uint32_t sequence = observation.observed_count + 1;
        const std::uint32_t slot =
            (sequence - 1) % kWin32AllocatorProbeTraceCapacity;
        Win32AllocatorProbeTraceEntry& entry = observation.trace[slot];
        entry.valid = true;
        entry.sequence = sequence;
        entry.eax = win32_context->Eax;
        entry.esi = win32_context->Esi;
        entry.source = source;
        entry.ds = context->guest_ds;
        entry.pending_before = pending_before;
        entry.pending_size_before = pending_size_before;
        entry.pending_after = context->pending_shadow_allocation_valid;
        entry.pending_size_after = context->pending_shadow_allocation_size;
        entry.result = result != nullptr ? result : "unknown";
        observation.observed_count = sequence;
        if (observation.trace_stored_count <
            kWin32AllocatorProbeTraceCapacity)
        {
            ++observation.trace_stored_count;
        }
        else
        {
            observation.trace_wrapped = true;
        }
    };

    std::uint32_t value = 0;
    const void* source_pointer = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(source));
    const bool read_from_guest =
        ReadGuestUInt32(context, source_pointer, &value);
    const bool read_from_shadow =
        !read_from_guest && ReadShadowUInt32(context, source, &value);
    if (!read_from_guest && !read_from_shadow)
    {
        std::uint32_t linear_address = 0;
        if (!repiu::runtime::TranslateSelectorOffset(
                context->selector_table,
                context->guest_ds,
                source,
                sizeof(value),
                &linear_address))
        {
            record_allocator_probe("rejected");
            return false;
        }
        const bool translated_read =
            linear_address < repiu::runtime::kDosLowMemorySize
                ? repiu::runtime::ReadDosLowMemoryUInt32(
                      context->dos_low_memory,
                      linear_address,
                      &value)
                : ReadGuestUInt32(
                      context,
                      reinterpret_cast<const void*>(
                          static_cast<std::uintptr_t>(linear_address)),
                      &value);
        if (!translated_read)
        {
            record_allocator_probe("rejected");
            return false;
        }
        const bool has_confirmed_allocation_size =
            win32_context->Eax == 0x0000002CU ||
            win32_context->Eax == 0x00001008U;
        bool captured = false;
        if (!context->pending_shadow_allocation_valid &&
            allocator_probe &&
            source == 0 &&
            has_confirmed_allocation_size)
        {
            context->pending_shadow_allocation_valid = true;
            context->pending_shadow_allocation_size = win32_context->Eax;
            captured = true;
        }
        record_allocator_probe(
            captured ? "captured"
                     : context->pending_shadow_allocation_valid
                           ? "pending-preserved"
                           : "zero-page");
    }
    else
    {
        record_allocator_probe("mapped-or-shadow");
    }

    if (instruction_offset >= 0x000F7A60ULL &&
        instruction_offset < 0x000F7AD5ULL)
    {
        AttachAllocatorReadProvenance(
            context,
            static_cast<std::uint32_t>(instruction_offset),
            source,
            value);
    }

    const std::uint8_t destination_register = (instruction[1] >> 3) & 0x07U;
    WriteGeneralRegister32(win32_context, destination_register, value);
    win32_context->Eip += instruction_size;
    HandleSegmentOverrideMemoryLoadInstruction(win32_context, context);
    return true;
}

bool HasEvenParity(std::uint8_t value)
{
    bool even_parity = true;
    while (value != 0)
    {
        even_parity = !even_parity;
        value = static_cast<std::uint8_t>(value & (value - 1U));
    }
    return even_parity;
}

void UpdateAdd32Flags(CONTEXT* win32_context,
                      std::uint32_t left,
                      std::uint32_t right,
                      std::uint32_t result)
{
    constexpr std::uint32_t kCarryFlag = 0x00000001U;
    constexpr std::uint32_t kParityFlag = 0x00000004U;
    constexpr std::uint32_t kAuxiliaryCarryFlag = 0x00000010U;
    constexpr std::uint32_t kZeroFlag = 0x00000040U;
    constexpr std::uint32_t kSignFlag = 0x00000080U;
    constexpr std::uint32_t kOverflowFlag = 0x00000800U;
    constexpr std::uint32_t kArithmeticFlags =
        kCarryFlag | kParityFlag | kAuxiliaryCarryFlag | kZeroFlag |
        kSignFlag | kOverflowFlag;

    win32_context->EFlags &= ~kArithmeticFlags;
    if (static_cast<std::uint64_t>(left) + right > 0xFFFFFFFFULL)
    {
        win32_context->EFlags |= kCarryFlag;
    }
    if (HasEvenParity(static_cast<std::uint8_t>(result & 0xFFU)))
    {
        win32_context->EFlags |= kParityFlag;
    }
    if (((left ^ right ^ result) & 0x10U) != 0)
    {
        win32_context->EFlags |= kAuxiliaryCarryFlag;
    }
    if (result == 0)
    {
        win32_context->EFlags |= kZeroFlag;
    }
    if ((result & 0x80000000U) != 0)
    {
        win32_context->EFlags |= kSignFlag;
    }
    if (((~(left ^ right) & (left ^ result)) & 0x80000000U) != 0)
    {
        win32_context->EFlags |= kOverflowFlag;
    }
}

std::uint8_t ReadGeneralRegister8(const CONTEXT* win32_context,
                                  std::uint8_t register_index)
{
    const std::uint8_t byte_register = register_index & 0x07U;
    if (byte_register < 4)
    {
        return static_cast<std::uint8_t>(
            ReadGeneralRegister32(win32_context, byte_register) & 0xFFU);
    }
    return static_cast<std::uint8_t>(
        (ReadGeneralRegister32(win32_context, byte_register - 4U) >> 8) &
        0xFFU);
}

void UpdateLogical32Flags(CONTEXT* win32_context, std::uint32_t result)
{
    constexpr std::uint32_t kCarryFlag = 0x00000001U;
    constexpr std::uint32_t kParityFlag = 0x00000004U;
    constexpr std::uint32_t kZeroFlag = 0x00000040U;
    constexpr std::uint32_t kSignFlag = 0x00000080U;
    constexpr std::uint32_t kOverflowFlag = 0x00000800U;
    constexpr std::uint32_t kDefinedLogicalFlags =
        kCarryFlag | kParityFlag | kZeroFlag | kSignFlag | kOverflowFlag;

    win32_context->EFlags &= ~kDefinedLogicalFlags;
    if (HasEvenParity(static_cast<std::uint8_t>(result & 0xFFU)))
    {
        win32_context->EFlags |= kParityFlag;
    }
    if (result == 0)
    {
        win32_context->EFlags |= kZeroFlag;
    }
    if ((result & 0x80000000U) != 0)
    {
        win32_context->EFlags |= kSignFlag;
    }
}

void UpdateSubtract8Flags(CONTEXT* win32_context,
                          std::uint8_t left,
                          std::uint8_t right,
                          std::uint8_t result)
{
    constexpr std::uint32_t kCarryFlag = 0x00000001U;
    constexpr std::uint32_t kParityFlag = 0x00000004U;
    constexpr std::uint32_t kAuxiliaryCarryFlag = 0x00000010U;
    constexpr std::uint32_t kZeroFlag = 0x00000040U;
    constexpr std::uint32_t kSignFlag = 0x00000080U;
    constexpr std::uint32_t kOverflowFlag = 0x00000800U;
    constexpr std::uint32_t kArithmeticFlags =
        kCarryFlag | kParityFlag | kAuxiliaryCarryFlag | kZeroFlag |
        kSignFlag | kOverflowFlag;

    win32_context->EFlags &= ~kArithmeticFlags;
    if (left < right)
    {
        win32_context->EFlags |= kCarryFlag;
    }
    if (HasEvenParity(result))
    {
        win32_context->EFlags |= kParityFlag;
    }
    if (((left ^ right ^ result) & 0x10U) != 0)
    {
        win32_context->EFlags |= kAuxiliaryCarryFlag;
    }
    if (result == 0)
    {
        win32_context->EFlags |= kZeroFlag;
    }
    if ((result & 0x80U) != 0)
    {
        win32_context->EFlags |= kSignFlag;
    }
    if ((((left ^ right) & (left ^ result)) & 0x80U) != 0)
    {
        win32_context->EFlags |= kOverflowFlag;
    }
}

bool HandleTracedMemoryAddInstruction(CONTEXT* win32_context,
                                      ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x03)
    {
        return false;
    }

    std::uint32_t source = 0;
    std::uint32_t instruction_size = 0;
    if (!DecodeModRmMemoryAddress(win32_context,
                                           instruction,
                                           &source,
                                           &instruction_size))
    {
        return false;
    }

    std::uint32_t source_value = 0;
    if (!ReadShadowUInt32(context, source, &source_value))
    {
        return false;
    }

    const std::uint8_t destination_register = (instruction[1] >> 3) & 0x07U;
    const std::uint32_t destination_value =
        ReadGeneralRegister32(win32_context, destination_register);
    const std::uint32_t result = destination_value + source_value;
    WriteGeneralRegister32(win32_context, destination_register, result);
    UpdateAdd32Flags(win32_context,
                     destination_value,
                     source_value,
                     result);
    win32_context->Eip += instruction_size;
    return true;
}

bool HandleTracedMemoryOrInstruction(CONTEXT* win32_context,
                                     ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x83 ||
        ((instruction[1] >> 3) & 0x07U) != 0x01U)
    {
        return false;
    }

    std::uint32_t destination = 0;
    std::uint32_t instruction_size = 0;
    if (!DecodeModRmMemoryAddress(win32_context,
                                           instruction,
                                           &destination,
                                           &instruction_size))
    {
        return false;
    }

    std::uint32_t destination_value = 0;
    void* destination_pointer = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(destination));
    const bool real_destination =
        ReadGuestUInt32(context, destination_pointer, &destination_value);
    if (!real_destination &&
        !ReadShadowUInt32(context, destination, &destination_value))
    {
        return false;
    }

    const std::uint32_t immediate = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(
            static_cast<std::int8_t>(instruction[instruction_size])));
    const std::uint32_t result = destination_value | immediate;
    if (real_destination)
    {
        if (!WriteGuestUInt32(context, destination_pointer, result))
        {
            return false;
        }
    }
    else
    {
        WriteShadowMemory(context, destination, result, 4);
    }
    RecordGuestMemoryStore(win32_context,
                           context,
                           0x83,
                           destination,
                           result,
                           4,
                           "or-imm8",
                           real_destination);
    const std::uint64_t instruction_offset =
        static_cast<std::uint64_t>(win32_context->Eip) -
        context->runtime_base;
    if (!real_destination && context->pending_shadow_allocation_valid &&
        instruction_offset == 0x000F7AD4ULL)
    {
        const std::uint32_t allocation_size =
            context->pending_shadow_allocation_size;
        const std::uint64_t payload_begin =
            static_cast<std::uint64_t>(destination) + 4U;
        const std::uint64_t payload_end =
            static_cast<std::uint64_t>(destination) +
            allocation_size - 4U;
        if (payload_begin < payload_end && payload_end <= 0xFFFFFFFFULL)
        {
            context->shadow_zero_payload_valid = true;
            context->shadow_zero_payload_begin =
                static_cast<std::uint32_t>(payload_begin);
            context->shadow_zero_payload_end =
                static_cast<std::uint32_t>(payload_end);
        }
        context->pending_shadow_allocation_valid = false;
        context->pending_shadow_allocation_size = 0;
    }
    UpdateLogical32Flags(win32_context, result);
    win32_context->Eip += instruction_size + 1;
    return true;
}

bool HandleTracedMemoryCompareByteInstruction(CONTEXT* win32_context,
                                              ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0x38)
    {
        return false;
    }

    std::uint32_t source = 0;
    std::uint32_t instruction_size = 0;
    if (!DecodeModRmMemoryAddress(win32_context,
                                           instruction,
                                           &source,
                                           &instruction_size))
    {
        return false;
    }

    std::uint8_t source_value = 0;
    if (!ReadShadowUInt8(context, source, &source_value))
    {
        return false;
    }

    const std::uint8_t register_index = (instruction[1] >> 3) & 0x07U;
    const std::uint8_t register_value =
        ReadGeneralRegister8(win32_context, register_index);
    const std::uint8_t result = static_cast<std::uint8_t>(
        source_value - register_value);
    UpdateSubtract8Flags(win32_context,
                         source_value,
                         register_value,
                         result);
    win32_context->Eip += instruction_size;
    return true;
}

bool HandleTracedMemoryTestInstruction(CONTEXT* win32_context,
                                       ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xF7 || instruction[1] != 0x07)
    {
        return false;
    }

    const std::uint32_t source = win32_context->Edi;
    const void* source_pointer = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(source));
    std::uint32_t source_value = 0;
    if (!ReadGuestUInt32(context, source_pointer, &source_value) &&
        !ReadShadowUInt32(context, source, &source_value))
    {
        return false;
    }

    const std::uint32_t mask =
        static_cast<std::uint32_t>(instruction[2]) |
        (static_cast<std::uint32_t>(instruction[3]) << 8) |
        (static_cast<std::uint32_t>(instruction[4]) << 16) |
        (static_cast<std::uint32_t>(instruction[5]) << 24);
    const std::uint32_t result = source_value & mask;

    win32_context->EFlags &= ~1U;
    win32_context->EFlags &= ~0x800U;
    if (result == 0)
    {
        win32_context->EFlags |= 0x40U;
    }
    else
    {
        win32_context->EFlags &= ~0x40U;
    }

    if ((result & 0x80000000U) != 0)
    {
        win32_context->EFlags |= 0x80U;
    }
    else
    {
        win32_context->EFlags &= ~0x80U;
    }

    win32_context->Eip += 6;
    return true;
}

bool HandleTracedFpuMemoryInstruction(CONTEXT* win32_context,
                                      ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xD9)
    {
        return false;
    }

    std::uint32_t address = 0;
    std::uint32_t instruction_size = 0;
    if (!DecodeModRmMemoryAddress(win32_context,
                                           instruction,
                                           &address,
                                           &instruction_size))
    {
        return false;
    }

    const std::uint8_t operation = (instruction[1] >> 3) & 0x07U;
    const void* source_pointer = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(address));
    if (operation == 0)
    {
        std::uint32_t value = 0;
        if (ReadGuestUInt32(context, source_pointer, &value))
        {
            context->last_traced_fpu_m32_value = value;
            context->has_last_traced_fpu_m32_value = true;
            win32_context->Eip += instruction_size;
            return true;
        }
        if (ReadShadowUInt32(context, address, &value))
        {
            context->last_traced_fpu_m32_value = value;
            context->has_last_traced_fpu_m32_value = true;
            win32_context->Eip += instruction_size;
            return true;
        }
        return false;
    }

    if (operation != 2 && operation != 3)
    {
        return false;
    }
    if (!context->has_last_traced_fpu_m32_value)
    {
        return false;
    }

    void* destination_pointer = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(address));
    const std::uint32_t value = context->last_traced_fpu_m32_value;
    if (IsGuestRangeWritable(context, destination_pointer, sizeof(value)))
    {
        if (!WriteGuestUInt32(context, destination_pointer, value))
        {
            return false;
        }
        RecordGuestMemoryStore(win32_context,
                               context,
                               0xD9,
                               address,
                               value,
                               4,
                               "fpu-m32",
                               true);
        win32_context->Eip += instruction_size;
        return true;
    }

    constexpr std::uint32_t kBoundaryObjectWindow = 64;
    const std::uint8_t mod =
        static_cast<std::uint8_t>((instruction[1] >> 6) & 0x03U);
    const std::uint8_t rm =
        static_cast<std::uint8_t>(instruction[1] & 0x07U);
    const std::uint64_t base =
        ReadGeneralRegister32(win32_context, rm);
    const std::uint64_t runtime_end =
        static_cast<std::uint64_t>(context->runtime_base) +
        context->runtime_size;
    const std::uint64_t address_end =
        static_cast<std::uint64_t>(address) + sizeof(value);
    const bool arena_boundary_object_store =
        (mod == 0x01 || mod == 0x02) && base < runtime_end &&
        base + kBoundaryObjectWindow >= runtime_end &&
        address >= runtime_end &&
        address_end <= runtime_end + kBoundaryObjectWindow;
    const bool chained_boundary_object_store =
        context->boundary_object_chain_valid &&
        (mod == 0x00 || mod == 0x01 || mod == 0x02) &&
        (base == context->boundary_object_chain_base ||
         base == context->boundary_object_chain_frontier) &&
        address >= base && address_end <= base + kBoundaryObjectWindow &&
        address_end <= context->boundary_object_chain_limit;
    if (!arena_boundary_object_store && !chained_boundary_object_store &&
        (context->last_dos_open_success ||
         context->last_dos_open_guest_path.empty()))
    {
        return false;
    }

    RecordGuestMemoryStore(win32_context,
                           context,
                           0xD9,
                           address,
                           value,
                           4,
                           "fpu-m32",
                           false);
    WriteShadowMemory(context, address, value, 4);
    if (arena_boundary_object_store || chained_boundary_object_store)
    {
        if (!context->boundary_object_chain_valid ||
            base == context->boundary_object_chain_frontier)
        {
            if (!context->boundary_object_chain_valid)
            {
                constexpr std::uint64_t kFallbackSpan = 4096;
                constexpr std::uint64_t kMaximumSpan = 0x00100000;
                const std::uint64_t observed_span =
                    static_cast<std::uint64_t>(win32_context->Esi) *
                    win32_context->Edx;
                const std::uint64_t chain_span =
                    observed_span >= kBoundaryObjectWindow &&
                            observed_span <= kMaximumSpan
                        ? observed_span
                        : kFallbackSpan;
                context->boundary_object_chain_limit =
                    runtime_end + chain_span;
            }
            context->boundary_object_chain_valid = true;
            context->boundary_object_chain_base =
                static_cast<std::uint32_t>(base);
        }
        context->boundary_object_chain_frontier = std::max(
            context->boundary_object_chain_frontier,
            static_cast<std::uint32_t>(address_end));
    }
    win32_context->Eip += instruction_size;
    return true;
}

bool HandleTracedDosInterrupt21(CONTEXT* win32_context,
                                ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr ||
        !IsGuestRangeReadable(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(win32_context->Eip)),
            2U))
    {
        return false;
    }
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xCD || instruction[1] != 0x21)
    {
        return false;
    }

    // Task 323: opened after the opcode match so the bucket measures serviced
    // INT 21h calls only, not every candidate check in the handler chain.
    const ExecutionTimeScope dos_time_scope(
        context->execution_time_profile.get(),
        ExecutionTimeBucket::kDosService);

    const std::uint16_t ax = static_cast<std::uint16_t>(
        win32_context->Eax & 0xFFFFU);
    const std::uint8_t ah = static_cast<std::uint8_t>(
        (win32_context->Eax >> 8) & 0xFF);
    switch (ah)
    {
        case 0x09:
            return HandleDosInterrupt21(win32_context, context);
        case 0x19:
            RecordHandledDosInterrupt(context, 0x21, ax);
            HandleDosGetCurrentDrive(win32_context, context);
            win32_context->Eip += 2;
            return true;
        case 0x30:
            RecordHandledDosInterrupt(context, 0x21, ax);
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | 0x0007U;
            win32_context->Ebx = 0;
            win32_context->Ecx = 0;
            win32_context->EFlags &= ~1U;
            win32_context->Eip += 2;
            return true;
        case 0x25:
            RecordHandledDosInterrupt(context, 0x21, ax);
            HandleDosSetInterruptVector(win32_context, context);
            win32_context->Eip += 2;
            return true;
        case 0x2A:
            RecordHandledDosInterrupt(context, 0x21, ax);
            HandleDosGetSystemDate(win32_context, context);
            win32_context->Eip += 2;
            return true;
        case 0x2B:
            RecordHandledDosInterrupt(context, 0x21, ax);
            HandleDosSetSystemDate(win32_context, context);
            win32_context->Eip += 2;
            return true;
        case 0x2C:
            RecordHandledDosInterrupt(context, 0x21, ax);
            HandleDosGetSystemTime(win32_context, context);
            win32_context->Eip += 2;
            return true;
        case 0x35:
            RecordHandledDosInterrupt(context, 0x21, ax);
            HandleDosGetInterruptVector(win32_context, context);
            win32_context->Eip += 2;
            return true;
        case 0x3B:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosChangeDirectory(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x3D:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosOpenFile(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x3E:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosCloseFile(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x3F:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosReadFile(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x40:
        {
            RecordHandledDosInterrupt(context, 0x21, ax);
            const void* text = reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(win32_context->Edx));
            const std::uint32_t byte_count = win32_context->Ecx;
            AppendConsoleOutput(
                context, text, byte_count, win32_context->Ebx == 2U);
            win32_context->Eax = byte_count;
            win32_context->EFlags &= ~1U;
            win32_context->Eip += 2;
            return true;
        }
        case 0x42:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosSeekFile(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x43:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosFileAttributes(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x44:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosIoctl(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x47:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosGetCurrentDirectory(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        case 0x4C:
            RecordHandledDosInterrupt(context, 0x21, ax);
            CaptureDosTermination(win32_context, context);
            RecoverFromHleExit(win32_context, context);
            return true;
        case 0xFF:
            RecordHandledDosInterrupt(context, 0x21, ax);
            return HandleDosInterrupt21(win32_context, context);
        case 0xED:
            RecordHandledDosInterrupt(context, 0x21, ax);
            win32_context->Eax &= 0xFFFFFF00U;
            win32_context->EFlags &= ~1U;
            win32_context->Eip += 2;
            return true;
        case 0x4A:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosResizeMemoryBlock(win32_context, context))
            {
                return false;
            }
            win32_context->Eip += 2;
            return true;
        default:
        {
            // Task 397: name the function on the traced path too. Backends that
            // run with enable_dos_hle off never reach HandleDosInterrupt21's
            // default branch, so an unimplemented service used to surface only
            // as a generic "unhandled HLE trap candidate" and had to be
            // identified by matching the logged byte window against the
            // original executable.
            std::ostringstream stream;
            stream << "unsupported DOS INT 21h AH=0x"
                   << std::hex << static_cast<unsigned>(ah);
            context->hle_message = stream.str();
            return false;
        }
    }
}

bool HandleTracedBiosInterrupt16(CONTEXT* win32_context,
                                 ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr ||
        !IsGuestRangeReadable(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(win32_context->Eip)),
            2U))
    {
        return false;
    }
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xCD || instruction[1] != 0x16)
    {
        return false;
    }

    return HandleBiosInterrupt16(win32_context, context);
}

void RecordUnsupportedTracedSoftwareInterrupt(CONTEXT* win32_context,
                                              ThreadContext* context)
{
    // Task 401: backends running with enable_dos_hle off never reach
    // HandleDosHleInstruction, which is where an unrecognised INT vector used
    // to be named. Both pumpit3 stops so far -- INT 21h AH=2Ah and INT 16h --
    // had to be identified by matching the logged byte window against the
    // original executable because of that.
    if (win32_context == nullptr || context == nullptr ||
        !IsGuestRangeReadable(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(win32_context->Eip)),
            2U))
    {
        return;
    }
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xCD || !context->hle_message.empty())
    {
        return;
    }

    std::ostringstream stream;
    stream << "unsupported software interrupt 0x"
           << std::hex << static_cast<unsigned>(instruction[1]);
    context->hle_message = stream.str();
}

bool HandleTracedDosInterrupt2F(CONTEXT* win32_context,
                                ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr ||
        !IsGuestRangeReadable(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(win32_context->Eip)),
            2U))
    {
        return false;
    }
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xCD || instruction[1] != 0x2F)
    {
        return false;
    }

    return HandleDosInterrupt2F(win32_context, context);
}

bool HandleTracedDpmiInterrupt31(CONTEXT* win32_context,
                                 ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr ||
        !IsGuestRangeReadable(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(win32_context->Eip)),
            2U))
    {
        return false;
    }
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xCD || instruction[1] != 0x31)
    {
        return false;
    }

    return HandleDpmiInterrupt31(win32_context, context);
}

bool HandleTracedMouseInterrupt33(CONTEXT* win32_context,
                                  ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr ||
        !IsGuestRangeReadable(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(win32_context->Eip)),
            2U))
    {
        return false;
    }
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if (instruction[0] != 0xCD || instruction[1] != 0x33)
    {
        return false;
    }

    return HandleMouseInterrupt33(win32_context, context);
}

bool HandleRepCmpsbInstruction(CONTEXT* win32_context,
                               ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if ((instruction[0] != 0xF2 && instruction[0] != 0xF3) ||
        instruction[1] != 0xA6)
    {
        return false;
    }
    const bool decrement =
        (win32_context->EFlags & 0x00000400U) != 0;
    while (win32_context->Ecx != 0)
    {
        std::uint32_t source = 0;
        std::uint32_t destination = 0;
        if (!ResolveSegmentLinearRange(context, context->guest_ds,
                                       win32_context->Esi, 1, false,
                                       &source) ||
            !ResolveSegmentLinearRange(context, context->guest_es,
                                       win32_context->Edi, 1, false,
                                       &destination))
        {
            return false;
        }
        const std::uint8_t left = *reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(source));
        const std::uint8_t right = *reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(destination));
        const std::uint8_t result = static_cast<std::uint8_t>(left - right);
        UpdateSubtract8Flags(win32_context, left, right, result);
        win32_context->Esi += decrement ? -1 : 1;
        win32_context->Edi += decrement ? -1 : 1;
        --win32_context->Ecx;
        const bool equal = result == 0;
        if ((instruction[0] == 0xF3 && !equal) ||
            (instruction[0] == 0xF2 && equal))
        {
            break;
        }
    }
    win32_context->Eip += 2;
    return true;
}

bool CopyHostMemoryWithoutVehRecursion(ThreadContext* context,
                                       std::uint32_t destination,
                                       const void* source,
                                       std::uint32_t byte_count,
                                       std::uint32_t* failure_stage,
                                       std::uint32_t* windows_error)
{
    if (source == nullptr || byte_count == 0)
    {
        return byte_count == 0;
    }
    std::vector<std::uint8_t> temporary(byte_count);
    SIZE_T bytes_read = 0;
    HANDLE process = GetCurrentProcess();
    if (!ReadProcessMemory(process,
                           source,
                           temporary.data(),
                           byte_count,
                           &bytes_read) ||
        bytes_read != byte_count)
    {
        if (failure_stage != nullptr)
        {
            *failure_stage = 1;
        }
        if (windows_error != nullptr)
        {
            *windows_error = GetLastError();
        }
        return false;
    }
    if (!WriteGuestBytes(
            context,
            reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(destination)),
            temporary.data(),
            byte_count))
    {
        if (failure_stage != nullptr)
        {
            *failure_stage = 2;
        }
        if (windows_error != nullptr)
        {
            *windows_error = GetLastError();
        }
        return false;
    }
    return true;
}

bool HandleRepMovsInstruction(CONTEXT* win32_context,
                              ThreadContext* context)
{
    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        win32_context->Eip);
    if ((instruction[0] != 0xF2 && instruction[0] != 0xF3) ||
        (instruction[1] != 0xA4 && instruction[1] != 0xA5) ||
        (win32_context->EFlags & 0x00000400U) != 0)
    {
        return false;
    }

    const std::uint32_t width = instruction[1] == 0xA5 ? 4U : 1U;
    const std::uint64_t byte_count64 =
        static_cast<std::uint64_t>(win32_context->Ecx) * width;
    if (byte_count64 > 0xFFFFFFFFULL)
    {
        return false;
    }
    const std::uint32_t byte_count =
        static_cast<std::uint32_t>(byte_count64);
    std::uint32_t source = 0;
    std::uint32_t destination = 0;
    bool source_is_low_memory = false;
    bool destination_is_low_memory = false;
    if (byte_count != 0 &&
        static_cast<std::uint64_t>(win32_context->Esi) + byte_count <=
            repiu::runtime::kDosLowMemorySize)
    {
        source = static_cast<std::uint32_t>(win32_context->Esi);
        source_is_low_memory = true;
    }
    else if (byte_count != 0 &&
             !ResolveSegmentLinearRange(context,
                                        context->guest_ds,
                                        win32_context->Esi,
                                        byte_count,
                                        false,
                                        &source))
    {
        return false;
    }
    if (byte_count != 0)
    {
        if (static_cast<std::uint64_t>(win32_context->Edi) + byte_count <=
                repiu::runtime::kDosLowMemorySize)
        {
            destination = static_cast<std::uint32_t>(win32_context->Edi);
            destination_is_low_memory = true;
        }
        else if (repiu::runtime::TranslateSelectorOffset(
                context->selector_table,
                context->guest_es,
                win32_context->Edi,
                byte_count,
                &destination) &&
            destination < repiu::runtime::kDosLowMemorySize &&
            static_cast<std::uint64_t>(destination) + byte_count <=
                repiu::runtime::kDosLowMemorySize)
        {
            destination_is_low_memory = true;
        }
        else if (!ResolveSegmentLinearRange(context,
                                            context->guest_es,
                                            win32_context->Edi,
                                            byte_count,
                                            true,
                                            &destination))
        {
            return false;
        }
    }
    if (byte_count != 0)
    {
        if (destination_is_low_memory)
        {
            const auto* bytes = source_is_low_memory
                ? context->dos_low_memory.bytes.data() + source
                : reinterpret_cast<const std::uint8_t*>(
                      static_cast<std::uintptr_t>(source));
            for (std::uint32_t index = 0; index < byte_count; ++index)
            {
                if (!repiu::runtime::WriteDosLowMemory(
                        &context->dos_low_memory,
                        destination + index,
                        bytes[index],
                        1U))
                {
                    return false;
                }
            }
        }
        else
        {
            const void* source_pointer = source_is_low_memory
                ? static_cast<const void*>(
                      context->dos_low_memory.bytes.data() + source)
                : reinterpret_cast<const void*>(
                      static_cast<std::uintptr_t>(source));
            std::uint32_t failure_stage = 0;
            std::uint32_t windows_error = 0;
            if (!CopyHostMemoryWithoutVehRecursion(context,
                                                   destination,
                                                   source_pointer,
                                                   byte_count,
                                                   &failure_stage,
                                                   &windows_error))
            {
                ++context->rep_movs_copy_failure_count;
                context->last_rep_movs_copy_failure_stage = failure_stage;
                context->last_rep_movs_copy_error = windows_error;
                context->last_rep_movs_copy_source = source;
                context->last_rep_movs_copy_destination = destination;
                context->last_rep_movs_copy_bytes = byte_count;
                return false;
            }
        }
    }
    win32_context->Esi += byte_count;
    win32_context->Edi += byte_count;
    win32_context->Ecx = 0;
    win32_context->Eip += 2;
    context->diagnostic_progress_count.fetch_add(
        1,
        std::memory_order_relaxed);
    return true;
}

} // namespace repiu::platform::win32
