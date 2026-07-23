#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace repiu::platform::win32::detail
{

struct VerifiedRegionFailure
{
    std::uint32_t instruction = 0;
    std::uint32_t opcode = 0;
    std::uint32_t bytes_low = 0;
    std::uint32_t bytes_high = 0;
};

struct NativeLinearSpan
{
    std::uint32_t boundary_address = 0;
    std::uint32_t instruction_count = 0;
    bool boundary_sensitive = false;
    bool boundary_memory_write = false;
};

bool VerifyNativeFunctionWithZydis(
    std::uint32_t entry,
    std::uint32_t runtime_base,
    std::uint32_t runtime_size,
    std::unordered_map<std::uint32_t, std::int8_t>* cache,
    VerifiedRegionFailure* failure);

// Route A region scan (Task 266). Like VerifyNativeFunctionWithZydis, but instead
// of rejecting a function that contains HLE-sensitive instructions (segment ops,
// INT, IO, string, privileged), it collects their addresses into `sensitive` so
// the caller can breakpoint only those and run everything between them natively.
// Follows direct branches and direct calls across the reachable graph. Returns
// true only when the region is fully analyzable: every branch/call target is
// direct and in range, no indirect/far transfer is reachable, at least one
// return exists, and the sensitive count stays within `max_sensitive`. Reachable
// indirect/far transfers make the sensitive set unprovable, so such regions are
// rejected (the caller keeps single-stepping them).
bool ScanNativeRegionWithZydis(
    std::uint32_t entry,
    std::uint32_t runtime_base,
    std::uint32_t runtime_size,
    std::uint32_t max_sensitive,
    std::vector<std::uint32_t>* sensitive);

// Task 275 general-entry native coverage. Finds a straight-line sequence of at
// least two ordinary instructions ending immediately before the first HLE-
// sensitive instruction, control transfer, or explicit memory write. The
// boundary itself is not part of the span and remains on the existing
// single-step/HLE path.
bool ScanNativeLinearSpanWithZydis(
    std::uint32_t entry,
    std::uint32_t runtime_base,
    std::uint32_t runtime_size,
    NativeLinearSpan* span);

}  // namespace repiu::platform::win32::detail
