#include "dos_int21_services.h"
#include "aot/aot_runtime_dispatch.h"
#include "execution_internal.h"
#include "guest_memory_access.h"
#include "dpmi_mscdex_services.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace repiu::platform::win32
{

void RecordDosChangeDirectory(ThreadContext* context,
                              const std::string& guest_path,
                              const repiu::hle::DosResolvedPath& resolved)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_chdir_count;
    context->last_dos_chdir_guest_path = guest_path;
    context->last_dos_chdir_host_path = resolved.host_path.string();
    context->last_dos_chdir_virtual_path = resolved.dos_path;
    context->last_dos_chdir_success =
        resolved.result == repiu::hle::DosPathResult::kOk;
    context->last_dos_chdir_error =
        repiu::hle::DosPathResultToErrorCode(resolved.result);
}

void RecordDosPathTrace(ThreadContext* context,
                        const char* service,
                        const std::string& guest_path,
                        const std::string& virtual_path,
                        const std::string& host_path,
                        bool success,
                        std::uint16_t dos_error,
                        std::uint8_t drive,
                        std::uint8_t access_mode)
{
    if (context == nullptr || service == nullptr)
    {
        return;
    }

    Win32DosPathObservation& observation = context->dos_path;
    const std::uint32_t sequence = observation.observed_count + 1;
    const std::uint32_t slot =
        (sequence - 1) % kWin32DosPathTraceCapacity;
    Win32DosPathTraceEntry& entry = observation.trace[slot];
    entry.valid = true;
    entry.sequence = sequence;
    entry.service = service;
    entry.guest_path = guest_path;
    entry.virtual_path = virtual_path;
    entry.host_path = host_path;
    entry.result = success ? "success" : "failure";
    entry.dos_error = dos_error;
    entry.drive = drive;
    entry.access_mode = access_mode;
    observation.observed_count = sequence;
    if (observation.trace_stored_count < kWin32DosPathTraceCapacity)
    {
        ++observation.trace_stored_count;
    }
    else
    {
        observation.trace_limit_reached = true;
    }
}

std::string BuildCurrentDosVirtualPath(
    const repiu::hle::DosVirtualFileSystemState& state)
{
    const std::string current_directory =
        repiu::hle::GetDosCurrentDirectory(state);
    if (current_directory.empty())
    {
        return "\\";
    }

    return "\\" + current_directory;
}

std::string BuildCurrentDosHostPath(
    const repiu::hle::DosVirtualFileSystemState& state)
{
    std::filesystem::path host_path = state.host_root;
    for (const std::string& component : state.current_components)
    {
        host_path /= component;
    }
    return host_path.lexically_normal().string();
}

bool HandleDosChangeDirectory(CONTEXT* win32_context, ThreadContext* context)
{
    std::string guest_path;
    repiu::hle::DosResolvedPath resolved;
    if (!ReadGuestAsciz(context,
                        static_cast<std::uint32_t>(win32_context->Edx),
                        260,
                        &guest_path))
    {
        resolved.result = repiu::hle::DosPathResult::kPathNotFound;
        resolved.guest_path = "";
        resolved.message = "DOS chdir path is outside runtime memory";
        RecordDosChangeDirectory(context, guest_path, resolved);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | 0x0003U;
        win32_context->EFlags |= 1U;
        return true;
    }

    if (!repiu::hle::ChangeDosCurrentDirectory(
            &context->dos_file_system,
            guest_path,
            &resolved))
    {
        return false;
    }

    RecordDosChangeDirectory(context, guest_path, resolved);
    RecordDosPathTrace(context,
                       "chdir",
                       guest_path,
                       resolved.dos_path,
                       resolved.host_path.string(),
                       resolved.result == repiu::hle::DosPathResult::kOk,
                       repiu::hle::DosPathResultToErrorCode(
                           resolved.result),
                       0,
                       0);
    if (resolved.result == repiu::hle::DosPathResult::kOk)
    {
        win32_context->EFlags &= ~1U;
    }
    else
    {
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) |
            repiu::hle::DosPathResultToErrorCode(resolved.result);
        win32_context->EFlags |= 1U;
    }
    return true;
}

