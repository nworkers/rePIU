#include "dos_file_create_probe.h"

#include "cpu_emul/instruction_emulation.h"
#include "execution/thread_context.h"
#include "repiu/hle/dos_file_system.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace repiu::tools
{
namespace
{

using repiu::hle::DosPathResult;
using repiu::hle::DosResolvedPath;
using repiu::hle::DosVirtualFileSystemState;

// A scratch root the probe owns outright, so a failed run never leaves anything
// inside a runtime mount.
std::filesystem::path MakeScratchRoot()
{
    std::error_code error;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path(error) /
        "repiu_dos_file_create_probe";
    if (error)
    {
        return {};
    }
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    return error ? std::filesystem::path{} : root;
}

bool WriteText(DosVirtualFileSystemState* state, std::uint16_t handle,
               const std::string& text, std::uint32_t* actual)
{
    std::uint16_t dos_error = 0;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(text.data());
    return repiu::hle::WriteDosFile(
               state, handle, bytes,
               static_cast<std::uint32_t>(text.size()), actual, &dos_error) &&
        dos_error == 0;
}

std::string ReadHostFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(stream)),
                       std::istreambuf_iterator<char>());
}

}  // namespace

bool RunDosFileCreateProbe()
{
    using repiu::hle::CloseDosFile;
    using repiu::hle::CreateDosFile;
    using repiu::hle::InitializeDosVirtualFileSystem;
    using repiu::hle::IsDosFileHandleWritable;
    using repiu::hle::OpenDosFile;
    using repiu::hle::QueryDosFileAttributes;
    using repiu::hle::ReadDosFile;

    const std::filesystem::path root = MakeScratchRoot();
    DosVirtualFileSystemState state;
    const bool initialized = !root.empty() &&
        InitializeDosVirtualFileSystem(root, &state) && state.valid;

    // Creation makes a file that was not there and hands back the first user
    // handle, because DOS numbers 0-4 are the standard devices (Task 228).
    DosResolvedPath resolved;
    std::uint16_t handle = 0;
    const bool created = initialized &&
        CreateDosFile(&state, "ERRLOG.TXT", 0, &resolved, &handle) &&
        resolved.result == DosPathResult::kOk && handle == 5U &&
        std::filesystem::exists(root / "ERRLOG.TXT") &&
        IsDosFileHandleWritable(state, handle) &&
        state.file_create_count == 1U;

    // Writing advances the offset, and a read on the same handle sees the new
    // length rather than the size cached when the handle was made.
    std::uint32_t actual = 0;
    const std::string first = "first line\n";
    const std::string second = "second line\n";
    bool wrote = created &&
        WriteText(&state, handle, first, &actual) &&
        actual == first.size() &&
        WriteText(&state, handle, second, &actual) &&
        actual == second.size() &&
        state.file_write_count == 2U &&
        state.file_written_bytes == first.size() + second.size();
    for (const repiu::hle::DosOpenFileHandle& open_file : state.open_files)
    {
        if (open_file.open && open_file.handle == handle)
        {
            wrote = wrote &&
                open_file.file_offset == first.size() + second.size() &&
                open_file.cached_file_size == first.size() + second.size();
        }
    }

    // Close, reopen for reading, and the bytes come back in order.
    std::uint16_t dos_error = 0;
    std::vector<std::uint8_t> bytes;
    std::uint32_t read_bytes = 0;
    std::uint16_t read_handle = 0;
    DosResolvedPath reopened;
    const bool round_trip = wrote &&
        CloseDosFile(&state, handle, &dos_error) && dos_error == 0 &&
        OpenDosFile(&state, "ERRLOG.TXT", 0, &reopened, &read_handle) &&
        reopened.result == DosPathResult::kOk &&
        ReadDosFile(&state, read_handle, 512U, &bytes, &read_bytes,
                    &dos_error) &&
        dos_error == 0 &&
        read_bytes == first.size() + second.size() &&
        std::string(bytes.begin(), bytes.end()) == first + second &&
        CloseDosFile(&state, read_handle, &dos_error) &&
        // The host file holds exactly what was written, not a partial flush.
        ReadHostFile(root / "ERRLOG.TXT") == first + second;

    // Creating over an existing file empties it, which is what "create or
    // truncate" means and what a guest reopening its log every run depends on.
    std::uint16_t truncate_handle = 0;
    DosResolvedPath truncated;
    const bool truncation = round_trip &&
        CreateDosFile(&state, "ERRLOG.TXT", 0, &truncated, &truncate_handle) &&
        truncated.result == DosPathResult::kOk &&
        std::filesystem::file_size(root / "ERRLOG.TXT") == 0U &&
        CloseDosFile(&state, truncate_handle, &dos_error);

    // A missing parent is path-not-found; creation must not invent directories.
    std::uint16_t missing_handle = 0;
    DosResolvedPath missing;
    const bool missing_parent = initialized &&
        CreateDosFile(&state, "NOSUCHDIR\\LOG.TXT", 0, &missing,
                      &missing_handle) &&
        missing.result == DosPathResult::kPathNotFound &&
        missing_handle == 0U &&
        !std::filesystem::exists(root / "NOSUCHDIR");

    // The guest creates its log with the read-only attribute and then writes to
    // it. Stamping that on the host file would block the very next write, so it
    // lives in the override table where AH=43h still reports it.
    std::uint16_t attribute_handle = 0;
    DosResolvedPath attributed;
    DosResolvedPath queried;
    std::uint16_t attributes = 0;
    const bool read_only_attribute = initialized &&
        CreateDosFile(&state, "RDONLY.TXT", 0x0001U, &attributed,
                      &attribute_handle) &&
        attributed.result == DosPathResult::kOk &&
        WriteText(&state, attribute_handle, "still writable\n", &actual) &&
        actual == 15U &&
        QueryDosFileAttributes(&state, "RDONLY.TXT", &queried, &attributes) &&
        (attributes & 0x0001U) != 0U &&
        (std::filesystem::status(root / "RDONLY.TXT").permissions() &
         std::filesystem::perms::owner_write) !=
            std::filesystem::perms::none &&
        CloseDosFile(&state, attribute_handle, &dos_error);

    // Handles 5..19 is the whole table; the sixteenth create is refused rather
    // than handed a number the guest clib cannot index.
    bool exhaustion = initialized;
    std::vector<std::uint16_t> held;
    for (std::uint16_t index = 0; index < 15U && exhaustion; ++index)
    {
        DosResolvedPath slot;
        std::uint16_t slot_handle = 0;
        const std::string name = "SLOT" + std::to_string(index) + ".TXT";
        exhaustion = CreateDosFile(&state, name, 0, &slot, &slot_handle) &&
            slot.result == DosPathResult::kOk && slot_handle != 0U;
        if (exhaustion)
        {
            held.push_back(slot_handle);
        }
    }
    {
        DosResolvedPath overflow;
        std::uint16_t overflow_handle = 0;
        exhaustion = exhaustion && held.size() == 15U &&
            CreateDosFile(&state, "OVERFLOW.TXT", 0, &overflow,
                          &overflow_handle) &&
            overflow.result == DosPathResult::kAccessDenied &&
            overflow_handle == 0U;
    }
    for (const std::uint16_t open_handle : held)
    {
        CloseDosFile(&state, open_handle, &dos_error);
    }

    // The dynamic backend reaches the traced INT 21h dispatcher before the
    // common DOS dispatcher. Exercise that integration boundary so a service
    // implemented in the common handler cannot be omitted from the traced
    // allow-list again.
    auto dispatch_context =
        std::make_unique<repiu::platform::win32::ThreadContext>();
    std::array<std::uint8_t, 32> guest_memory = {};
    guest_memory[0] = 0xCDU;
    guest_memory[1] = 0x21U;
    constexpr char kDispatchPath[] = "DISPATCH.TXT";
    std::memcpy(guest_memory.data() + 2, kDispatchPath,
                sizeof(kDispatchPath));
    dispatch_context->runtime_base = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(guest_memory.data()));
    dispatch_context->runtime_size =
        static_cast<std::uint32_t>(guest_memory.size());
    const bool dispatch_initialized = initialized &&
        InitializeDosVirtualFileSystem(root, &dispatch_context->dos_file_system);
    CONTEXT win32_context = {};
    win32_context.Eax = 0x00003C00U;
    win32_context.Ecx = 0U;
    win32_context.Edx = dispatch_context->runtime_base + 2U;
    win32_context.Eip = dispatch_context->runtime_base;
    win32_context.EFlags = 1U;
    bool traced_dispatch = dispatch_initialized &&
        repiu::platform::win32::HandleTracedDosInterrupt21(
            &win32_context, dispatch_context.get()) &&
        win32_context.Eip == dispatch_context->runtime_base + 2U &&
        (win32_context.EFlags & 1U) == 0U &&
        (win32_context.Eax & 0xFFFFU) == 5U &&
        dispatch_context->dos_file_system.file_create_count == 1U &&
        std::filesystem::exists(root / "DISPATCH.TXT");
    if (traced_dispatch)
    {
        traced_dispatch = CloseDosFile(
            &dispatch_context->dos_file_system, 5U, &dos_error) &&
            dos_error == 0U;
    }

    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);
    const bool cleaned = !cleanup_error && !std::filesystem::exists(root);

    const bool all = initialized && created && wrote && round_trip &&
        truncation && missing_parent && read_only_attribute && exhaustion &&
        traced_dispatch && cleaned;

    std::cout << "dos_file_create_initialized="
              << (initialized ? "true" : "false")
              << "\ndos_file_create_created=" << (created ? "true" : "false")
              << "\ndos_file_create_write=" << (wrote ? "true" : "false")
              << "\ndos_file_create_round_trip="
              << (round_trip ? "true" : "false")
              << "\ndos_file_create_truncation="
              << (truncation ? "true" : "false")
              << "\ndos_file_create_missing_parent="
              << (missing_parent ? "true" : "false")
              << "\ndos_file_create_read_only_attribute="
              << (read_only_attribute ? "true" : "false")
              << "\ndos_file_create_exhaustion="
              << (exhaustion ? "true" : "false")
              << "\ndos_file_create_traced_dispatch="
              << (traced_dispatch ? "true" : "false")
              << "\ndos_file_create_cleanup="
              << (cleaned ? "true" : "false")
              << "\ndos_file_create_all=" << (all ? "true" : "false") << "\n";
    return all;
}

}  // namespace repiu::tools
