#pragma once

// Host-neutral classifier for AOT boundary exits (Task 262). Given the guest
// instruction bytes the translated block ended on, name the kind of control
// transfer (or lack of one) that forced the single-step boundary, so the
// ~1,400/s inline-cache churn can be attributed to a dominant reason. Pure
// opcode decode: no guest or host state is touched, so this pair compiles and
// unit-tests standalone off the Win32 loader host.

#include <cstddef>
#include <cstdint>

namespace repiu::platform::win32
{

enum class AotBoundaryReason : std::uint32_t
{
    kReturn = 0,            // C3 C2 CB CA
    kIndirectBranch = 1,    // FF /2 /3 /4 /5 (indirect call/jmp) -- inline-cache miss
    kDirectBranch = 2,      // E8 E9 EB 9A EA (direct call/jmp) -- out-of-boundary target
    kConditionalBranch = 3, // 70..7F, 0F 80..8F, E0..E3
    kOther = 4,             // non-transfer / prefixed / unsupported stop
};

constexpr std::uint32_t kAotBoundaryReasonCount = 5;

// Classify from up to `size` readable bytes at the boundary guest EIP. A null
// pointer or zero size yields kOther. Only the lead opcode (and the ModRM /reg
// field for 0xFF) is inspected; prefixed forms fall into kOther by design.
AotBoundaryReason ClassifyAotBoundaryInstruction(const std::uint8_t* bytes,
                                                 std::size_t size);

} // namespace repiu::platform::win32