bool HandleDosGetCurrentDirectory(CONTEXT* win32_context,
                                  ThreadContext* context)
{
    const std::string current_directory =
        repiu::hle::GetDosCurrentDirectory(context->dos_file_system);
    const std::uint8_t drive = static_cast<std::uint8_t>(
        win32_context->Edx & 0xFFU);
    ++context->handled_dos_getcwd_count;
    context->last_dos_getcwd_drive = drive;
    context->last_dos_getcwd_path = current_directory;
    if (current_directory.size() >= 64)
    {
        context->last_dos_getcwd_success = false;
        context->last_dos_getcwd_error = 0x0005U;
        RecordDosPathTrace(context,
                           "getcwd",
                           "",
                           BuildCurrentDosVirtualPath(
                               context->dos_file_system),
                           BuildCurrentDosHostPath(context->dos_file_system),
                           false,
                           context->last_dos_getcwd_error,
                           drive,
                           0);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | 0x0005U;
        win32_context->EFlags |= 1U;
        return true;
    }

    std::string asciz = current_directory;
    asciz.push_back('\0');
    void* destination = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(win32_context->Esi));
    if (!WriteGuestBytes(context,
                         destination,
                         asciz.data(),
                         asciz.size()))
    {
        context->last_dos_getcwd_success = false;
        context->last_dos_getcwd_error = 0x0003U;
        RecordDosPathTrace(context,
                           "getcwd",
                           "",
                           BuildCurrentDosVirtualPath(
                               context->dos_file_system),
                           BuildCurrentDosHostPath(context->dos_file_system),
                           false,
                           context->last_dos_getcwd_error,
                           drive,
                           0);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | 0x0003U;
        win32_context->EFlags |= 1U;
        return true;
    }

    context->last_dos_getcwd_success = true;
    context->last_dos_getcwd_error = 0;
    RecordDosPathTrace(context,
                       "getcwd",
                       "",
                       BuildCurrentDosVirtualPath(context->dos_file_system),
                       BuildCurrentDosHostPath(context->dos_file_system),
                       true,
                       0,
                       drive,
                       0);
    win32_context->EFlags &= ~1U;
    return true;
}

void HandleDosGetCurrentDrive(CONTEXT* win32_context, ThreadContext* context)
{
    constexpr std::uint8_t kDefaultDriveC = 2;
    ++context->handled_dos_getdrive_count;
    context->last_dos_getdrive_value = kDefaultDriveC;
    RecordDosPathTrace(context,
                       "getdrive",
                       "",
                       "",
                       "",
                       true,
                       0,
                       kDefaultDriveC,
                       0);
    win32_context->Eax =
        (win32_context->Eax & 0xFFFFFF00U) | kDefaultDriveC;
    win32_context->EFlags &= ~1U;
}

void RecordDosOpen(ThreadContext* context,
                   const std::string& guest_path,
                   const repiu::hle::DosResolvedPath& resolved,
                   std::uint16_t handle,
                   std::uint8_t access_mode)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_open_count;
    // Asset trace (env-gated): which files the game asks for, and which it fails
    // to get. A missing or refused asset is invisible from the render side --
    // the game simply stops issuing the draws that would have used it.
    {
        static const bool asset_trace_enabled =
            std::getenv("REPIU_DOS_ASSET_TRACE") != nullptr;
        if (asset_trace_enabled)
        {
            static long asset_trace_count = 0;
            const long index = InterlockedIncrement(&asset_trace_count);
            const bool ok =
                resolved.result == repiu::hle::DosPathResult::kOk;
            // Log every failure, but cap successes so a hot reload loop cannot
            // drown the interesting lines.
            if (!ok || index <= 200)
            {
                fprintf(stderr,
                        "[repiu-asset] %-4s open #%ld \"%s\" -> %s handle=%u"
                        " err=%u\n",
                        ok ? "OK" : "FAIL", index, guest_path.c_str(),
                        resolved.host_path.string().c_str(), handle,
                        repiu::hle::DosPathResultToErrorCode(resolved.result));
            }
        }
    }
    context->last_dos_open_guest_path = guest_path;
    context->last_dos_open_host_path = resolved.host_path.string();
    context->last_dos_open_virtual_path = resolved.dos_path;
    context->last_dos_open_success =
        resolved.result == repiu::hle::DosPathResult::kOk;
    context->last_dos_open_error =
        repiu::hle::DosPathResultToErrorCode(resolved.result);
    context->last_dos_open_handle = handle;
    context->last_dos_open_access_mode = access_mode;
    RecordDosPathTrace(context,
                       "open",
                       guest_path,
                       resolved.dos_path,
                       resolved.host_path.string(),
                       resolved.result == repiu::hle::DosPathResult::kOk,
                       repiu::hle::DosPathResultToErrorCode(
                           resolved.result),
                       0,
                       access_mode);
}

bool HandleDosOpenFile(CONTEXT* win32_context, ThreadContext* context)
{
    std::string guest_path;
    repiu::hle::DosResolvedPath resolved;
    const std::uint8_t access_mode = static_cast<std::uint8_t>(
        win32_context->Eax & 0xFFU);
    if (!ReadGuestAsciz(context,
                        static_cast<std::uint32_t>(win32_context->Edx),
                        260,
                        &guest_path))
    {
        resolved.result = repiu::hle::DosPathResult::kFileNotFound;
        resolved.guest_path = "";
        resolved.message = "DOS open path is outside runtime memory";
        RecordDosOpen(context, guest_path, resolved, 0, access_mode);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | 0x0002U;
        win32_context->EFlags |= 1U;
        return true;
    }

    std::uint16_t handle = 0;
    if (!repiu::hle::OpenDosFile(&context->dos_file_system,
                                 guest_path,
                                 access_mode,
                                 &resolved,
                                 &handle))
    {
        return false;
    }

    RecordDosOpen(context, guest_path, resolved, handle, access_mode);
    if (resolved.result == repiu::hle::DosPathResult::kOk)
    {
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | handle;
        win32_context->EFlags &= ~1U;
    }
    else
    {
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) |
            repiu::hle::DosPathResultToErrorCode(resolved.result);
        win32_context->EFlags |= 1U;
    }
    return true;
}

