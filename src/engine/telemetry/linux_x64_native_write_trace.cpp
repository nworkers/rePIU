#include "repiu/platform/linux_x64_aot_dispatch.h"

#if !defined(_WIN32) && defined(__x86_64__)

#include "../cpu_emul/guest_memory_access.h"
#include "repiu/engine/glide_lfb_native_store_census.h"
#include "repiu/engine/guest_write_trace.h"

#include <Zydis.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{

constexpr std::uint32_t kVerbosePrintLimit = 128U;
std::atomic<std::uint32_t> g_native_observer_call_count = 0U;

bool NativeMemoryWriteTraceVerboseEnabled()
{
    static const bool enabled = [] {
        const char* const value =
            std::getenv("REPIU_LINUX_X64_MEMORY_WRITE_TRACE");
        return value != nullptr &&
            (std::strcmp(value, "verbose") == 0 ||
             std::strcmp(value, "2") == 0);
    }();
    return enabled;
}

bool ReadGuestRegister32(
    const repiu::platform::LinuxX64GuestRegisterState& guest,
    const ZydisRegister register_id,
    std::uint32_t* value)
{
    if (value == nullptr)
    {
        return false;
    }
    const ZydisRegister enclosing = ZydisRegisterGetLargestEnclosing(
        ZYDIS_MACHINE_MODE_LEGACY_32, register_id);
    switch (enclosing)
    {
        case ZYDIS_REGISTER_EAX: *value = guest.eax; return true;
        case ZYDIS_REGISTER_ECX: *value = guest.ecx; return true;
        case ZYDIS_REGISTER_EDX: *value = guest.edx; return true;
        case ZYDIS_REGISTER_EBX: *value = guest.ebx; return true;
        case ZYDIS_REGISTER_ESP: *value = guest.esp; return true;
        case ZYDIS_REGISTER_EBP: *value = guest.ebp; return true;
        case ZYDIS_REGISTER_ESI: *value = guest.esi; return true;
        case ZYDIS_REGISTER_EDI: *value = guest.edi; return true;
        default: return false;
    }
}

bool DecodeNativeMemoryWrite(
    const repiu::platform::LinuxX64AotDispatchFrame& frame,
    std::uint32_t* destination,
    std::uint32_t* byte_count)
{
    if (destination == nullptr || byte_count == nullptr)
    {
        return false;
    }
    auto* const context = reinterpret_cast<repiu::engine::ThreadContext*>(
        frame.context);
    if (context == nullptr ||
        !repiu::engine::IsGuestRangeReadable(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(frame.guest.eip)),
            15U))
    {
        return false;
    }

    const auto* const bytes = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(frame.guest.eip));
    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(
            &decoder, ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32)))
    {
        return false;
    }
    ZydisDecodedInstruction instruction{};
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
            &decoder, bytes, 15U, &instruction, operands)))
    {
        return false;
    }
    if ((instruction.attributes & ZYDIS_ATTRIB_HAS_SEGMENT) != 0U)
    {
        return false;
    }

    for (std::uint8_t index = 0U;
         index < instruction.operand_count_visible; ++index)
    {
        const ZydisDecodedOperand& operand = operands[index];
        if (operand.type != ZYDIS_OPERAND_TYPE_MEMORY ||
            (operand.actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) == 0U)
        {
            continue;
        }
        std::uint32_t base = 0U;
        std::uint32_t index_value = 0U;
        if ((operand.mem.base != ZYDIS_REGISTER_NONE &&
             !ReadGuestRegister32(
                 frame.guest, operand.mem.base, &base)) ||
            (operand.mem.index != ZYDIS_REGISTER_NONE &&
             !ReadGuestRegister32(
                 frame.guest, operand.mem.index, &index_value)))
        {
            return false;
        }
        const std::uint32_t displacement = operand.mem.disp.has_displacement
            ? static_cast<std::uint32_t>(operand.mem.disp.value)
            : 0U;
        const std::uint32_t computed = base +
            index_value * static_cast<std::uint32_t>(operand.mem.scale) +
            displacement;
        const std::uint32_t width =
            (static_cast<std::uint32_t>(operand.size) + 7U) / 8U;
        if (width == 0U)
        {
            return false;
        }
        *destination = computed;
        *byte_count = width;
        return true;
    }
    return false;
}

}  // namespace

