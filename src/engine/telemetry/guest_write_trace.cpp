#include "repiu/engine/guest_write_trace.h"

#include "repiu/platform/guest_cpu_context.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace repiu::engine
{
namespace
{

constexpr std::uint32_t kGuestPageMask = 0xFFFFF000U;
constexpr std::uint32_t kPrintLimit = 32U;
constexpr std::size_t kBytePreviewSize = 16U;
constexpr std::uint32_t kTraceRingSize = 64U;

struct GuestWriteTraceRecord
{
    GuestWriteTraceEvent event = GuestWriteTraceEvent::kHle;
    std::uint32_t occurrence = 0U;
    std::uint32_t execution_address = 0U;
    std::uint32_t guest_source = 0U;
    std::uint32_t destination = 0U;
    std::uint32_t byte_count = 0U;
    std::uint8_t bytes[kBytePreviewSize] = {};
    std::uint8_t byte_count_preview = 0U;
    bool has_registers = false;
    std::uint32_t eax = 0U;
    std::uint32_t ebx = 0U;
    std::uint32_t ecx = 0U;
    std::uint32_t edx = 0U;
    std::uint32_t esi = 0U;
    std::uint32_t edi = 0U;
    std::uint32_t esp = 0U;
    std::uint32_t eflags = 0U;
};

std::atomic<std::uint32_t> g_guest_write_trace_count = 0;
GuestWriteTraceRecord g_guest_write_trace_ring[kTraceRingSize];

const char* GuestWriteTraceEventName(GuestWriteTraceEvent event)
{
    switch (event)
    {
        case GuestWriteTraceEvent::kHle:
            return "hle";
        case GuestWriteTraceEvent::kNativeFault:
            return "native-fault";
        case GuestWriteTraceEvent::kNativeComplete:
            return "native-complete";
    }
    return "unknown";
}

}  // namespace

void WriteTraceText(char* out, std::size_t* length, const char* text)
{
    while (*text != '\0')
    {
        out[(*length)++] = *text++;
    }
}

void WriteTraceHex(char* out, std::size_t* length, const std::uint32_t value)
{
    static constexpr char kDigits[] = "0123456789ABCDEF";
    WriteTraceText(out, length, "0x");
    for (int shift = 28; shift >= 0; shift -= 4)
    {
        out[(*length)++] = kDigits[(value >> shift) & 0x0FU];
    }
}

void WriteTraceNamedHex(char* out, std::size_t* length, const char* name,
                        const std::uint32_t value)
{
    WriteTraceText(out, length, name);
    WriteTraceHex(out, length, value);
}

void WriteTraceBytes(char* out, std::size_t* length,
                     const GuestWriteTraceRecord& record)
{
    WriteTraceText(out, length, " bytes=");
    static constexpr char kDigits[] = "0123456789ABCDEF";
    for (std::size_t index = 0; index < record.byte_count_preview; ++index)
    {
        const std::uint8_t value = record.bytes[index];
        out[(*length)++] = kDigits[(value >> 4U) & 0x0FU];
        out[(*length)++] = kDigits[value & 0x0FU];
    }
}

void WriteTraceRecord(char* out, std::size_t* length,
                      const GuestWriteTraceRecord& record)
{
    WriteTraceText(out, length, "[repiu-guest-write-trace-tail] event=");
    WriteTraceText(out, length, GuestWriteTraceEventName(record.event));
    WriteTraceNamedHex(out, length, " n=", record.occurrence);
    WriteTraceNamedHex(out, length, " watch=", GuestWriteTraceAddress());
    WriteTraceNamedHex(out, length, " execution=", record.execution_address);
    WriteTraceNamedHex(out, length, " source=", record.guest_source);
    WriteTraceNamedHex(out, length, " destination=", record.destination);
    WriteTraceNamedHex(out, length, " size=", record.byte_count);
    WriteTraceBytes(out, length, record);
    if (record.has_registers)
    {
        WriteTraceNamedHex(out, length, " eax=", record.eax);
        WriteTraceNamedHex(out, length, " ebx=", record.ebx);
        WriteTraceNamedHex(out, length, " ecx=", record.ecx);
        WriteTraceNamedHex(out, length, " edx=", record.edx);
        WriteTraceNamedHex(out, length, " esi=", record.esi);
        WriteTraceNamedHex(out, length, " edi=", record.edi);
        WriteTraceNamedHex(out, length, " esp=", record.esp);
        WriteTraceNamedHex(out, length, " eflags=", record.eflags);
    }
    out[(*length)++] = '\n';
}

std::uint32_t GuestWriteTraceAddress()
{
    static const std::uint32_t address = []() {
        const char* setting = std::getenv("REPIU_GUEST_WRITE_TRACE");
        if (setting == nullptr || *setting == '\0')
        {
            return 0U;
        }
        char* end = nullptr;
        const unsigned long parsed = std::strtoul(setting, &end, 0);
        if (end == setting || *end != '\0' ||
            parsed > std::numeric_limits<std::uint32_t>::max())
        {
            return 0U;
        }
        return static_cast<std::uint32_t>(parsed);
    }();
    return address;
}

bool GuestWriteTraceMatches(
    std::uint32_t destination,
    std::uint32_t byte_count)
{
    const std::uint32_t watched = GuestWriteTraceAddress();
    if (watched == 0U)
    {
        return false;
    }
    if (byte_count == 0U)
    {
        return destination == watched;
    }
    const std::uint64_t end =
        static_cast<std::uint64_t>(destination) + byte_count;
    return watched >= destination &&
        static_cast<std::uint64_t>(watched) < end;
}

bool GuestWriteTracePageMatches(std::uint32_t guest_address)
{
    const std::uint32_t watched = GuestWriteTraceAddress();
    return watched != 0U &&
        (guest_address & kGuestPageMask) == (watched & kGuestPageMask);
}

void RecordGuestWriteTrace(
    GuestWriteTraceEvent event,
    std::uint32_t execution_address,
    std::uint32_t guest_source,
    std::uint32_t destination,
    std::uint32_t byte_count,
    const void* bytes,
    const repiu::platform::GuestCpuContext* registers)
{
    if (!GuestWriteTraceMatches(destination, byte_count))
    {
        return;
    }
    const std::uint32_t occurrence =
        g_guest_write_trace_count.fetch_add(1U, std::memory_order_relaxed) +
        1U;
    GuestWriteTraceRecord& record =
        g_guest_write_trace_ring[(occurrence - 1U) % kTraceRingSize];
    record.event = event;
    record.occurrence = occurrence;
    record.execution_address = execution_address;
    record.guest_source = guest_source;
    record.destination = destination;
    record.byte_count = byte_count;
    record.byte_count_preview = static_cast<std::uint8_t>(
        bytes == nullptr ? 0U : std::min<std::size_t>(
            byte_count, kBytePreviewSize));
    if (record.byte_count_preview != 0U)
    {
        std::memcpy(record.bytes, bytes, record.byte_count_preview);
    }
    record.has_registers = registers != nullptr;
    if (registers != nullptr)
    {
        record.eax = static_cast<std::uint32_t>(registers->Eax);
        record.ebx = static_cast<std::uint32_t>(registers->Ebx);
        record.ecx = static_cast<std::uint32_t>(registers->Ecx);
        record.edx = static_cast<std::uint32_t>(registers->Edx);
        record.esi = static_cast<std::uint32_t>(registers->Esi);
        record.edi = static_cast<std::uint32_t>(registers->Edi);
        record.esp = static_cast<std::uint32_t>(registers->Esp);
        record.eflags = static_cast<std::uint32_t>(registers->EFlags);
    }
    if (occurrence > kPrintLimit)
    {
        return;
    }

    std::uint8_t byte_preview[kBytePreviewSize] = {};
    const std::size_t preview_size =
        bytes == nullptr ? 0U : std::min<std::size_t>(
            byte_count, kBytePreviewSize);
    if (preview_size != 0U)
    {
        std::memcpy(byte_preview, bytes, preview_size);
    }

    char line[1024] = {};
    int length = std::snprintf(
        line,
        sizeof(line),
        "[repiu-guest-write-trace] event=%s n=%u watch=0x%08X "
        "execution=0x%08X source=0x%08X destination=0x%08X size=%u",
        GuestWriteTraceEventName(event),
        occurrence,
        GuestWriteTraceAddress(),
        execution_address,
        guest_source,
        destination,
        byte_count);
    if (length < 0 || static_cast<std::size_t>(length) >= sizeof(line))
    {
        return;
    }
    std::size_t cursor = static_cast<std::size_t>(length);
    length = std::snprintf(line + cursor, sizeof(line) - cursor, " bytes=");
    if (length < 0 || static_cast<std::size_t>(length) >= sizeof(line) - cursor)
    {
        return;
    }
    cursor += static_cast<std::size_t>(length);
    for (std::size_t index = 0; index < preview_size; ++index)
    {
        length = std::snprintf(
            line + cursor,
            sizeof(line) - cursor,
            "%02X",
            static_cast<unsigned int>(byte_preview[index]));
        if (length < 0 ||
            static_cast<std::size_t>(length) >= sizeof(line) - cursor)
        {
            return;
        }
        cursor += static_cast<std::size_t>(length);
    }
    if (registers != nullptr)
    {
        length = std::snprintf(
            line + cursor,
            sizeof(line) - cursor,
            " eax=0x%08X ebx=0x%08X ecx=0x%08X edx=0x%08X "
            "esi=0x%08X edi=0x%08X esp=0x%08X eflags=0x%08X",
            static_cast<std::uint32_t>(registers->Eax),
            static_cast<std::uint32_t>(registers->Ebx),
            static_cast<std::uint32_t>(registers->Ecx),
            static_cast<std::uint32_t>(registers->Edx),
            static_cast<std::uint32_t>(registers->Esi),
            static_cast<std::uint32_t>(registers->Edi),
            static_cast<std::uint32_t>(registers->Esp),
            static_cast<std::uint32_t>(registers->EFlags));
        if (length < 0 ||
            static_cast<std::size_t>(length) >= sizeof(line) - cursor)
        {
            return;
        }
        cursor += static_cast<std::size_t>(length);
    }
    if (cursor + 2U >= sizeof(line))
    {
        return;
    }
    line[cursor++] = '\n';
    line[cursor] = '\0';
    std::fputs(line, stderr);
}

void DumpGuestWriteTraceTail(const int file_descriptor)
{
    if (GuestWriteTraceAddress() == 0U)
    {
        return;
    }
    const std::uint32_t total =
        g_guest_write_trace_count.load(std::memory_order_relaxed);
    const std::uint32_t first = total > kTraceRingSize
        ? total - kTraceRingSize + 1U
        : 1U;
    for (std::uint32_t occurrence = first; occurrence <= total; ++occurrence)
    {
        const GuestWriteTraceRecord& record =
            g_guest_write_trace_ring[(occurrence - 1U) % kTraceRingSize];
        char line[1024] = {};
        std::size_t length = 0U;
        WriteTraceRecord(line, &length, record);
#if !defined(_WIN32)
        const ssize_t written = write(file_descriptor, line, length);
        (void)written;
#else
        (void)file_descriptor;
#endif
    }
}

}  // namespace repiu::engine