bool HandleDosFileAttributes(CONTEXT* win32_context,
                             ThreadContext* context)
{
    const std::uint8_t subfunction = static_cast<std::uint8_t>(
        win32_context->Eax & 0xFFU);
    std::string guest_path;
    repiu::hle::DosResolvedPath resolved;
    if (!ReadGuestAsciz(context, win32_context->Edx, 260, &guest_path))
    {
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | 0x0003U;
        win32_context->EFlags |= 1U;
        return true;
    }

    bool success = false;
    if (subfunction == 0x00)
    {
        std::uint16_t attributes = 0;
        success = repiu::hle::QueryDosFileAttributes(
            &context->dos_file_system,
            guest_path,
            &resolved,
            &attributes);
        if (success)
        {
            win32_context->Ecx =
                (win32_context->Ecx & 0xFFFF0000U) | attributes;
        }
    }
    else if (subfunction == 0x01)
    {
        success = repiu::hle::SetDosFileAttributes(
            &context->dos_file_system,
            guest_path,
            static_cast<std::uint16_t>(win32_context->Ecx & 0xFFFFU),
            &resolved);
    }
    else
    {
        resolved.result = repiu::hle::DosPathResult::kAccessDenied;
        resolved.message = "unsupported DOS file attribute subfunction";
    }

    RecordDosPathTrace(context,
                       subfunction == 0 ? "attributes-query" : "attributes-set",
                       guest_path,
                       resolved.dos_path,
                       resolved.host_path.string(),
                       success,
                       repiu::hle::DosPathResultToErrorCode(resolved.result),
                       0,
                       0);
    if (success)
    {
        win32_context->EFlags &= ~1U;
    }
    else
    {
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) |
            repiu::hle::DosPathResultToErrorCode(resolved.result);
        win32_context->EFlags |= 1U;
    }
    return true;
}

const repiu::hle::DosOpenFileHandle* FindDosOpenFile(
    const ThreadContext* context,
    std::uint16_t handle)
{
    if (context == nullptr)
    {
        return nullptr;
    }
    for (const repiu::hle::DosOpenFileHandle& candidate :
         context->dos_file_system.open_files)
    {
        if (candidate.open && candidate.handle == handle)
        {
            return &candidate;
        }
    }
    return nullptr;
}

void CaptureDosTermination(CONTEXT* win32_context,
                           ThreadContext* context)
{
    if (win32_context == nullptr || context == nullptr)
    {
        return;
    }
    context->dos_termination_captured = true;
    context->dos_termination_ax = win32_context->Eax & 0xFFFFU;
    context->dos_termination_eip = win32_context->Eip;
    context->dos_termination_esp = win32_context->Esp;
    const void* stack = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(win32_context->Esp));
    if (IsGuestRangeReadable(
            context, stack, sizeof(context->dos_termination_stack)))
    {
        std::memcpy(context->dos_termination_stack,
                    stack,
                    sizeof(context->dos_termination_stack));
    }
}

Win32DosFileIoTraceEntry& AllocateDosFileIoTrace(
    ThreadContext* context,
    const char* operation,
    std::uint16_t handle)
{
    Win32DosFileIoObservation& observation = context->dos_file_io;
    const std::uint32_t sequence = ++observation.observed_count;
    const std::uint32_t slot =
        (sequence - 1U) % kWin32DosFileIoTraceCapacity;
    Win32DosFileIoTraceEntry& entry = observation.trace[slot];
    entry = {};
    entry.valid = true;
    entry.sequence = sequence;
    entry.operation = operation;
    entry.handle = handle;
    const repiu::hle::DosOpenFileHandle* open_file =
        FindDosOpenFile(context, handle);
    if (open_file != nullptr)
    {
        entry.host_path = open_file->host_path.string();
        entry.position_before = static_cast<std::uint32_t>(
            open_file->file_offset);
        entry.position_after = entry.position_before;
    }
    observation.trace_stored_count = std::min(
        observation.observed_count, kWin32DosFileIoTraceCapacity);
    observation.trace_wrapped =
        observation.observed_count > kWin32DosFileIoTraceCapacity;
    return entry;
}