extern "C" void RepiuLinuxX64NativeMemoryWriteTrace(
    repiu::platform::LinuxX64AotDispatchFrame* const frame)
{
    const bool legacy_trace = repiu::engine::GuestWriteTraceNativeObserverEnabled();
    const bool lfb_census = repiu::engine::GlideLfbNativeStoreCensusEnabled();
    static const bool lfb_source_census =
        repiu::engine::GlideLfbNativeStoreSourceCensusEnabled();
    if (frame == nullptr || (!legacy_trace && !lfb_census))
    {
        return;
    }
    const std::uint32_t observer_call =
        g_native_observer_call_count.fetch_add(1U, std::memory_order_relaxed) +
        1U;
    auto* const context = reinterpret_cast<repiu::engine::ThreadContext*>(
        frame->context);
    const bool eip_readable = context != nullptr &&
        repiu::engine::IsGuestRangeReadable(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(frame->guest.eip)),
            15U);
    std::uint32_t decoded_destination = 0U;
    std::uint32_t decoded_byte_count = 0U;
    const bool decoded = DecodeNativeMemoryWrite(
        *frame, &decoded_destination, &decoded_byte_count);
    if (lfb_census && context != nullptr)
    {
        repiu::engine::RecordGlideLfbNativeStoreCensus(
            &context->glide_lfb_native_store_census, decoded,
            decoded_destination, decoded_byte_count);
        if (lfb_source_census)
        {
            repiu::engine::RecordGlideLfbNativeStoreCensusSource(
                &context->glide_lfb_native_store_census, decoded,
                frame->guest.eip, decoded_destination, decoded_byte_count);
        }
    }
    const bool matches = decoded && repiu::engine::GuestWriteTraceMatches(
        decoded_destination, decoded_byte_count);
    if (NativeMemoryWriteTraceVerboseEnabled() &&
        observer_call <= kVerbosePrintLimit)
    {
        std::fprintf(
            stderr,
            "[repiu-native-write-observer] n=%u guest=0x%08X decoded=%u "
            "destination=0x%08X size=%u matches=%u readable=%u "
            "runtime=0x%08X+0x%08X\n",
            observer_call,
            frame->guest.eip,
            decoded ? 1U : 0U,
            decoded_destination,
            decoded_byte_count,
            matches ? 1U : 0U,
            eip_readable ? 1U : 0U,
            context != nullptr ? context->runtime_base : 0U,
            context != nullptr ? context->runtime_size : 0U);
    }
    if (matches && eip_readable)
    {
        const auto* const instruction_bytes = reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(frame->guest.eip));
        std::fprintf(
            stderr,
            "[repiu-native-write-match] guest=0x%08X bytes=",
            frame->guest.eip);
        for (std::size_t index = 0U; index < 8U; ++index)
        {
            std::fprintf(stderr, "%02X", instruction_bytes[index]);
        }
        std::fprintf(
            stderr,
            " destination=0x%08X size=%u eax=0x%08X ebx=0x%08X "
            "ecx=0x%08X edx=0x%08X esi=0x%08X edi=0x%08X "
            "ebp=0x%08X esp=0x%08X\n",
            decoded_destination,
            decoded_byte_count,
            frame->guest.eax,
            frame->guest.ebx,
            frame->guest.ecx,
            frame->guest.edx,
            frame->guest.esi,
            frame->guest.edi,
            frame->guest.ebp,
            frame->guest.esp);
    }
    if (!matches)
    {
        return;
    }
    const std::uint32_t destination = decoded_destination;
    const std::uint32_t byte_count = decoded_byte_count;
    const void* const source = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(destination));
    if (context == nullptr ||
        !repiu::engine::IsGuestRangeReadable(context, source, byte_count))
    {
        return;
    }
    repiu::engine::RecordGuestWriteTrace(
        repiu::engine::GuestWriteTraceEvent::kNativeAot,
        frame->guest.eip,
        frame->guest.eip,
        destination,
        byte_count,
        source,
        nullptr);
}

#endif  // !defined(_WIN32) && defined(__x86_64__)
