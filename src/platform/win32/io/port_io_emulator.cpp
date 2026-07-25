#include "port_io_emulator.h"
#include "repiu/platform/win32/execution_trampoline.h"
#include "execution_internal.h"
#include "cpu_emul/guest_memory_access.h"
#include <vector>
#include <sstream>
#include <iomanip>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include "eeprom_93c46.h"
#include "piu10_sound_port.h"

namespace repiu::platform::win32
{

namespace
{
    constexpr std::uint16_t kPortPiuJammaBase = 0x02A0;
    constexpr std::uint16_t kPortPiuJammaEnd = 0x02AF;

    constexpr std::uint16_t kPortPiuEepromWrite = 0x02AC;
    constexpr std::uint16_t kPortPiuEepromRead = 0x02AE;

    constexpr std::uint16_t kPortPiuIn0 = 0x02A8;
    constexpr std::uint16_t kPortPiuSystem = 0x02A9;
    constexpr std::uint16_t kPortPiuIn1 = 0x02AA;

    std::string EepromBackingPath()
    {
        const char* path = std::getenv("REPIU_EEPROM_PATH");
        return path != nullptr && *path != '\0' ? path : "eeprom.dat";
    }

    struct JammaBitName
    {
        std::uint8_t mask;
        const char* name;
    };

    // Active-low inputs (MAME xtom3d.cpp `pumpitup`, IP_ACTIVE_LOW): a bit
    // reads 1 while released and drops to 0 while the panel/button is held.
    constexpr JammaBitName kJammaBitsIn0[] = {
        {0x01, "P1-UpLeft"}, {0x02, "P1-UpRight"}, {0x04, "P1-Center"},
        {0x08, "P1-DownLeft"}, {0x10, "P1-DownRight"},
    };
    constexpr JammaBitName kJammaBitsSystem[] = {
        {0x04, "COIN1"}, {0x40, "SERVICE1"}, {0x80, "TEST"},
    };
    constexpr JammaBitName kJammaBitsIn1[] = {
        {0x01, "P2-UpLeft"}, {0x02, "P2-UpRight"}, {0x04, "P2-Center"},
        {0x08, "P2-DownLeft"}, {0x10, "P2-DownRight"},
    };
}

// The guest polls these ports every frame, so logging each read would flood
// the console exactly like the INT 8 line did. Log edges only: one line per
// press and per release, which is what actually confirms a key reached the
// guest. Called per byte from ReadJammaPort8.
static void LogJammaInputTransition(std::uint16_t port, std::uint8_t value)
{
    const JammaBitName* names = nullptr;
    std::size_t name_count = 0;
    switch (port)
    {
        case kPortPiuIn0:
            names = kJammaBitsIn0;
            name_count = sizeof(kJammaBitsIn0) / sizeof(kJammaBitsIn0[0]);
            break;
        case kPortPiuSystem:
            names = kJammaBitsSystem;
            name_count = sizeof(kJammaBitsSystem) / sizeof(kJammaBitsSystem[0]);
            break;
        case kPortPiuIn1:
            names = kJammaBitsIn1;
            name_count = sizeof(kJammaBitsIn1) / sizeof(kJammaBitsIn1[0]);
            break;
        default:
            return;
    }

    const std::size_t index = port - kPortPiuIn0;

    // Announce the first poll of each port once. Without this a silent log is
    // ambiguous: it cannot distinguish "the key mapping is broken" from "the
    // guest has not started polling this port yet".
    static bool first_poll_logged[3] = {false, false, false};
    if (!first_poll_logged[index])
    {
        first_poll_logged[index] = true;
        std::fprintf(stderr,
                     "[repiu-input] polling started port=0x%04X value=0x%02X\n",
                     static_cast<unsigned>(port),
                     static_cast<unsigned>(value));
    }

    static std::uint8_t previous[3] = {0xFFU, 0xFFU, 0xFFU};
    std::uint8_t& last = previous[index];
    const std::uint8_t changed = static_cast<std::uint8_t>(last ^ value);
    if (changed == 0U)
    {
        return;
    }
    last = value;

    for (std::size_t i = 0; i < name_count; ++i)
    {
        if ((changed & names[i].mask) == 0U)
        {
            continue;
        }
        const bool pressed = (value & names[i].mask) == 0U;
        std::fprintf(stderr,
                     "[repiu-input] %-14s %-8s port=0x%04X value=0x%02X\n",
                     names[i].name,
                     pressed ? "PRESSED" : "released",
                     static_cast<unsigned>(port),
                     static_cast<unsigned>(value));
    }
}

static std::uint8_t ReadJammaPort8(std::uint16_t port)
{
    std::uint8_t value = 0xFF; // Active Low
    auto is_pressed = [](int vk) -> bool {
        return (GetAsyncKeyState(vk) & 0x8000) != 0;
    };

    switch (port)
    {
        case kPortPiuIn0: // IN0: P1
            if (is_pressed('Q')) value &= ~0x01;
            if (is_pressed('E')) value &= ~0x02;
            if (is_pressed('S')) value &= ~0x04;
            if (is_pressed('Z')) value &= ~0x08;
            if (is_pressed('C')) value &= ~0x10;
            break;

        case kPortPiuSystem: // SYSTEM
            if (is_pressed(VK_F5)) value &= ~0x04; // COIN1
            if (is_pressed(VK_F2)) value &= ~0x40; // SERVICE1
            if (is_pressed(VK_F1)) value &= ~0x80; // TEST/CLEAR
            break;

        case kPortPiuIn1: // IN1: P2
            if (is_pressed(VK_HOME)) value &= ~0x01;
            if (is_pressed(VK_PRIOR)) value &= ~0x02; // PgUp
            if (is_pressed(VK_NUMPAD5)) value &= ~0x04;
            if (is_pressed(VK_END)) value &= ~0x08;
            if (is_pressed(VK_NEXT)) value &= ~0x10; // PgDn
            break;
    }

    LogJammaInputTransition(port, value);
    return value;
}

static std::unique_ptr<Eeprom93c46> g_eeprom;

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

bool IsPortIoTraceCandidate(std::uint16_t port,
                            std::uint32_t width,
                            bool is_input)
{
    return !is_input && (width == 1 || width == 2 || width == 4) && port >= kPortPiuJammaBase && port <= kPortPiuJammaEnd;
}

bool HandlePortIoInstruction(CONTEXT* win32_context, ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return false;
    }

