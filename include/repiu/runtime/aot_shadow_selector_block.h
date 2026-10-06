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
//
// Task i018. `accepted_pair` holds, per register, the two selectors a guarded
// load slot may switch between natively by writing the shadow word, with no
// INT3: [0] is the host's flat selector, written once when the block is
// seeded, and [1] is the one non-flat selector the HLE load last accepted
// whose descriptor base is 0 (pumpitea's 0x0024). Zero matches no load value,
// so a cleared entry simply sends every switch back to the HLE. The slot
// compares against these words rather than against patched immediates, so
// changing a pair member never requires a cache re-patch.
struct AotShadowSelectorBlock
{
    std::uint16_t selectors[6] = {};
    std::uint16_t accepted_pair[6][2] = {};
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

// Task i018. Seed every register's accepted flat member. ES/DS/FS/GS take the
// loader's flat data selector and SS its flat stack selector; zero selectors
// seed nothing. Call once, after the block is reserved.
void SeedAotShadowAcceptedPairs(AotShadowSelectorBlock* block,
                                std::uint16_t flat_data_selector,
                                std::uint16_t flat_stack_selector);

}  // namespace repiu::runtime
