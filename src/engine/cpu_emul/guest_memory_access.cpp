#include "guest_memory_access.h"
#include "execution_internal.h"

#include "repiu/platform/host_error_stream.h"
#include "repiu/platform/virtual_memory.h"
#include "repiu/engine/guest_write_trace.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>

namespace repiu::engine
{

bool IsGuestRangeReadable(ThreadContext* context,
                          const void* source,
                          std::uint32_t byte_count)
{
    if (context == nullptr || source == nullptr || byte_count == 0)
    {
        return false;
    }

    const std::uintptr_t base =
        static_cast<std::uintptr_t>(context->runtime_base);
    const std::uintptr_t size =
        static_cast<std::uintptr_t>(context->runtime_size);
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(source);
    const std::uintptr_t end = address + byte_count;
    return address >= base && end >= address && end <= base + size;
}

bool IsGuestRangeWritable(ThreadContext* context,
                          void* destination,
                          std::uint32_t byte_count)
{
    return IsGuestRangeReadable(context, destination, byte_count);
}

bool WriteGuestUInt16(ThreadContext* context,
                      void* destination,
                      std::uint16_t value)
{
    if (!IsGuestRangeWritable(context, destination, sizeof(value)))
    {
        return false;
    }

    repiu::platform::MemoryProtection previous_protect =
        repiu::platform::MemoryProtection::kOther;
    if (!repiu::platform::ProtectMemory(
            destination,
            sizeof(value),
            repiu::platform::MemoryProtection::kExecuteReadWrite,
            &previous_protect))
    {
        std::ostringstream stream;
        // The host's error number is no longer carried, so the address takes
        // its place -- which is the more useful of the two when a guest store
        // into protected code is what went wrong.
        stream << "protecting guest memory failed for guest segment store at 0x"
               << std::hex
               << reinterpret_cast<std::uintptr_t>(destination);
        context->hle_message = stream.str();
        return false;
    }

    std::memcpy(destination, &value, sizeof(value));

    // Putting back exactly what was there is the whole reason the call above
    // reports it.
    if (!repiu::platform::ProtectMemory(destination,
                                        sizeof(value),
                                        previous_protect,
                                        nullptr))
    {
        return false;
    }
    const bool noted = NoteSuccessfulAotGuestWrite(
        context,
        static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(destination)),
        sizeof(value));
    RecordGuestWriteTrace(
        GuestWriteTraceEvent::kHle,
        0U,
        0U,
        static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(destination)),
        sizeof(value),
        &value);
    return noted;
}

bool WriteGuestUInt8(ThreadContext* context,
                     void* destination,
                     std::uint8_t value)
{
    if (!IsGuestRangeWritable(context, destination, sizeof(value)))
    {
        return false;
    }

    repiu::platform::MemoryProtection previous_protect =
        repiu::platform::MemoryProtection::kOther;
    if (!repiu::platform::ProtectMemory(
            destination,
            sizeof(value),
            repiu::platform::MemoryProtection::kExecuteReadWrite,
            &previous_protect))
    {
        std::ostringstream stream;
        // The host's error number is no longer carried, so the address takes
        // its place -- which is the more useful of the two when a guest store
        // into protected code is what went wrong.
        stream << "protecting guest memory failed for guest byte store at 0x"
               << std::hex
               << reinterpret_cast<std::uintptr_t>(destination);
        context->hle_message = stream.str();
        return false;
    }

    std::memcpy(destination, &value, sizeof(value));

    // Putting back exactly what was there is the whole reason the call above
    // reports it.
    if (!repiu::platform::ProtectMemory(destination,
                                        sizeof(value),
                                        previous_protect,
                                        nullptr))
    {
        return false;
    }
    const bool noted = NoteSuccessfulAotGuestWrite(
        context,
        static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(destination)),
        sizeof(value));
    RecordGuestWriteTrace(
        GuestWriteTraceEvent::kHle,
        0U,
        0U,
        static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(destination)),
        sizeof(value),
        &value);
    return noted;
}

bool WriteGuestUInt32(ThreadContext* context,
                      void* destination,
                      std::uint32_t value)
{
    if (!IsGuestRangeWritable(context, destination, sizeof(value)))
    {
        return false;
    }

    repiu::platform::MemoryProtection previous_protect =
        repiu::platform::MemoryProtection::kOther;
    if (!repiu::platform::ProtectMemory(
            destination,
            sizeof(value),
            repiu::platform::MemoryProtection::kExecuteReadWrite,
            &previous_protect))
    {
        std::ostringstream stream;
        // The host's error number is no longer carried, so the address takes
        // its place -- which is the more useful of the two when a guest store
        // into protected code is what went wrong.
        stream << "protecting guest memory failed for guest dword store at 0x"
               << std::hex
               << reinterpret_cast<std::uintptr_t>(destination);
        context->hle_message = stream.str();
        return false;
    }

    std::memcpy(destination, &value, sizeof(value));

    // Putting back exactly what was there is the whole reason the call above
    // reports it.
    if (!repiu::platform::ProtectMemory(destination,
                                        sizeof(value),
                                        previous_protect,
                                        nullptr))
    {
        return false;
    }
    const bool noted = NoteSuccessfulAotGuestWrite(
        context,
        static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(destination)),
        sizeof(value));
    RecordGuestWriteTrace(
        GuestWriteTraceEvent::kHle,
        0U,
        0U,
        static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(destination)),
        sizeof(value),
        &value);
    return noted;
}


bool ReadGuestUInt8(ThreadContext* context,
                    const void* source,
                    std::uint8_t* value)
{
    if (value == nullptr ||
        !IsGuestRangeReadable(context, source, sizeof(*value)))
    {
        return false;
    }

    std::memcpy(value, source, sizeof(*value));
    return true;
}