    std::uint32_t decode_eip = win32_context->Eip;
    if (IsAotCacheAddress(context, win32_context->Eip))
    {
        if (context->aot_placement != nullptr)
        {
            FindAotGuestAddress(*context->aot_placement, win32_context->Eip, &decode_eip);
        }
    }

    if (!IsGuestRangeReadable(context, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(decode_eip)), 2U))
    {
        return false;
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

    // The YMZ280B window is checked before every other PIU10 register because
    // 0x02A0..0x02A3 sits inside the JAMMA input range below, which would
    // otherwise swallow it and answer 0xFF.
    if (IsPiu10SoundPort(port))
    {
        Ymz280bAudioOut* audio =
            context->ymz_audio_available ? &context->ymz_audio : nullptr;
        if (is_input)
        {
            const std::uint32_t emulated_val =
                ReadPiu10SoundPort(audio, port, width);
            if (width == 1)
            {
                win32_context->Eax =
                    (win32_context->Eax & 0xFFFFFF00U) | emulated_val;
            }
            else if (width == 2)
            {
                win32_context->Eax =
                    (win32_context->Eax & 0xFFFF0000U) | emulated_val;
            }
            else
            {
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
                         audio != nullptr ? "emulated-ymz-read"
                                          : "ymz-unavailable-read");
        }
        else
        {
            WritePiu10SoundPort(audio, port, width, value);
            RecordPortIo(context,
                         static_cast<std::uint32_t>(win32_context->Eip),
                         opcode,
                         port,
                         width,
                         value,
                         false,
                         true,
                         audio != nullptr ? "emulated-ymz-write"
                                          : "ymz-unavailable-write");
        }
        // Never NOP-patch this window. Sound registers are reprogrammed
        // continuously, so latching the first access would mean permanent
        // silence -- the same reason the EEPROM and JAMMA paths advance EIP and
        // re-trap instead of patching.
        win32_context->Eip += instruction_len;
        return true;
    }

    if (is_input)
    {
        if (port == kPortPiuEepromRead)
        {
            if (!g_eeprom)
            {
                g_eeprom = std::make_unique<Eeprom93c46>(
                    EepromBackingPath());
            }
            std::uint32_t emulated_val = 0;
            std::uint8_t do_bit = g_eeprom->ReadData();
            std::uint8_t result_8bit = do_bit | 0xFEU;
            
            if (width == 1)
            {
                emulated_val = result_8bit;
                win32_context->Eax = (win32_context->Eax & 0xFFFFFF00U) | emulated_val;
            }
            else if (width == 2)
            {
                emulated_val = 0xFF00U | result_8bit;
                win32_context->Eax = (win32_context->Eax & 0xFFFF0000U) | emulated_val;
            }
            else
            {
                emulated_val = 0xFFFFFF00U | result_8bit;
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
                         "emulated-eeprom-read");
            win32_context->Eip += instruction_len;
            return true;
        }

        if (port >= kPortPiuJammaBase && port <= kPortPiuJammaEnd)
        {
            std::uint32_t emulated_val = 0;
            for (std::uint32_t i = 0; i < width; ++i)
            {
                emulated_val |= (static_cast<std::uint32_t>(ReadJammaPort8(port + static_cast<std::uint16_t>(i))) << (i * 8));
            }

            if (width == 1)
            {
                win32_context->Eax = (win32_context->Eax & 0xFFFFFF00U) | emulated_val;
            }
            else if (width == 2)
            {
                win32_context->Eax = (win32_context->Eax & 0xFFFF0000U) | emulated_val;
            }
            else
            {
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
            // JAMMA input registers are polled every frame. NOP-patching the
            // guest IN instruction (as the write/init paths do) would latch the
            // first sample forever and never observe later press/release
            // transitions, so advance EIP instead and re-trap on each poll,
            // mirroring the dynamic EEPROM read path above.
            win32_context->Eip += instruction_len;
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

    if (port == kPortPiuEepromWrite)
    {
        if (!g_eeprom)
        {
            g_eeprom = std::make_unique<Eeprom93c46>(EepromBackingPath());
        }
        g_eeprom->WriteControl(static_cast<std::uint8_t>(value & 0xFF));

        RecordPortIo(context,
                     static_cast<std::uint32_t>(win32_context->Eip),
                     opcode,
                     port,
                     width,
                     value,
                     false,
                     true,
                     "emulated-eeprom-write");
        win32_context->Eip += instruction_len;
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
