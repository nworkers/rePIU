#include "repiu/engine/mscdex_command_trace.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace repiu::engine
{

namespace
{

const char* CommandName(std::uint8_t command)
{
    switch (command)
    {
        case 0x03: return "ioctl-in";
        case 0x0C: return "ioctl-out";
        case 0x83: return "seek";
        case 0x84: return "play";
        case 0x85: return "stop";
        case 0x88: return "resume";
        default: return "other";
    }
}

}  // namespace

bool MscdexCommandTraceEnabled()
{
    static const bool enabled = [] {
        const char* value = std::getenv("REPIU_MSCDEX_COMMAND_TRACE");
        return value != nullptr && std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

void RecordMscdexCommand(MscdexCommandTrace* trace,
                         const MscdexCommandEntry& entry)
{
    if (trace == nullptr)
    {
        return;
    }
    ++trace->total_commands;
    if (trace->entry_count >= kMscdexCommandTraceCapacity)
    {
        // The interesting window is the storm that precedes the stall, and that
        // is at the end, so a full ring keeps counting rather than wrapping and
        // losing the ordering the trace exists to show.
        ++trace->overflow_count;
        return;
    }
    trace->entries[trace->entry_count] = entry;
    ++trace->entry_count;
}

bool WriteMscdexCommandTraceDump(const MscdexCommandTrace& trace,
                                 std::uint32_t* written_entry_count)
{
    if (written_entry_count != nullptr)
    {
        *written_entry_count = 0U;
    }
    if (!trace.enabled || trace.entry_count == 0U)
    {
        return false;
    }

    std::filesystem::path path = "build/mscdex_command_trace.txt";
    if (const char* value = std::getenv("REPIU_MSCDEX_COMMAND_TRACE_DUMP"))
    {
        if (std::strcmp(value, "1") != 0 && value[0] != '\0')
        {
            path = value;
        }
    }
    std::error_code error;
    if (path.has_parent_path())
    {
        std::filesystem::create_directories(path.parent_path(), error);
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        return false;
    }

    out << "# Task 422 MSCDEX command trace\n"
        << "# commands=" << trace.total_commands
        << " entries=" << trace.entry_count
        << " overflow=" << trace.overflow_count << "\n"
        << "# wall_ms command name ioctl_subfunction address_mode "
           "argument_lba argument_length success current_lba\n";
    for (std::uint32_t index = 0; index < trace.entry_count; ++index)
    {
        const MscdexCommandEntry& entry = trace.entries[index];
        out << entry.wall_milliseconds << ' '
            << static_cast<unsigned>(entry.command) << ' '
            << CommandName(entry.command) << ' '
            << static_cast<unsigned>(entry.ioctl_subfunction) << ' '
            << static_cast<unsigned>(entry.address_mode) << ' '
            << entry.argument_lba << ' ' << entry.argument_length << ' '
            << (entry.success ? 1 : 0) << ' ' << entry.current_lba << '\n';
    }
    out.flush();
    if (!out.good())
    {
        return false;
    }
    if (written_entry_count != nullptr)
    {
        *written_entry_count = trace.entry_count;
    }
    return true;
}

}  // namespace repiu::engine
