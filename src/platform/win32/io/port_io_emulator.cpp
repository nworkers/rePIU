#include "port_io_emulator.h"
#include "repiu/platform/win32/execution_trampoline.h"
#include "execution_internal.h"
#include <vector>
#include <sstream>
#include <iomanip>

namespace repiu::platform::win32
{

void RecordPortIo(ThreadContext* context,
                  std::uint32_t address,
                  std::uint32_t opcode,
                  std::uint16_t port,
                  std::uint32_t width,
                  std::uint32_t value,
                  bool is_input,
                  bool handled,
                  const std::string& result)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->port_io.observed_count;
    context->port_io.last_address = address;
    context->port_io.last_opcode = opcode;
    context->port_io.last_port = port;
    context->port_io.last_width = width;
    context->port_io.last_value = value;
    context->port_io.last_is_input = is_input;
    context->port_io.last_handled = handled;
    context->port_io.last_result = result;
    if (context->port_io.trace_stored_count < kWin32PortIoTraceCapacity)
    {
        Win32PortIoTraceEntry& entry =
            context->port_io.trace[context->port_io.trace_stored_count];
        entry.valid = true;
        entry.sequence = context->port_io.observed_count;
        entry.address = address;
        entry.opcode = opcode;
        entry.port = port;
        entry.width = width;
        entry.value = value;
        entry.is_input = is_input;
        entry.handled = handled;
        ++context->port_io.trace_stored_count;
    }
}

bool IsObservedPortInitializationWrite(std::uint16_t port,
                                       std::uint32_t width,
                                       std::uint32_t value)
{
    if (width != 4)
    {
        return false;
    }

    return (port == 0x02AC && value == 0x00000010U) ||
           (port == 0x02A0 && value == 0x00000001U) ||
           (port == 0x02A2 && value == 0x00000000U);
}

bool IsPortIoTraceCandidate(std::uint16_t port,
                            std::uint32_t width,
                            bool is_input)
{
    return !is_input && (width == 1 || width == 2 || width == 4) && port >= 0x02A0 && port <= 0x02AF;
}

bool HandlePortIoInstruction(CONTEXT* win32_context, ThreadContext* context)
{
    std::uint32_t decode_eip = win32_context->Eip;
    if (IsAotCacheAddress(context, win32_context->Eip))
    {
        if (context->aot_placement != nullptr)
        {
            FindAotGuestAddress(*context->aot_placement, win32_context->Eip, &decode_eip);
        }
    }

    const std::uint8_t* instruction = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(decode_eip));
    
    bool has_prefix = (instruction[0] == 0x66);
    std::uint8_t opcode_byte = has_prefix ? instruction[1] : instruction[0];
    std::uint32_t instruction_len = has_prefix ? 2 : 1;

    if (opcode_byte != 0xEC && opcode_byte != 0xED &&
        opcode_byte != 0xEE && opcode_byte != 0xEF)
    {
        return false;
    }

    const std::uint16_t port = static_cast<std::uint16_t>(
        win32_context->Edx & 0xFFFFU);
    const bool is_input = (opcode_byte == 0xEC || opcode_byte == 0xED);
    
    std::uint32_t width = 1;
    if (opcode_byte == 0xED || opcode_byte == 0xEF)
    {
        width = has_prefix ? 2 : 4;
    }

    const std::uint32_t opcode = has_prefix ? (0x6600U | opcode_byte) : opcode_byte;
    std::uint32_t value = 0;
    if (!is_input)
    {
        if (width == 1)
        {
            value = win32_context->Eax & 0xFFU;
        }
        else if (width == 2)
        {
            value = win32_context->Eax & 0xFFFFU;
        }
        else
        {
            value = win32_context->Eax;
        }
    }

    auto apply_nop_patch = [context, decode_eip, instruction_len] () {
        std::vector<std::uint8_t> nop_buffer(instruction_len, 0x90);
        WriteGuestBytes(context, reinterpret_cast<void*>(static_cast<std::uintptr_t>(decode_eip)), nop_buffer.data(), instruction_len);
    };

    if (is_input)
    {
        if (port >= 0x02A0 && port <= 0x02AF)
        {
            std::uint32_t emulated_val = 0;
            if (width == 1)
            {
                emulated_val = 0xFFU;
                win32_context->Eax = (win32_context->Eax & 0xFFFFFF00U) | emulated_val;
            }
            else if (width == 2)
            {
                emulated_val = 0xFFFFU;
                win32_context->Eax = (win32_context->Eax & 0xFFFF0000U) | emulated_val;
            }
            else
            {
                emulated_val = 0xFFFFFFFFU;
                win32_context->Eax = emulated_val;
            }

            RecordPortIo(context,
                         static_cast<std::uint32_t>(win32_context->Eip),
                         opcode,
                         port,
                         width,
                         emulated_val,
                         true,
                         true,
                         "emulated-jamma");
            apply_nop_patch();
            return true;
        }

        RecordPortIo(context,
                     static_cast<std::uint32_t>(win32_context->Eip),
                     opcode,
                     port,
                     width,
                     value,
                     true,
                     false,
                     "unsupported-in");
        std::ostringstream stream;
        stream << "unsupported port I/O IN EAX,DX port=0x"
               << std::hex << static_cast<unsigned>(port);
        context->hle_message = stream.str();
        return false;
    }

    if (IsObservedPortInitializationWrite(port, width, value))
    {
        RecordPortIo(context,
                     static_cast<std::uint32_t>(win32_context->Eip),
                     opcode,
                     port,
                     width,
                     value,
                     false,
                     true,
                     "ignored");
        apply_nop_patch();
        return true;
    }

    if (IsPortIoTraceCandidate(port, width, false))
    {
        if (context->port_io.observed_count >= kWin32DeferredPortIoLimit)
        {
            context->port_io.trace_limit_reached = true;
            RecordPortIo(context,
                         static_cast<std::uint32_t>(win32_context->Eip),
                         opcode,
                         port,
                         width,
                         value,
                         false,
                         true,
                         "deferred-limit");
            apply_nop_patch();
            return true;
        }

        RecordPortIo(context,
                     static_cast<std::uint32_t>(win32_context->Eip),
                     opcode,
                     port,
                     width,
                     value,
                     false,
                     true,
                     "deferred-ignored");
        apply_nop_patch();
        return true;
    }

    RecordPortIo(context,
                 static_cast<std::uint32_t>(win32_context->Eip),
                 opcode,
                 port,
                 width,
                 value,
                 false,
                 true,
                 "unsupported-ignored");
    apply_nop_patch();
    return true;
}

} // namespace repiu::platform::win32
