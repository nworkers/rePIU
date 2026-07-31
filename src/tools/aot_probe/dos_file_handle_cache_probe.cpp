#include "dos_file_handle_cache_probe.h"

#include "repiu/hle/dos_file_system.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace repiu::tools
{
namespace
{

// A real file is needed because the whole point of the change is host open
// behaviour; a mock stream would not exercise it.
class ScopedTempTree
{
public:
    ScopedTempTree()
    {
        std::error_code error;
        root_ = std::filesystem::temp_directory_path(error) /
            "repiu_dos_handle_probe";
        std::filesystem::remove_all(root_, error);
        std::filesystem::create_directories(root_, error);
        std::ofstream stream(root_ / "ASSET.BIN", std::ios::binary);
        for (int index = 0; index < 8192; ++index)
        {
            const char value = static_cast<char>(index & 0xFF);
            stream.write(&value, 1);
        }
    }

    ~ScopedTempTree()
    {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

}  // namespace

bool RunDosFileHandleCacheProbe()
{
    using repiu::hle::CloseDosFile;
    using repiu::hle::DosVirtualFileSystemState;
    using repiu::hle::OpenDosFile;
    using repiu::hle::ReadDosFile;

    ScopedTempTree tree;

    DosVirtualFileSystemState state;
    state.valid = true;
    state.host_root = tree.root();

    std::uint16_t handle = 0;
    std::uint16_t dos_error = 0;
    repiu::hle::DosResolvedPath resolved;
    const bool opened =
        OpenDosFile(&state, "C:\\ASSET.BIN", 0U, &resolved, &handle) &&
        resolved.result == repiu::hle::DosPathResult::kOk;

    // Four sequential reads against one handle must cost one host open. That
    // ratio is the entire point of Task 374 -- it was one open per read.
    std::vector<std::uint8_t> bytes;
    std::uint32_t actual = 0;
    bool reads_ok = opened;
    for (int index = 0; index < 4 && reads_ok; ++index)
    {
        reads_ok = ReadDosFile(&state, handle, 1024U, &bytes, &actual,
                               &dos_error) &&
            dos_error == 0 && actual == 1024U && bytes.size() == 1024U;
    }
    const bool one_open_for_many_reads = reads_ok &&
        state.file_read_count == 4U && state.host_file_open_count == 1U;

    // Content must still be right, and sequential offsets must advance: a cached
    // stream that failed to seek would silently return the first block again.
    const bool content_advanced =
        reads_ok && bytes.size() == 1024U &&
        bytes[0] == static_cast<std::uint8_t>(3072 & 0xFF) &&
        bytes[1] == static_cast<std::uint8_t>(3073 & 0xFF);

    // Reading past the end must still short-read rather than fail, using the
    // size captured at open instead of a fresh stat.
    std::uint16_t eof_error = 0;
    const bool sought = repiu::hle::SeekDosFile(
        &state, handle, 0U, 8000, &actual, &eof_error) && eof_error == 0;
    const bool short_read =
        sought &&
        ReadDosFile(&state, handle, 1024U, &bytes, &actual, &eof_error) &&
        eof_error == 0 && actual == 192U;

    // A copy of the state must not share the stream: two live handles pointing
    // at one stream would share a file position.
    DosVirtualFileSystemState copy = state;
    std::uint16_t copy_error = 0;
    std::uint32_t copy_actual = 0;
    std::vector<std::uint8_t> copy_bytes;
    const bool copy_reopens =
        !copy.open_files.empty() && !copy.open_files[0].host_stream.warm() &&
        repiu::hle::SeekDosFile(&copy, handle, 0U, 0, &copy_actual,
                                &copy_error) &&
        ReadDosFile(&copy, handle, 16U, &copy_bytes, &copy_actual,
                    &copy_error) &&
        copy_error == 0 && copy_actual == 16U &&
        copy.host_file_open_count == state.host_file_open_count + 1U;

    // Closing releases the stream, so the reused slot cannot read the old file.
    std::uint16_t close_error = 0;
    const bool closed =
        CloseDosFile(&state, handle, &close_error) && close_error == 0 &&
        !state.open_files.empty() && !state.open_files[0].host_stream.warm();

    // A missing file still reports the DOS code it always did.
    std::uint16_t missing_error = 0;
    std::uint16_t missing_handle = 0;
    repiu::hle::DosResolvedPath missing_resolved;
    OpenDosFile(&state, "C:\\NOPE.BIN", 0U, &missing_resolved,
                &missing_handle);
    const bool missing_reported =
        missing_resolved.result != repiu::hle::DosPathResult::kOk;
    (void)missing_error;

    const bool all = opened && one_open_for_many_reads && content_advanced &&
        short_read && copy_reopens && closed && missing_reported;
    std::cout << "dos_handle_cache_opened=" << (opened ? "true" : "false")
              << "\ndos_handle_cache_one_open_for_many_reads="
              << (one_open_for_many_reads ? "true" : "false")
              << "\ndos_handle_cache_content_advanced="
              << (content_advanced ? "true" : "false")
              << "\ndos_handle_cache_short_read="
              << (short_read ? "true" : "false")
              << "\ndos_handle_cache_copy_reopens="
              << (copy_reopens ? "true" : "false")
              << "\ndos_handle_cache_close_releases="
              << (closed ? "true" : "false")
              << "\ndos_handle_cache_missing_reported="
              << (missing_reported ? "true" : "false")
              << "\ndos_handle_cache_all=" << (all ? "true" : "false")
              << std::endl;
    return all;
}

}  // namespace repiu::tools