void RecordDosRead(const CONTEXT* win32_context,
                   ThreadContext* context,
                   std::uint16_t handle,
                   std::uint32_t requested_bytes,
                   std::uint32_t actual_bytes,
                   std::uint32_t buffer,
                   bool success,
                   std::uint16_t error,
                   const std::vector<std::uint8_t>* bytes)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_read_count;
    context->last_dos_read_handle = handle;
    context->last_dos_read_requested_bytes = requested_bytes;
    context->last_dos_read_actual_bytes = actual_bytes;
    context->last_dos_read_buffer = buffer;
    context->last_dos_read_success = success;
    context->last_dos_read_error = error;
    Win32DosFileIoTraceEntry& entry =
        AllocateDosFileIoTrace(context, "read", handle);
    if (win32_context != nullptr)
    {
        entry.guest_eip = win32_context->Eip;
        entry.guest_esp = win32_context->Esp;
        const auto* guest_stack = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(win32_context->Esp));
        if (IsGuestRangeReadable(context,
                                 guest_stack,
                                 sizeof(entry.guest_stack)))
        {
            std::memcpy(entry.guest_stack,
                        guest_stack,
                        sizeof(entry.guest_stack));
        }
    }
    entry.requested_bytes = requested_bytes;
    entry.actual_bytes = actual_bytes;
    entry.dos_error = error;
    const repiu::hle::DosOpenFileHandle* open_file =
        FindDosOpenFile(context, handle);
    if (open_file != nullptr)
    {
        entry.position_after = static_cast<std::uint32_t>(
            open_file->file_offset);
        entry.position_before = entry.position_after - actual_bytes;
    }
    if (bytes != nullptr)
    {
        entry.prefix_size = static_cast<std::uint32_t>(std::min<std::size_t>(
            bytes->size(), kWin32DosFileIoPrefixCapacity));
        if (entry.prefix_size != 0)
        {
            std::memcpy(entry.prefix, bytes->data(), entry.prefix_size);
        }
    }
}

bool HandleDosReadFile(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint16_t handle = static_cast<std::uint16_t>(
        win32_context->Ebx & 0xFFFFU);
    const std::uint32_t requested_bytes =
        static_cast<std::uint32_t>(win32_context->Ecx);
    const std::uint32_t buffer =
        static_cast<std::uint32_t>(win32_context->Edx);

    // Resolve the handle to a name before the read so the trace says which
    // asset is being consumed; a bare handle number is useless for deciding
    // whether e.g. PIU.DAT was read to completion.
    const repiu::hle::DosOpenFileHandle* read_file_before =
        FindDosOpenFile(context, handle);
    const std::string read_name = read_file_before != nullptr
        ? read_file_before->dos_path
        : std::string("<unknown>");
    const std::uint32_t read_offset_before = read_file_before != nullptr
        ? static_cast<std::uint32_t>(read_file_before->file_offset)
        : 0U;

    std::vector<std::uint8_t> bytes;
    std::uint32_t actual_bytes = 0;
    std::uint16_t dos_error = 0;
    if (!repiu::hle::ReadDosFile(&context->dos_file_system,
                                 handle,
                                 requested_bytes,
                                 &bytes,
                                 &actual_bytes,
                                 &dos_error))
    {
        return false;
    }

    {
        static const bool asset_trace_enabled =
            std::getenv("REPIU_DOS_ASSET_TRACE") != nullptr;
        if (asset_trace_enabled)
        {
            static long read_trace_count = 0;
            const long index = InterlockedIncrement(&read_trace_count);
            // A short read is the signal worth catching: it means the guest
            // asked for more than the HLE returned, which is how a truncated
            // asset silently becomes missing content.
            const bool short_read =
                dos_error == 0 && actual_bytes < requested_bytes;
            if (short_read || dos_error != 0 || index <= 120)
            {
                fprintf(stderr,
                        "[repiu-asset] %-5s read #%ld \"%s\" h=%u off=%u"
                        " want=%u got=%u err=%u\n",
                        dos_error != 0 ? "ERR"
                                       : (short_read ? "SHORT" : "read"),
                        index, read_name.c_str(), handle, read_offset_before,
                        requested_bytes, actual_bytes, dos_error);
            }
        }
    }

    if (dos_error != 0)
    {
        RecordDosRead(win32_context,
                      context,
                      handle,
                      requested_bytes,
                      0,
                      buffer,
                      false,
                      dos_error,
                      &bytes);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | dos_error;
        win32_context->EFlags |= 1U;
        return true;
    }

    if (!bytes.empty() &&
        !WriteGuestBytes(context,
                         reinterpret_cast<void*>(
                             static_cast<std::uintptr_t>(buffer)),
                         bytes.data(),
                         bytes.size()))
    {
        constexpr std::uint16_t kPathNotFound = 0x0003;
        RecordDosRead(win32_context,
                      context,
                      handle,
                      requested_bytes,
                      0,
                      buffer,
                      false,
                      kPathNotFound,
                      &bytes);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | kPathNotFound;
        win32_context->EFlags |= 1U;
        return true;
    }

    RecordDosRead(win32_context,
                  context,
                  handle,
                  requested_bytes,
                  actual_bytes,
                  buffer,
                  true,
                  0,
                  &bytes);
    win32_context->Eax = actual_bytes;
    win32_context->EFlags &= ~1U;
    return true;
}

