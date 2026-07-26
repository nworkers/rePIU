#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace repiu::platform::win32::detail
{

constexpr std::uint32_t kNativeLinearSpanRejectSnapshotCapacity = 30;

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
    std::uint32_t cacheable_rejection_byte_count = 0;
    std::uint32_t crossed_memory_write_count = 0;
    std::uint32_t chained_direct_jump_count = 0;
    bool boundary_sensitive = false;
    bool boundary_memory_write = false;
    bool boundary_write_guard_uncovered = false;
    bool boundary_backward_jump = false;
};

using NativeLinearSpanWriteGuardQuery = bool (*)(
    void* context, std::uint32_t guest_page);
using NativeLinearSpanRegisterQuery = bool (*)(
    void* context, std::uint32_t zydis_register, std::uint32_t* value);
using NativeLinearSpanWriteTargetQuery = bool (*)(
    void* context, std::uint32_t address, std::uint32_t byte_count);
using NativeLinearSpanDirectJumpTargetQuery = bool (*)(
    void* context, std::uint32_t target);

struct NativeLinearSpanOptions
{
    bool allow_memory_writes = false;
    NativeLinearSpanWriteGuardQuery write_guard_query = nullptr;
    NativeLinearSpanRegisterQuery register_query = nullptr;
    NativeLinearSpanWriteTargetQuery write_target_query = nullptr;
    bool chain_forward_direct_jumps = false;
    NativeLinearSpanDirectJumpTargetQuery direct_jump_target_query = nullptr;
    void* write_guard_context = nullptr;
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
    NativeLinearSpan* span,
    const NativeLinearSpanOptions* options = nullptr);

}  // namespace repiu::platform::win32::detail
