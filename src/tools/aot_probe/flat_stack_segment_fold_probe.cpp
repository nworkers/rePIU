#include "flat_stack_segment_fold_probe.h"

#include "repiu/runtime/aot_segment_patch.h"

#include <cstdint>
#include <iostream>

namespace repiu::tools
{
namespace
{

using repiu::runtime::AotSegmentAccessPolicy;
using repiu::runtime::AotSegmentResolution;
using repiu::runtime::ApplyFlatStackSegmentFold;
using repiu::runtime::ApplyFlatSegmentFolds;
using repiu::runtime::AotSegmentTable;

// The pumpit2a values Task 711 measured on Linux: the loader's stack selector
// 0x0034 over object 4 at 0x01110000, and Task 692's guest-built 16-bit stack
// B4 at 0x0158A83C.
constexpr std::uint16_t kLoaderStackSelector = 0x0034U;
constexpr std::uint32_t kLoaderStackBase = 0x01110000U;
constexpr std::uint16_t kGuestStackSelector = 0x00B4U;
constexpr std::uint32_t kGuestStackBase = 0x0158A83CU;
// Task 717: the loader's data selector over object 2, and object 3's selector,
// which is 16-bit code with a real base.
constexpr std::uint16_t kLoaderDataSelector = 0x0024U;
constexpr std::uint32_t kLoaderDataBase = 0x01010000U;
constexpr std::uint16_t kObject3Selector = 0x002CU;
constexpr std::uint32_t kObject3Base = 0x01100000U;

AotSegmentResolution Folded(const std::uint16_t selector,
                            const std::uint32_t base)
{
    AotSegmentResolution resolution;
    resolution.selector = selector;
    resolution.base = base;
    resolution.limit = 0x0047CC8FU;
    resolution.flags = 0x4092U;
    resolution.policy = AotSegmentAccessPolicy::kNativeFolded;
    return resolution;
}

}  // namespace

bool RunFlatStackSegmentFoldProbe()
{
    // On the loader's stack, `ss:[esp]` has to be `[esp]`: the base goes.
    AotSegmentResolution loader = Folded(kLoaderStackSelector, kLoaderStackBase);
    ApplyFlatStackSegmentFold(kLoaderStackSelector, &loader);
    const bool loader_flat = loader.base == 0U &&
        loader.selector == kLoaderStackSelector &&
        loader.limit == 0x0047CC8FU &&
        loader.policy == AotSegmentAccessPolicy::kNativeFolded;

    // On a stack the guest built itself the base is real and stays.
    AotSegmentResolution guest = Folded(kGuestStackSelector, kGuestStackBase);
    ApplyFlatStackSegmentFold(kLoaderStackSelector, &guest);
    const bool guest_kept = guest.base == kGuestStackBase;

    // Selector 0 and DOS low memory are HLE-boundary resolutions and mean
    // something else entirely; they are not folds and are not touched.
    AotSegmentResolution low = Folded(kLoaderStackSelector, 0x00000400U);
    low.policy = AotSegmentAccessPolicy::kHleLowMemory;
    ApplyFlatStackSegmentFold(kLoaderStackSelector, &low);
    const bool low_memory_kept = low.base == 0x00000400U;

    // No known loader stack means nothing is decided.
    AotSegmentResolution unknown = Folded(kLoaderStackSelector, kLoaderStackBase);
    ApplyFlatStackSegmentFold(0U, &unknown);
    const bool unknown_kept = unknown.base == kLoaderStackBase;

    ApplyFlatStackSegmentFold(kLoaderStackSelector, nullptr);

    // Task 717. The whole table: the data selector goes flat in ES and DS,
    // the stack selector goes flat in ES as it does in SS, and a guest-built
    // stack and object 3 keep their bases. pumpit2a's `%d` reads through ES
    // loaded with 0x0024.
    AotSegmentTable table;
    table.segments[0] = Folded(kLoaderDataSelector, kLoaderDataBase);   // ES
    table.segments[2] = Folded(kGuestStackSelector, kGuestStackBase);  // SS
    table.segments[3] = Folded(kLoaderDataSelector, kLoaderDataBase);   // DS
    table.segments[4] = Folded(kLoaderStackSelector, kLoaderStackBase); // FS
    table.segments[5] = Folded(kObject3Selector, kObject3Base);         // GS
    ApplyFlatSegmentFolds(kLoaderStackSelector, kLoaderDataSelector, &table);
    const bool table_ok = table.segments[0].base == 0U &&
        table.segments[3].base == 0U && table.segments[4].base == 0U &&
        table.segments[2].base == kGuestStackBase &&
        table.segments[5].base == kObject3Base;
    ApplyFlatSegmentFolds(kLoaderStackSelector, kLoaderDataSelector, nullptr);

    const bool all = loader_flat && guest_kept && low_memory_kept &&
        unknown_kept && table_ok;
    std::cout << "flat_stack_segment_fold_loader_stack="
              << (loader_flat ? "true" : "false")
              << "\nflat_stack_segment_fold_guest_stack="
              << (guest_kept ? "true" : "false")
              << "\nflat_stack_segment_fold_low_memory="
              << (low_memory_kept ? "true" : "false")
              << "\nflat_stack_segment_fold_unknown="
              << (unknown_kept ? "true" : "false")
              << "\nflat_segment_fold_table=" << (table_ok ? "true" : "false")
              << "\nflat_stack_segment_fold_all=" << (all ? "true" : "false")
              << "\n";
    return all;
}

}  // namespace repiu::tools