void RecordDosSeek(ThreadContext* context,
                   std::uint16_t handle,
                   std::uint8_t origin,
                   std::int32_t offset,
                   std::uint32_t position_before,
                   std::uint32_t position,
                   bool success,
                   std::uint16_t error)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_seek_count;
    context->last_dos_seek_handle = handle;
    context->last_dos_seek_origin = origin;
    context->last_dos_seek_offset = offset;
    context->last_dos_seek_position = position;
    context->last_dos_seek_success = success;
    context->last_dos_seek_error = error;
    Win32DosFileIoTraceEntry& entry =
        AllocateDosFileIoTrace(context, "seek", handle);
    entry.origin = origin;
    entry.seek_offset = offset;
    entry.position_before = position_before;
    entry.position_after = success ? position : position_before;
    entry.dos_error = error;
}

bool HandleDosSeekFile(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint8_t origin = static_cast<std::uint8_t>(
        win32_context->Eax & 0xFFU);
    const std::uint16_t handle = static_cast<std::uint16_t>(
        win32_context->Ebx & 0xFFFFU);
    const std::uint32_t raw_offset =
        ((win32_context->Ecx & 0xFFFFU) << 16) |
        (win32_context->Edx & 0xFFFFU);
    const std::int32_t offset = static_cast<std::int32_t>(raw_offset);
    const repiu::hle::DosOpenFileHandle* open_file_before =
        FindDosOpenFile(context, handle);
    const std::uint32_t position_before = open_file_before != nullptr
        ? static_cast<std::uint32_t>(open_file_before->file_offset)
        : 0U;

    std::uint32_t new_position = 0;
    std::uint16_t dos_error = 0;
    if (!repiu::hle::SeekDosFile(&context->dos_file_system,
                                 handle,
                                 origin,
                                 offset,
                                 &new_position,
                                 &dos_error))
    {
        return false;
    }

    {
        static const bool asset_trace_enabled =
            std::getenv("REPIU_DOS_ASSET_TRACE") != nullptr;
        if (asset_trace_enabled)
        {
            static long seek_trace_count = 0;
            const long index = InterlockedIncrement(&seek_trace_count);
            if (dos_error != 0 || index <= 120)
            {
                fprintf(stderr,
                        "[repiu-asset] %-5s seek #%ld \"%s\" h=%u origin=%u"
                        " off=%d %u->%u err=%u\n",
                        dos_error != 0 ? "ERR" : "seek", index,
                        open_file_before != nullptr
                            ? open_file_before->dos_path.c_str()
                            : "<unknown>",
                        handle, origin, offset, position_before, new_position,
                        dos_error);
            }
        }
    }

    if (dos_error != 0)
    {
        RecordDosSeek(context,
                      handle,
                      origin,
                      offset,
                      position_before,
                      0,
                      false,
                      dos_error);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | dos_error;
        win32_context->EFlags |= 1U;
        return true;
    }

    RecordDosSeek(context,
                  handle,
                  origin,
                  offset,
                  position_before,
                  new_position,
                  true,
                  0);
    win32_context->Eax =
        (win32_context->Eax & 0xFFFF0000U) |
        (new_position & 0xFFFFU);
    win32_context->Edx =
        (win32_context->Edx & 0xFFFF0000U) |
        ((new_position >> 16) & 0xFFFFU);
    win32_context->EFlags &= ~1U;
    return true;
}

void RecordDosClose(ThreadContext* context,
                    std::uint16_t handle,
                    bool success,
                    std::uint16_t error)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_close_count;
    context->last_dos_close_handle = handle;
    context->last_dos_close_success = success;
    context->last_dos_close_error = error;
}

bool HandleDosCloseFile(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint16_t handle = static_cast<std::uint16_t>(
        win32_context->Ebx & 0xFFFFU);

    std::uint16_t dos_error = 0;
    if (!repiu::hle::CloseDosFile(&context->dos_file_system,
                                  handle,
                                  &dos_error))
    {
        return false;
    }

    if (dos_error != 0)
    {
        RecordDosClose(context, handle, false, dos_error);
        win32_context->Eax =
            (win32_context->Eax & 0xFFFF0000U) | dos_error;
        win32_context->EFlags |= 1U;
        return true;
    }

    RecordDosClose(context, handle, true, 0);
    win32_context->EFlags &= ~1U;
    return true;
}

