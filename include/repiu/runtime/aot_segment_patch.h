#pragma once

// Task 568. Writing the bytes of a segment-override patch, apart from deciding
// which memory to open for it.
//
// The two are different kinds of knowledge and they were in the same place.
// What to write is a fact about what the emitter emitted, and it belongs beside
// the emitter; unprotecting a page and flushing an instruction cache is the
// engine's business. Keeping them together meant the only way to exercise the
// patcher was to link the whole engine -- which on Linux drags OpenGL into a
// probe deliberately built to have no platform layer, so the x64 slot could not
// be verified against the real patcher at all.
//
// The split is on that seam. `PatchAotSegmentOverrideSites` takes bytes that
// are already writable and does nothing else.

#include "repiu/runtime/aot_code_cache.h"

#include <cstdint>
#include <vector>

namespace repiu::runtime
{

enum class AotSegmentAccessPolicy : std::uint8_t
{
    kUnresolved = 0,
    kNativeFolded,
    kHleLowMemory,
};

struct AotSegmentResolution
{
    std::uint32_t shadow_address = 0;
    std::uint16_t selector = 0;
    std::uint32_t base = 0;
    std::uint32_t limit = 0;
    std::uint32_t flags = 0;
    AotSegmentAccessPolicy policy = AotSegmentAccessPolicy::kUnresolved;
};

struct AotSegmentTable
{
    AotSegmentResolution segments[6];
};

// Task 712. The segment-table index of SS, in the order `AotSegmentTable`
// keeps: 0=ES, 1=CS, 2=SS, 3=DS, 4=FS, 5=GS.
inline constexpr std::uint8_t kAotSegmentIndexSs = 2U;

// Task 712. What an explicit `SS:` override folds when the guest is on the
// loader's own stack.
//
// On x86, `push`, `pop`, `[esp]` and `ss:[esp]` are the same memory, so an
// explicit SS override has to reach the address an implicit stack access under
// the same SS reaches. Under the loader's initial stack selector, 32-bit
// implicit stack accesses run natively on the host's flat segments and the
// loader hands out a linear `ESP` -- which makes the effective base 0, whatever
// the selector's descriptor says. Folding that descriptor base instead is what
// sent Watcom `lseek`'s `mov ss:[edi],ax` past `[esp]` and made pumpit2a's
// PIU.BIN loader read 1,110,269 records from a 560-byte file.
//
// Any other SS keeps its descriptor base. The guest does switch stacks itself
// -- Task 692's object 3 loads selector B4 with base 0x0158A83C and a 16-bit
// `SP` -- and there the base is real, and the mode16 PUSH HLE adds it too.
//
// Only a natively folded SS entry is touched; selector 0 and DOS low-memory
// resolutions route to the HLE boundary and mean something else. A zero
// `flat_stack_selector` means none is known, and nothing changes.
void ApplyFlatStackSegmentFold(std::uint16_t flat_stack_selector,
                               AotSegmentResolution* ss_resolution);

// Task 717. Task 712's rule, for every segment register and for the loader's
// data selector as well as its stack selector.
//
// The two selectors are the loader's bindings for the guest's 32-bit objects:
// the initial DS (object 2) and the initial SS (object 4). Under either one the
// guest's implicit accesses run natively on the host's flat segments with
// linear offsets, so an explicit override naming the same selector has to fold
// base 0 too -- whichever register carries it. pumpit2a reads DS into DX, loads
// it into ES, and formats `%d` through `es:[ebx]` into a stack buffer. Win32
// hands the guest the host's flat selector there; Linux x64 hands it 0x0024,
// whose descriptor base 0x01010000 sent the read into texture pixels, and the
// unterminated "number" overwrote the stack and the heap above it.
//
// Other selectors keep their descriptor base, as Task 712's do. CS has no
// shadow and is not touched. A zero selector means none is known.
void ApplyFlatSegmentFolds(std::uint16_t flat_stack_selector,
                           std::uint16_t flat_data_selector,
                           AotSegmentTable* table);

struct AotSegmentOverridePatchStats
{
    std::uint32_t native_site_count = 0;
    std::uint32_t hle_site_count = 0;
    std::uint32_t unresolved_site_count = 0;
};

struct AotGuardedSegmentLoadPatchStats
{
    std::uint32_t native_site_count = 0;
    std::uint32_t unresolved_site_count = 0;
};

struct AotGuardedSegmentPopPatchStats
{
    std::uint32_t native_site_count = 0;
    std::uint32_t unresolved_site_count = 0;
};

struct AotGuardedSegmentReadPatchStats
{
    std::uint32_t native_site_count = 0;
    std::uint32_t unresolved_site_count = 0;
};

// Patch every segment-override site in `sites` into `bytes`, which must already
// be writable and must be the base of the placed image the offsets refer to.
// Returns how many sites were considered.
std::uint32_t PatchAotSegmentOverrideSites(
    std::uint8_t* bytes,
    const std::vector<AotSegmentOverrideSite>& sites,
    const AotSegmentTable& table,
    AotSegmentOverridePatchStats* stats);

// Patch guarded segment-load sites into already-writable image bytes. Counter
// addresses are required only by i386 sites that declare counter operands;
// long-mode sites patch only the shadow selector address.
std::uint32_t PatchAotGuardedSegmentLoadSites(
    std::uint8_t* bytes,
    const std::vector<AotGuardedSegmentLoadSite>& sites,
    const AotSegmentTable& table,
    std::uint32_t success_counter_address,
    std::uint32_t fallback_counter_address,
    AotGuardedSegmentLoadPatchStats* stats);

// Patch guarded segment-pop sites into already-writable image bytes, on the
// same contract as the load patcher above. Task 571 moved this off the engine,
// which had assumed every site opens `9C` and carries two counter operands --
// true of the i386 slot and of neither long-mode slot.
std::uint32_t PatchAotGuardedSegmentPopSites(
    std::uint8_t* bytes,
    const std::vector<AotGuardedSegmentPopSite>& sites,
    const AotSegmentTable& table,
    std::uint32_t success_counter_address,
    std::uint32_t fallback_counter_address,
    AotGuardedSegmentPopPatchStats* stats);

// Patch guarded segment-read sites into already-writable image bytes, on the
// same contract as the two patchers above. Task 586 moved this off the engine,
// which had two copies of it -- placement and re-resolution -- each assuming the
// i386 `9C` opening.
std::uint32_t PatchAotGuardedSegmentReadSites(
    std::uint8_t* bytes,
    const std::vector<AotGuardedSegmentReadSite>& sites,
    const AotSegmentTable& table,
    AotGuardedSegmentReadPatchStats* stats);

}  // namespace repiu::runtime

