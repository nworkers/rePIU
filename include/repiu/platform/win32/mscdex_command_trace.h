#pragma once

#include <cstdint>

namespace repiu::platform::win32

{

// Task 422. Task 421's position census showed the guest issuing about sixty-five
// state-changing CD commands per second with nothing playing, right before the
// run stalls at a fixed guest EIP. The existing telemetry keeps only the *last*
// command, so a storm is indistinguishable from a single call. This ring keeps
// the sequence instead, which is what says which request the guest is retrying
// and what we answered.
// See docs/design/20260805-422-mscdex-command-trace.md.

constexpr std::uint32_t kWin32MscdexCommandTraceCapacity = 8192U;

struct Win32MscdexCommandEntry
{
    std::uint32_t wall_milliseconds = 0;
    // MSCDEX request header command byte (0x03 IOCTL in, 0x0C IOCTL out,
    // 0x83 seek, 0x84 play, 0x85 stop, 0x88 resume).
    std::uint8_t command = 0;
    // For the two IOCTL commands, the control block's subfunction; 0xFF when
    // the command carries none.
    std::uint8_t ioctl_subfunction = 0xFFU;
    // Address mode byte for seek and play, meaningless elsewhere.
    std::uint8_t address_mode = 0;
    bool success = false;
    // Seek and play arguments after conversion to a logical LBA, so the trace
    // reads in the same units as the position census.
    std::uint32_t argument_lba = 0;
    std::uint32_t argument_length = 0;
    // What the position read at the moment the command was served, so a command
    // storm can be aligned against the music without joining two files by time.
    std::uint32_t current_lba = 0;
};

struct Win32MscdexCommandTrace
{
    bool enabled = false;
    // `GetTickCount` at construction. Entries store time relative to it so the
    // trace shares a zero with the position census and the two files can be
    // read side by side without aligning them by hand.
    std::uint32_t base_tick = 0;
    std::uint32_t total_commands = 0;
    std::uint32_t overflow_count = 0;
    std::uint32_t entry_count = 0;
    Win32MscdexCommandEntry entries[kWin32MscdexCommandTraceCapacity];
};

bool MscdexCommandTraceEnabled();

void RecordMscdexCommand(Win32MscdexCommandTrace* trace,
                         const Win32MscdexCommandEntry& entry);

// Writes the sequence to `build/mscdex_command_trace.txt`, or to the path in
// `REPIU_MSCDEX_COMMAND_TRACE_DUMP`. Called on teardown.
bool WriteMscdexCommandTraceDump(const Win32MscdexCommandTrace& trace,
                                 std::uint32_t* written_entry_count);

}  // namespace repiu::platform::win32