void RecordDosIoctl(ThreadContext* context,
                    std::uint8_t subfunction,
                    std::uint16_t handle,
                    bool success,
                    std::uint16_t error,
                    std::uint16_t device_info)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_ioctl_count;
    context->last_dos_ioctl_subfunction = subfunction;
    context->last_dos_ioctl_handle = handle;
    context->last_dos_ioctl_success = success;
    context->last_dos_ioctl_error = error;
    context->last_dos_ioctl_device_info = device_info;
}

bool HandleDosIoctl(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint8_t subfunction = static_cast<std::uint8_t>(
        win32_context->Eax & 0xFFU);
    const std::uint16_t handle = static_cast<std::uint16_t>(
        win32_context->Ebx & 0xFFFFU);
    if (subfunction != 0x00)
    {
        std::ostringstream stream;
        stream << "unsupported DOS IOCTL subfunction AL=0x"
               << std::hex << static_cast<unsigned>(subfunction);
        context->hle_message = stream.str();
        return false;
    }

    std::uint16_t device_info = 0;
    if (handle < 5)
    {
        device_info = 0x0080;
        RecordDosIoctl(context, subfunction, handle, true, 0, device_info);
        win32_context->Edx =
            (win32_context->Edx & 0xFFFF0000U) | device_info;
        win32_context->EFlags &= ~1U;
        return true;
    }

    if (repiu::hle::IsDosFileHandleOpen(context->dos_file_system, handle))
    {
        RecordDosIoctl(context, subfunction, handle, true, 0, device_info);
        win32_context->Edx =
            (win32_context->Edx & 0xFFFF0000U) | device_info;
        win32_context->EFlags &= ~1U;
        return true;
    }

    constexpr std::uint16_t kInvalidHandle = 0x0006;
    RecordDosIoctl(context,
                   subfunction,
                   handle,
                   false,
                   kInvalidHandle,
                   0);
    win32_context->Eax =
        (win32_context->Eax & 0xFFFF0000U) | kInvalidHandle;
    win32_context->EFlags |= 1U;
    return true;
}


void RecordDosResize(ThreadContext* context,
                     std::uint16_t selector,
                     std::uint16_t paragraphs,
                     bool success,
                     std::uint16_t error,
                     std::uint32_t requested_end,
                     std::uint32_t allocator_end)
{
    if (context == nullptr)
    {
        return;
    }

    ++context->handled_dos_resize_count;
    context->last_dos_resize_selector = selector;
    context->last_dos_resize_paragraphs = paragraphs;
    context->last_dos_resize_success = success;
    context->last_dos_resize_error = error;
    context->last_dos_resize_requested_end = requested_end;
    context->last_dos_resize_allocator_end = allocator_end;
}


bool HandleDosResizeMemoryBlock(CONTEXT* win32_context,
                                ThreadContext* context)
{
    // DOS/4G resize requests can carry 32-bit paragraph counts in EBX;
    // record the full register so 16-bit truncation stays observable
    // (Task 221) while the legacy 16-bit path remains in effect below.
    const std::uint32_t full_ebx =
        static_cast<std::uint32_t>(win32_context->Ebx);
    const std::uint16_t paragraphs = static_cast<std::uint16_t>(
        win32_context->Ebx & 0xFFFFU);
    constexpr std::uint16_t kInsufficientMemory = 0x0008;

    // The raw guest_es shadow can still hold the host entry-time selector
    // (observed 0x2B), which resolves to no descriptor and disables the
    // allocator ceiling; use the physical/shadow-reconciling reader.
    const std::uint16_t resize_selector =
        ReadGuestSegmentSelector(*context, 0U, win32_context);
    std::uint32_t selector_base = 0;
    const auto* descriptor = repiu::runtime::FindDescriptor(
        context->selector_table, resize_selector);
    if (descriptor != nullptr)
    {
        selector_base = descriptor->base;
    }
    if (context->shared_live_telemetry != nullptr)
    {
        InterlockedIncrement(
            &context->shared_live_telemetry->dos_resize_count);
        InterlockedExchange(
            &context->shared_live_telemetry->dos_resize_last_ebx,
            static_cast<long>(full_ebx));
        InterlockedExchange(
            &context->shared_live_telemetry->dos_resize_last_selector,
            static_cast<long>(resize_selector));
        InterlockedExchange(
            &context->shared_live_telemetry->dos_resize_last_base,
            static_cast<long>(selector_base));
    }

    // DOS/4G resize semantics: the block is the client program's linear
    // block. When the ES descriptor does not resolve (flat model, or the
    // shadow still holding the host selector), fall back to the relocated
    // program base so the request is measured against the real block.
    const std::uint32_t block_base =
        selector_base != 0U ? selector_base : context->runtime_base;
    const std::uint64_t requested_size =
        static_cast<std::uint64_t>(full_ebx) * 16U;
    const std::uint64_t requested_end = block_base + requested_size;
    std::uint32_t allocator_end =
        static_cast<std::uint32_t>(requested_end & 0xFFFFFFFFU);

    if (context->linexe_arena_layout.valid &&
        context->linexe_arena_layout.dynamic_allocator_end != 0)
    {
        const std::uint32_t dynamic_allocator_end =
            context->linexe_arena_layout.dynamic_allocator_end;

        if (requested_end > dynamic_allocator_end)
        {
            std::uint32_t max_paragraphs = 0;
            if (dynamic_allocator_end > block_base)
            {
                max_paragraphs = (dynamic_allocator_end - block_base) / 16U;
            }
            const std::uint32_t final_allocator_end =
                block_base + max_paragraphs * 16U;

            RecordDosResize(context,
                            resize_selector,
                            paragraphs,
                            false,
                            kInsufficientMemory,
                            static_cast<std::uint32_t>(
                                requested_end & 0xFFFFFFFFU),
                            final_allocator_end);
            if (context->shared_live_telemetry != nullptr)
            {
                InterlockedIncrement(
                    &context->shared_live_telemetry->dos_resize_reject_count);
            }

            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | kInsufficientMemory;
            // DOS/4G clients read the maximum block size back as a full
            // 32-bit paragraph count.
            win32_context->Ebx = max_paragraphs;
            win32_context->EFlags |= 1U;
            return true;
        }
    }

    RecordDosResize(context,
                    resize_selector,
                    paragraphs,
                    true,
                    0,
                    static_cast<std::uint32_t>(requested_end & 0xFFFFFFFFU),
                    allocator_end);
    win32_context->EFlags &= ~1U;
    return true;
}

