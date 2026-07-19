#include "dpmi_mscdex_services.h"
#include "execution_internal.h"
#include "guest_memory_access.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <vector>

namespace repiu::platform::win32
{

std::uint8_t* ResolveMscdexBuffer(ThreadContext* context,
                                  std::uint16_t segment,
                                  std::uint16_t offset,
                                  std::uint32_t bytes,
                                  std::uint32_t* resolve_kind)
{
    if (resolve_kind != nullptr)
    {
        *resolve_kind = 0;
    }
    std::uint32_t linear = 0;
    if (ResolveSegmentLinearRange(context, segment, offset, bytes, true,
                                  &linear))
    {
        if (resolve_kind != nullptr)
        {
            *resolve_kind = 1;
        }
        return reinterpret_cast<std::uint8_t*>(
            static_cast<std::uintptr_t>(linear));
    }
    const std::uint32_t real_linear =
        static_cast<std::uint32_t>(segment) * 16U + offset;
    if (context->dos_low_memory.valid &&
        static_cast<std::uint64_t>(real_linear) + bytes <=
            context->dos_low_memory.bytes.size())
    {
        if (resolve_kind != nullptr)
        {
            *resolve_kind = 2;
        }
        return context->dos_low_memory.bytes.data() + real_linear;
    }
    return nullptr;
}

std::uint32_t ReadPacketU32(const std::uint8_t* packet,
                            std::size_t offset)
{
    std::uint32_t value = 0;
    std::memcpy(&value, packet + offset, sizeof(value));
    return value;
}

void WritePacketU16(std::uint8_t* packet, std::size_t offset,
                    std::uint16_t value)
{
    std::memcpy(packet + offset, &value, sizeof(value));
}

void WritePacketU32(std::uint8_t* packet, std::size_t offset,
                    std::uint32_t value)
{
    std::memcpy(packet + offset, &value, sizeof(value));
}

void WritePacketMsf3(std::uint8_t* packet, std::size_t offset,
                     std::uint32_t msf)
{
    packet[offset] = static_cast<std::uint8_t>((msf >> 16U) & 0xFFU);
    packet[offset + 1U] = static_cast<std::uint8_t>((msf >> 8U) & 0xFFU);
    packet[offset + 2U] = static_cast<std::uint8_t>(msf & 0xFFU);
}

std::uint32_t MscdexMsfToLba(std::uint32_t msf)
{
    const std::uint32_t minute = (msf >> 16U) & 0xFFU;
    const std::uint32_t second = (msf >> 8U) & 0xFFU;
    const std::uint32_t frame = msf & 0xFFU;
    const std::uint32_t absolute = (minute * 60U + second) * 75U + frame;
    return absolute >= 150U ? absolute - 150U : 0U;
}

std::uint32_t MscdexLbaToMsf(std::uint32_t lba)
{
    lba += 150U;
    return ((lba / (60U * 75U)) << 16U) |
        (((lba / 75U) % 60U) << 8U) | (lba % 75U);
}

bool HandleMscdexIoctl(ThreadContext* context, std::uint8_t* request)
{
    const std::uint16_t offset = static_cast<std::uint16_t>(
        request[14] | (static_cast<std::uint16_t>(request[15]) << 8U));
    const std::uint16_t segment = static_cast<std::uint16_t>(
        request[16] | (static_cast<std::uint16_t>(request[17]) << 8U));
    const std::uint16_t length = static_cast<std::uint16_t>(
        request[18] | (static_cast<std::uint16_t>(request[19]) << 8U));
    std::uint8_t* control = ResolveMscdexBuffer(
        context, segment, offset, std::max<std::uint16_t>(length, 16U));
    if (control == nullptr)
    {
        return false;
    }
    const auto& tracks = context->cd_image.tracks();
    switch (control[0])
    {
        case 6:  // device status
            if (length < 5U) return false;
            WritePacketU32(control, 1, 0x00000290U);
            return true;
        case 9:  // media changed
            if (length < 2U) return false;
            control[1] = 1U;
            return true;
        case 10:  // audio disc information
            if (length < 7U || tracks.empty()) return false;
            control[1] = tracks.front().number;
            control[2] = tracks.back().number;
            WritePacketU32(control, 3,
                           MscdexLbaToMsf(context->cd_image.lead_out_lba()));
            return true;
        case 11:  // audio track information
        {
            if (length < 7U) return false;
            const repiu::media::ChdCdTrack* track =
                context->cd_image.FindTrack(control[1]);
            if (track == nullptr) return false;
            WritePacketU32(control, 2, MscdexLbaToMsf(track->start_lba));
            control[6] = track->audio ? 0U : 0x40U;
            return true;
        }
        case 12:  // Q-channel information
        {
            if (length < 11U) return false;
            const std::uint32_t lba = context->cd_audio.current_lba();
            const repiu::media::ChdCdTrack* track =
                context->cd_image.FindTrackByLba(lba);
            control[1] = track != nullptr && !track->audio ? 0x40U : 0U;
            control[2] = track != nullptr ? track->number : 0U;
            control[3] = 1U;
            WritePacketMsf3(control, 4, MscdexLbaToMsf(
                track != nullptr ? lba - track->start_lba : 0U));
            control[7] = 0U;
            WritePacketMsf3(control, 8, MscdexLbaToMsf(lba));
            return true;
        }
        case 15:  // audio status
            if (length < 11U) return false;
            WritePacketU16(control, 1,
                context->cd_audio.playing() ? 0U : 1U);
            WritePacketU32(control, 3,
                           MscdexLbaToMsf(context->cd_audio.current_lba()));
            WritePacketU32(control, 7, 0U);
            return true;
        default:
            return false;
    }
}


bool HandleDpmiInterrupt31(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint16_t ax = static_cast<std::uint16_t>(
        win32_context->Eax & 0xFFFF);
    if (ax == 0x0000)
    {
        const std::uint16_t count = static_cast<std::uint16_t>(
            win32_context->Ecx & 0xFFFFU);
        ++context->dpmi_allocate_call_count;
        context->dpmi_last_allocate_requested_count = count;
        std::uint16_t first_selector = 0;
        bool success = count != 0;
        for (std::uint32_t index = 0; success && index < count; ++index)
        {
            std::uint16_t selector = 0;
            success = repiu::runtime::AllocateSelector(
                &context->dpmi_selector_allocator, &selector) &&
                repiu::runtime::RegisterDescriptor(
                    &context->selector_table,
                    {selector, 0, 0, 0x0092U, true});
            if (index == 0)
            {
                first_selector = selector;
            }
        }
        RecordHandledDosInterrupt(context, 0x31, ax);
        if (success)
        {
            context->dpmi_last_allocated_selector = first_selector;
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | first_selector;
            win32_context->EFlags &= ~1U;
        }
        else
        {
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | 0x8011U;
            win32_context->EFlags |= 1U;
        }
        win32_context->Eip += 2;
        return true;
    }
    if (ax == 0x0300 && (win32_context->Ebx & 0xFFU) == 0x2FU)
    {
        // DPMI 0.9 real-mode register structure: FLAGS is a 16-bit word at
        // 0x20 and ES is the 16-bit word at 0x22 (0x24 is DS).
        constexpr std::size_t kRealModeFrameBytes = 0x34U;
        constexpr std::size_t kFrameEbxOffset = 0x10U;
        constexpr std::size_t kFrameEcxOffset = 0x18U;
        constexpr std::size_t kFrameEaxOffset = 0x1CU;
        constexpr std::size_t kFrameFlagsOffset = 0x20U;
        constexpr std::size_t kFrameEsOffset = 0x22U;
        void* frame = reinterpret_cast<void*>(static_cast<std::uintptr_t>(
            win32_context->Edi));
        if (!context->fatal_breakpoint_continued)
        {
            if (!IsGuestRangeWritable(
                    context, frame, kRealModeFrameBytes))
            {
                return false;
            }
            auto* bytes = static_cast<std::uint8_t*>(frame);
            std::uint32_t frame_eax = 0;
            std::uint32_t frame_ebx = 0;
            std::uint32_t frame_ecx = 0;
            std::uint16_t frame_es = 0;
            std::memcpy(&frame_eax, bytes + kFrameEaxOffset,
                        sizeof(frame_eax));
            std::memcpy(&frame_ebx, bytes + kFrameEbxOffset,
                        sizeof(frame_ebx));
            std::memcpy(&frame_ecx, bytes + kFrameEcxOffset,
                        sizeof(frame_ecx));
            std::memcpy(&frame_es, bytes + kFrameEsOffset,
                        sizeof(frame_es));
            if (context->shared_live_telemetry != nullptr)
            {
                InterlockedExchange(
                    &context->shared_live_telemetry->dpmi_frame_eax,
                    static_cast<long>(frame_eax));
                InterlockedExchange(
                    &context->shared_live_telemetry->dpmi_frame_ebx,
                    static_cast<long>(frame_ebx));
                InterlockedExchange(
                    &context->shared_live_telemetry->dpmi_frame_ecx,
                    static_cast<long>(frame_ecx));
            }
            const std::uint16_t frame_ax = static_cast<std::uint16_t>(
                frame_eax & 0xFFFFU);
            if (frame_ax == 0x1500U)
            {
                frame_ebx = (frame_ebx & 0xFFFF0000U) |
                    (context->mscdex_available ? 1U : 0U);
                frame_ecx = (frame_ecx & 0xFFFF0000U) |
                    (context->mscdex_available ? context->mscdex_drive : 0U);
                std::memcpy(bytes + kFrameEbxOffset, &frame_ebx,
                            sizeof(frame_ebx));
                std::memcpy(bytes + kFrameEcxOffset, &frame_ecx,
                            sizeof(frame_ecx));
            }
            else if (frame_ax == 0x1510U)
            {
                std::uint16_t frame_flags = 0;
                std::memcpy(&frame_flags, bytes + kFrameFlagsOffset,
                            sizeof(frame_flags));
                const bool called = context->mscdex_available &&
                    (frame_ecx & 0xFFFFU) == context->mscdex_drive &&
                    HandleMscdexRequest(
                        context, frame_es,
                        static_cast<std::uint16_t>(frame_ebx & 0xFFFFU));
                if (called)
                {
                    frame_flags = static_cast<std::uint16_t>(
                        frame_flags & ~1U);
                }
                else
                {
                    frame_eax = (frame_eax & 0xFFFF0000U) | 0x000FU;
                    frame_flags |= 1U;
                }
                std::memcpy(bytes + kFrameEaxOffset, &frame_eax,
                            sizeof(frame_eax));
                std::memcpy(bytes + kFrameFlagsOffset, &frame_flags,
                            sizeof(frame_flags));
            }
            else
            {
                return false;
            }
        }
        RecordHandledDosInterrupt(context, 0x31, ax);
        win32_context->EFlags &= ~1U;
        win32_context->Eip += 2;
        return true;
    }

    if (ax == 0x0006)
    {
        const std::uint16_t selector = static_cast<std::uint16_t>(
            win32_context->Ebx & 0xFFFFU);
        const repiu::runtime::GuestDescriptor* descriptor =
            repiu::runtime::FindDescriptor(context->selector_table, selector);
        RecordHandledDosInterrupt(context, 0x31, ax);
        if (descriptor == nullptr || !descriptor->present)
        {
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | 0x8022U;
            win32_context->EFlags |= 1U;
        }
        else
        {
            win32_context->Ecx =
                (win32_context->Ecx & 0xFFFF0000U) |
                ((descriptor->base >> 16) & 0xFFFFU);
            win32_context->Edx =
                (win32_context->Edx & 0xFFFF0000U) |
                (descriptor->base & 0xFFFFU);
            win32_context->EFlags &= ~1U;
        }
        win32_context->Eip += 2;
        return true;
    }

    if (ax == 0x0007)
    {
        const std::uint16_t selector = static_cast<std::uint16_t>(
            win32_context->Ebx & 0xFFFFU);
        const repiu::runtime::GuestDescriptor* existing =
            repiu::runtime::FindDescriptor(context->selector_table, selector);
        RecordHandledDosInterrupt(context, 0x31, ax);
        if (existing == nullptr || !existing->present)
        {
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | 0x8022U;
            win32_context->EFlags |= 1U;
        }
        else
        {
            repiu::runtime::GuestDescriptor updated = *existing;
            updated.base = ((win32_context->Ecx & 0xFFFFU) << 16) |
                           (win32_context->Edx & 0xFFFFU);
            if (!repiu::runtime::RegisterDescriptor(
                    &context->selector_table, updated))
            {
                return false;
            }
            win32_context->EFlags &= ~1U;
        }
        win32_context->Eip += 2;
        return true;
    }
    if (ax == 0x0008 || ax == 0x0009)
    {
        const std::uint16_t selector = static_cast<std::uint16_t>(
            win32_context->Ebx & 0xFFFFU);
        const repiu::runtime::GuestDescriptor* existing =
            repiu::runtime::FindDescriptor(context->selector_table, selector);
        RecordHandledDosInterrupt(context, 0x31, ax);
        if (existing == nullptr || !existing->present)
        {
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | 0x8022U;
            win32_context->EFlags |= 1U;
        }
        else
        {
            repiu::runtime::GuestDescriptor updated = *existing;
            if (ax == 0x0008)
            {
                updated.limit = ((win32_context->Ecx & 0xFFFFU) << 16) |
                                (win32_context->Edx & 0xFFFFU);
            }
            else
            {
                updated.flags = win32_context->Ecx & 0xFFFFU;
            }
            if (!repiu::runtime::RegisterDescriptor(
                    &context->selector_table, updated))
            {
                return false;
            }
            win32_context->EFlags &= ~1U;
        }
        win32_context->Eip += 2;
        return true;
    }
    if (ax == 0x0400)
    {
        RecordHandledDosInterrupt(context, 0x31, ax);
        win32_context->Eax &= 0xFFFF0000U;
        win32_context->EFlags &= ~1U;
        win32_context->Eip += 2;
        return true;
    }

    if (ax == 0x0100)
    {
        const std::uint16_t paragraphs = static_cast<std::uint16_t>(
            win32_context->Ebx & 0xFFFFU);
        const std::uint32_t size = static_cast<std::uint32_t>(paragraphs) * 16U;

        RecordHandledDosInterrupt(context, 0x31, ax);

        bool allocated = false;
        std::uint16_t segment = 0;
        std::uint16_t selector = 0;

        if (size > 0 && context->dpmi_low_memory_bump_offset + size <= repiu::runtime::kDosLowMemorySize)
        {
            std::size_t slot_index = context->dpmi_real_mode_blocks.size();
            for (std::size_t index = 0; index < context->dpmi_real_mode_blocks.size(); ++index)
            {
                if (!context->dpmi_real_mode_blocks[index].active)
                {
                    slot_index = index;
                    break;
                }
            }

            if (slot_index < context->dpmi_real_mode_blocks.size() &&
                repiu::runtime::AllocateSelector(&context->dpmi_selector_allocator, &selector))
            {
                const std::uint32_t offset = context->dpmi_low_memory_bump_offset;
                if (repiu::runtime::RegisterDescriptor(
                        &context->selector_table,
                        {selector, offset, size - 1U, 0x0092U, true}))
                {
                    segment = static_cast<std::uint16_t>(offset / 16U);

                    ThreadContext::RealModeBlock& block = context->dpmi_real_mode_blocks[slot_index];
                    block.selector = selector;
                    block.offset = offset;
                    block.size = size;
                    block.active = true;

                    context->dpmi_low_memory_bump_offset += size;
                    context->dpmi_low_memory_bump_offset = (context->dpmi_low_memory_bump_offset + 15U) & ~15U;

                    allocated = true;
                }
            }
        }

        if (allocated)
        {
            win32_context->Eax = (win32_context->Eax & 0xFFFF0000U) | segment;
            win32_context->Edx = (win32_context->Edx & 0xFFFF0000U) | selector;
            win32_context->EFlags &= ~1U;
        }
        else
        {
            win32_context->Eax = (win32_context->Eax & 0xFFFF0000U) | 0x8011U;
            win32_context->EFlags |= 1U;
        }
        win32_context->Eip += 2;
        return true;
    }

    if (ax == 0x0101)
    {
        const std::uint16_t selector = static_cast<std::uint16_t>(
            win32_context->Edx & 0xFFFFU);

        RecordHandledDosInterrupt(context, 0x31, ax);

        bool freed = false;
        for (std::size_t index = 0; index < context->dpmi_real_mode_blocks.size(); ++index)
        {
            ThreadContext::RealModeBlock& block = context->dpmi_real_mode_blocks[index];
            if (block.active && block.selector == selector)
            {
                repiu::runtime::RegisterDescriptor(
                    &context->selector_table,
                    {selector, block.offset, block.size - 1U, 0x0092U, false});

                block.active = false;
                freed = true;
                break;
            }
        }

        if (freed)
        {
            win32_context->EFlags &= ~1U;
        }
        else
        {
            win32_context->Eax = (win32_context->Eax & 0xFFFF0000U) | 0x8022U;
            win32_context->EFlags |= 1U;
        }
        win32_context->Eip += 2;
        return true;
    }

    if (ax == 0x0204)
    {
        const std::uint8_t vector = static_cast<std::uint8_t>(win32_context->Ebx & 0xFFU);
        RecordHandledDosInterrupt(context, 0x31, ax);
        const DpmiInterruptVectorShadow& shadow = context->dpmi_interrupt_vectors[vector];
        win32_context->Ecx = (win32_context->Ecx & 0xFFFF0000U) | shadow.selector;
        win32_context->Edx = shadow.offset;
        win32_context->EFlags &= ~1U;
        win32_context->Eip += 2;
        return true;
    }

    if (ax == 0x0205)
    {
        const std::uint8_t vector = static_cast<std::uint8_t>(win32_context->Ebx & 0xFFU);
        RecordHandledDosInterrupt(context, 0x31, ax);
        DpmiInterruptVectorShadow& shadow = context->dpmi_interrupt_vectors[vector];
        shadow.selector = static_cast<std::uint16_t>(win32_context->Ecx & 0xFFFFU);
        shadow.offset = win32_context->Edx;
        shadow.valid = true;
        fprintf(stderr, "[repiu-live] DPMI INT 31h AX=0205 vector 0x%02X set to %04X:%08X\n", vector, shadow.selector, shadow.offset);
        win32_context->EFlags &= ~1U;
        win32_context->Eip += 2;
        return true;
    }

    std::ostringstream stream;
    stream << "unsupported DPMI INT 31h AX=0x"
           << std::hex << static_cast<unsigned>(ax);
    context->hle_message = stream.str();
    return false;
}

bool HandleMouseInterrupt33(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint16_t ax = static_cast<std::uint16_t>(
        win32_context->Eax & 0xFFFF);
    if (ax == 0x0000)
    {
        RecordHandledDosInterrupt(context, 0x33, ax);
        win32_context->Eax &= 0xFFFF0000U;
        win32_context->Ebx &= 0xFFFF0000U;
        win32_context->Eip += 2;
        return true;
    }
    if (ax == 0x0002)
    {
        RecordHandledDosInterrupt(context, 0x33, ax);
        win32_context->Eip += 2;
        return true;
    }

    std::ostringstream stream;
    stream << "unsupported mouse INT 33h AX=0x"
           << std::hex << static_cast<unsigned>(ax);
    context->hle_message = stream.str();
    return false;
}

bool HandleMscdexRequest(ThreadContext* context,
                         std::uint16_t segment,
                         std::uint16_t offset)
{
    std::uint32_t resolve_kind = 0;
    std::uint8_t* request =
        ResolveMscdexBuffer(context, segment, offset, 26U, &resolve_kind);
    context->mscdex_frame_es = segment;
    context->mscdex_last_resolve_kind = resolve_kind;
    std::uint32_t header_bytes = 0;
    if (request != nullptr)
    {
        std::memcpy(&header_bytes, request, sizeof(header_bytes));
    }
    context->mscdex_last_header_bytes = header_bytes;
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->mscdex_frame_es,
            static_cast<long>(segment));
        InterlockedExchange(
            &context->shared_live_telemetry->mscdex_resolve_kind,
            static_cast<long>(resolve_kind));
        InterlockedExchange(
            &context->shared_live_telemetry->mscdex_header,
            static_cast<long>(header_bytes));
    }
    if (request == nullptr || request[0] < 13U)
    {
        ++context->mscdex_decline_count;
        context->mscdex_last_decline_reason = request == nullptr ? 1U : 2U;
        if (context->shared_live_telemetry != nullptr)
        {
            InterlockedExchange(
                &context->shared_live_telemetry->mscdex_decline_reason,
                static_cast<long>(context->mscdex_last_decline_reason));
        }
        return false;
    }
    ++context->mscdex_request_count;
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->mscdex_request_count,
            static_cast<long>(context->mscdex_request_count));
        InterlockedExchange(
            &context->shared_live_telemetry->mscdex_last_command,
            static_cast<long>(request[2]));
    }
    bool success = false;
    switch (request[2])
    {
        case 0x03:
            success = HandleMscdexIoctl(context, request);
            break;
        case 0x84:
        {
            const std::uint32_t start = request[13] == 1U
                ? MscdexMsfToLba(ReadPacketU32(request, 14))
                : ReadPacketU32(request, 14);
            success = context->cd_audio_available &&
                context->cd_audio.Play(start, ReadPacketU32(request, 18));
            break;
        }
        case 0x85:
            context->cd_audio.Stop();
            success = true;
            break;
        case 0x88:
            success = context->cd_audio.Resume();
            break;
        default:
            break;
    }
    WritePacketU16(request, 3,
                   success ? 0x0100U : 0x8103U);
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedExchange(
            &context->shared_live_telemetry->mscdex_last_status,
            success ? 0x0100L : 0x8103L);
    }
    return true;
}

} // namespace repiu::platform::win32
