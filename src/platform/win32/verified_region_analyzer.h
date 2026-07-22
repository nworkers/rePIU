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

}  // namespace repiu::platform::win32::detail