bool ReadGuestUInt32(ThreadContext* context,
                     const void* source,
                     std::uint32_t* value)
{
    if (value == nullptr ||
        !IsGuestRangeReadable(context, source, sizeof(*value)))
    {
        return false;
    }

    std::memcpy(value, source, sizeof(*value));
    return true;
}

void WriteShadowMemory(ThreadContext* context,
                       std::uint32_t destination,
                       std::uint32_t value,
                       std::uint32_t byte_count)
{
    if (context == nullptr || byte_count == 0)
    {
        return;
    }

    ++context->shadow_memory_write_count;
    const ShadowWriteProvenance provenance = {
        context->shadow_write_provenance_count + 1,
        context->last_memory_store_address,
        context->last_memory_store_opcode,
        destination,
        value,
        byte_count,
    };
    const std::uint32_t provenance_slot =
        context->shadow_write_provenance_count %
        kShadowWriteProvenanceCapacity;
    context->shadow_write_provenance[provenance_slot] = provenance;
    ++context->shadow_write_provenance_count;
    for (std::uint32_t index = 0; index < byte_count; ++index)
    {
        const std::uint32_t address = destination + index;
        context->shadow_memory[address] =
            static_cast<std::uint8_t>((value >> (index * 8)) & 0xFFU);
        if (!context->shadow_memory_range_valid)
        {
            context->shadow_memory_range_valid = true;
            context->shadow_memory_min_address = address;
            context->shadow_memory_max_address = address;
        }
        else
        {
            context->shadow_memory_min_address =
                std::min(context->shadow_memory_min_address, address);
            context->shadow_memory_max_address =
                std::max(context->shadow_memory_max_address, address);
        }
    }
}

bool ReadShadowUInt32(ThreadContext* context,
                      std::uint32_t source,
                      std::uint32_t* value)
{
    if (context == nullptr || value == nullptr)
    {
        return false;
    }

    std::uint32_t result = 0;
    for (std::uint32_t index = 0; index < 4; ++index)
    {
        const auto found = context->shadow_memory.find(source + index);
        if (found != context->shadow_memory.end())
        {
            result |=
                static_cast<std::uint32_t>(found->second) << (index * 8);
            continue;
        }

        const std::uint32_t address = source + index;
        if (!context->shadow_zero_payload_valid ||
            address < context->shadow_zero_payload_begin ||
            address >= context->shadow_zero_payload_end)
        {
            return false;
        }
    }

    *value = result;
    ++context->shadow_memory_read_hit_count;
    return true;
}

bool ReadShadowUInt8(ThreadContext* context,
                     std::uint32_t source,
                     std::uint8_t* value)
{
    if (context == nullptr || value == nullptr)
    {
        return false;
    }

    const auto found = context->shadow_memory.find(source);
    if (found != context->shadow_memory.end())
    {
        *value = found->second;
    }
    else if (context->shadow_zero_payload_valid &&
             source >= context->shadow_zero_payload_begin &&
             source < context->shadow_zero_payload_end)
    {
        *value = 0;
    }
    else
    {
        return false;
    }
    ++context->shadow_memory_read_hit_count;
    return true;
}

bool AppendConsoleOutput(ThreadContext* context,
                         const void* source,
                         std::uint32_t byte_count,
                         bool stderr_stream)
{
    if (context == nullptr || source == nullptr || byte_count == 0)
    {
        return false;
    }

    if (!IsGuestRangeReadable(context, source, byte_count))
    {
        context->hle_message = "DOS console output buffer is outside runtime memory";
        return false;
    }

    char* output = stderr_stream
        ? context->hle_stderr_output
        : context->hle_stdout_output;
    std::uint32_t* output_size = stderr_stream
        ? &context->hle_stderr_output_size
        : &context->hle_stdout_output_size;
    const std::uint32_t capacity = stderr_stream
        ? sizeof(context->hle_stderr_output)
        : sizeof(context->hle_stdout_output);
    const std::uint32_t available = capacity - *output_size;
    const std::uint32_t copied = std::min(byte_count, available);
    if (copied == 0)
    {
        return false;
    }

    std::memcpy(
        output + *output_size,
        source,
        copied);
    *output_size += copied;
    // Task 523: echo it as the guest writes it.
    //
    // The buffer above is printed by the loader's summary, which a run that
    // ends any other way never reaches -- and a guest that is failing prints
    // exactly then. Its own message is the best diagnostic available and it
    // was being swallowed by the very failure it describes.
    if (std::getenv("REPIU_DOS_INT_TRACE") != nullptr)
    {
        repiu::platform::WriteHostErrorStream("[repiu-guest-out] ", 18U);
        repiu::platform::WriteHostErrorStream(
            reinterpret_cast<const char*>(source), copied);
        repiu::platform::WriteHostErrorStream("\n", 1U);
    }
    return true;
}

bool ReadGuestAsciz(ThreadContext* context,
                    std::uint32_t address,
                    std::uint32_t max_length,
                    std::string* value)
{
    if (context == nullptr || value == nullptr || max_length == 0)
    {
        return false;
    }

    value->clear();
    const char* text = reinterpret_cast<const char*>(
        static_cast<std::uintptr_t>(address));
    for (std::uint32_t index = 0; index < max_length; ++index)
    {
        if (!IsGuestRangeReadable(context, text + index, 1))
        {
            return false;
        }

        const char ch = text[index];
        if (ch == '\0')
        {
            return true;
        }
        value->push_back(ch);
    }

    return false;
}

} // namespace repiu::engine
