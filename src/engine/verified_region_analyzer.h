#pragma once

#include <cstdint>
#include <unordered_map>

namespace repiu::engine::detail
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
    bool boundary_sensitive = false;
    bool boundary_memory_write = false;
};

bool VerifyNativeFunctionWithZydis(
    std::uint32_t entry,
    std::uint32_t runtime_base,
    std::uint32_t runtime_size,
    std::unordered_map<std::uint32_t, std::int8_t>* cache,
    VerifiedRegionFailure* failure);

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

}  // namespace repiu::engine::detail