void HandleDosGetInterruptVector(CONTEXT* win32_context,
                                 ThreadContext* context)
{
    const std::uint8_t vector = static_cast<std::uint8_t>(
        win32_context->Eax & 0xFFU);
    const DosInterruptVectorShadow& entry =
        context->dos_interrupt_vectors[vector];
    const std::uint16_t segment = entry.valid ? entry.segment : 0;
    const std::uint16_t offset = entry.valid ? entry.offset : 0;

    context->guest_es = segment;
    win32_context->SegEs = segment;
    ReResolveAotSegmentOverrides(context);
    win32_context->Ebx =
        (win32_context->Ebx & 0xFFFF0000U) | offset;
    win32_context->EFlags &= ~1U;
}

void HandleDosSetInterruptVector(CONTEXT* win32_context,
                                 ThreadContext* context)
{
    const std::uint8_t vector = static_cast<std::uint8_t>(
        win32_context->Eax & 0xFFU);
    DosInterruptVectorShadow& entry =
        context->dos_interrupt_vectors[vector];

    entry.segment = context->guest_ds != 0
        ? context->guest_ds
        : static_cast<std::uint16_t>(win32_context->SegDs);
    entry.offset = static_cast<std::uint16_t>(
        win32_context->Edx & 0xFFFFU);
    entry.valid = true;

    // DOS extenders intercept INT 21h AH=25h from 32-bit clients
    // to set the protected mode interrupt vector instead.
    DpmiInterruptVectorShadow& dpmi_entry =
        context->dpmi_interrupt_vectors[vector];
    dpmi_entry.selector = entry.segment;
    dpmi_entry.offset = win32_context->Edx;
    dpmi_entry.valid = true;

    fprintf(stderr, "[repiu-live] DOS INT 21h AH=25h vector 0x%02X set to %04X:%08X\n", vector, dpmi_entry.selector, dpmi_entry.offset);

    win32_context->EFlags &= ~1U;
}

