#pragma once

// Task 585/586. The selector words a long-mode guard slot compares against, and
// the memory they have to live in.
//
// A guard slot reads its shadow selector through `cmp word ptr [disp32]`. That
// operand is 32 bits on every host, which is a fact about the emitted
// instruction rather than about the process: on x86-64 the natural home for
// these words -- `ThreadContext`, on the execution thread's stack -- sits above
// 4 GiB, and truncating its address produced an unmapped low address that the
// guard then dereferenced (Task 585).
//
// So the words move to memory chosen for the operand: one page reserved below
// 4 GiB. `ThreadContext` still holds the authoritative selectors; this block is
// the copy the emitted code can name.
//
// Task 586 gave this its own files. It first landed in `aot_segment_patch`,
// whose whole purpose is to be linkable without the platform layer -- and
// reserving a page is exactly the platform work that seam exists to keep out.

#include <cstddef>
#include <cstdint>

namespace repiu::runtime
{

// 6 segment registers: 0=ES, 1=CS, 2=SS, 3=DS, 4=FS, 5=GS. CS has no shadow and
// its slot is left zero, so the index matches `AotSegmentTable::segments`
// rather than being compacted.
struct AotShadowSelectorBlock
{
    std::uint16_t selectors[6] = {};
};

struct AotShadowSelectorReservation
{
    bool valid = false;
    void* base = nullptr;
    std::size_t size = 0;
    AotShadowSelectorBlock* block = nullptr;
    // Why the reservation ended the way it did. Always set, on success too, so
    // the caller has one line to log rather than a bool to interpret.
    const char* message = "shadow selector block was not requested";
};

// Fixed addresses tried in order on a 64-bit host, chosen to sit below the AOT
// code cache's own candidates and above the guest arena's expansion slack.
inline constexpr std::uintptr_t kAotShadowSelectorCandidateBases[] = {
    0x1F000000U,
    0x27000000U,
    0x2F000000U,
    0x37000000U,
    0x3F000000U,
};

// Reserve one page for the block.
//
// On a 32-bit host any reservation is addressable by a 32-bit operand, so the
// allocator chooses. On a 64-bit host only the candidate ladder can satisfy the
// operand: an unhinted `mmap` on x86-64 returns an address above 4 GiB, so a
// "let the allocator choose" fallback there is not a fallback but a guaranteed
// failure with a release on the way out. Task 586 removed it -- failing with a
// reason the caller can print is more useful than failing twice.
[[nodiscard]] AotShadowSelectorReservation ReserveAotShadowSelectorBlock();
void ReleaseAotShadowSelectorBlock(const AotShadowSelectorReservation& reservation);

}  // namespace repiu::runtime