bool HandleDosInterrupt21(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint16_t ax = static_cast<std::uint16_t>(
        win32_context->Eax & 0xFFFFU);
    const std::uint8_t ah = static_cast<std::uint8_t>(
        (win32_context->Eax >> 8) & 0xFF);

    switch (ah)
    {
        case 0x09:
        {
            RecordHandledDosInterrupt(context, 0x21, ax);
            const char* text = reinterpret_cast<const char*>(
                static_cast<std::uintptr_t>(win32_context->Edx));
            if (text == nullptr ||
                !IsGuestRangeReadable(context, text, 1))
            {
                win32_context->Eax = 0;
                break;
            }

            std::uint32_t length = 0;
            while (length < 4096 &&
                   IsGuestRangeReadable(context, text, length + 1) &&
                   text[length] != '$')
            {
                ++length;
            }
            AppendConsoleOutput(context, text, length);
            win32_context->Eax =
                (win32_context->Eax & 0xFFFFFF00U) | static_cast<DWORD>('$');
            break;
        }
        case 0x19:
            RecordHandledDosInterrupt(context, 0x21, ax);
            HandleDosGetCurrentDrive(win32_context, context);
            break;
        case 0x30:
            RecordHandledDosInterrupt(context, 0x21, ax);
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | 0x0007U;
            win32_context->Ebx = 0;
            win32_context->Ecx = 0;
            break;
        case 0x25:
            RecordHandledDosInterrupt(context, 0x21, ax);
            HandleDosSetInterruptVector(win32_context, context);
            break;
        case 0x35:
            RecordHandledDosInterrupt(context, 0x21, ax);
            HandleDosGetInterruptVector(win32_context, context);
            break;
        case 0x3B:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosChangeDirectory(win32_context, context))
            {
                return false;
            }
            break;
        case 0x3D:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosOpenFile(win32_context, context))
            {
                return false;
            }
            break;
        case 0x3E:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosCloseFile(win32_context, context))
            {
                return false;
            }
            break;
        case 0x3F:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosReadFile(win32_context, context))
            {
                return false;
            }
            break;
        case 0x40:
        {
            const void* text = reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(win32_context->Edx));
            const std::uint32_t byte_count = win32_context->Ecx;
            AppendConsoleOutput(
                context, text, byte_count, win32_context->Ebx == 2U);
            win32_context->Eax = byte_count;
            win32_context->EFlags &= ~1U;
            break;
        }
        case 0x42:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosSeekFile(win32_context, context))
            {
                return false;
            }
            break;
        case 0x43:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosFileAttributes(win32_context, context))
            {
                return false;
            }
            break;
        case 0x44:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosIoctl(win32_context, context))
            {
                return false;
            }
            break;
        case 0x47:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosGetCurrentDirectory(win32_context, context))
            {
                return false;
            }
            break;
        case 0x4C:
            CaptureDosTermination(win32_context, context);
            RecoverFromHleExit(win32_context, context);
            return true;
        case 0x4A:
            RecordHandledDosInterrupt(context, 0x21, ax);
            if (!HandleDosResizeMemoryBlock(win32_context, context))
            {
                return false;
            }
            break;
        case 0xFF:
            if (ax == kDos4gIdentificationAx &&
                (win32_context->Edx & 0xFFFFU) == kDos4gIdentificationDx &&
                context->linexe_environment_active)
            {
                win32_context->Eax =
                    (win32_context->Eax & 0xFFFF0000U) |
                    kDos4gwIdentificationAxResult;
                context->guest_gs = kDos4gwClientDataSelector;
                ReResolveAotSegmentOverrides(context);
                if (kDos4gwIdentificationCarry)
                {
                    win32_context->EFlags |= 1U;
                }
            }
            else
            {
                win32_context->Eax &= 0xFFFFFF00U;
                win32_context->EFlags &= ~1U;
            }
            break;
        case 0xED:
            win32_context->Eax &= 0xFFFFFF00U;
            win32_context->EFlags &= ~1U;
            break;
        default:
        {
            std::ostringstream stream;
            stream << "unsupported DOS INT 21h AH=0x"
                   << std::hex << static_cast<unsigned>(ah);
            context->hle_message = stream.str();
            return false;
        }
    }

    win32_context->Eip += 2;
    return true;
}

bool HandleDosInterrupt2F(CONTEXT* win32_context, ThreadContext* context)
{
    const std::uint16_t ax = static_cast<std::uint16_t>(
        win32_context->Eax & 0xFFFF);
    if (ax == 0x1686)
    {
        RecordHandledDosInterrupt(context, 0x2F, ax);
        win32_context->Eax &= 0xFFFF0000U;
        win32_context->Eip += 2;
        return true;
    }
    if (ax == 0x1500)
    {
        RecordHandledDosInterrupt(context, 0x2F, ax);
        if (context->shared_live_telemetry != nullptr)
        {
            InterlockedIncrement(
                &context->shared_live_telemetry->mscdex_probe_count);
        }
        win32_context->Ebx = (win32_context->Ebx & 0xFFFF0000U) |
            (context->mscdex_available ? 1U : 0U);
        win32_context->Ecx = (win32_context->Ecx & 0xFFFF0000U) |
            (context->mscdex_available ? context->mscdex_drive : 0U);
        win32_context->Eip += 2;
        return true;
    }
    if (ax == 0x1510)
    {
        RecordHandledDosInterrupt(context, 0x2F, ax);
        const bool called = context->mscdex_available &&
            (win32_context->Ecx & 0xFFFFU) == context->mscdex_drive &&
            HandleMscdexRequest(
                context,
                context->guest_es,
                static_cast<std::uint16_t>(win32_context->Ebx & 0xFFFFU));
        if (called)
        {
            win32_context->EFlags &= ~1U;
        }
        else
        {
            win32_context->Eax =
                (win32_context->Eax & 0xFFFF0000U) | 0x000FU;
            win32_context->EFlags |= 1U;
        }
        win32_context->Eip += 2;
        return true;
    }

    std::ostringstream stream;
    stream << "unsupported DOS interrupt 0x2f AX=0x"
           << std::hex << static_cast<unsigned>(ax);
    context->hle_message = stream.str();
    return false;
}

} // namespace repiu::platform::win32
