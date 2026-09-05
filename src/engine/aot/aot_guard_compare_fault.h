#pragma once

// Task 586. A tripwire on the one fault that corrupts the guest stack in
// silence.
//
// Every guard slot opens by pushing EFLAGS onto the *guest* stack, compares a
// shadow selector, and restores the flags on both arms of the branch. That is
// balanced -- unless the compare itself faults. Then the HLE emulates the
// original guest instruction and resumes at the *next* one, the slot's
// `flags_restore` never runs, and a stray EFLAGS word stays on the guest stack
// with ESP four low. The guest pops it into a register some instructions later
// and dereferences it.
//
// That is what Tasks 583, 584 and 585 spent three units chasing: an access
// violation whose reported address was `0x00200202`, which is not a pointer at
// all but an EFLAGS bit pattern. Task 585 removed the trigger by giving the
// guard a shadow address that is genuinely mapped. This says so out loud if the
// window is ever entered again.
//
// The test is exact rather than approximate: the window is the compare
// instruction, whose bounds the sites already record.
//
//   segment override      (cache_offset, guard_selector_offset + 2)
//   load / pop / read     (cache_offset, shadow_address_offset + 4)
//
// It deliberately does not repair. After Task 585 this window is unreachable,
// so reaching it means a premise broke; rewinding guest ESP and EFLAGS on a
// path that cannot be exercised would substitute a guess for a diagnosis.

#include "repiu/engine/aot_code_cache.h"

#include <cstdint>

namespace repiu::engine
{

enum class AotGuardSlotKind : std::uint8_t
{
    kSegmentOverride = 0,
    kGuardedLoad,
    kGuardedPop,
    kGuardedRead,
};

struct AotGuardCompareFault
{
    AotGuardSlotKind kind = AotGuardSlotKind::kSegmentOverride;
    std::uint32_t guest_source = 0;
    std::uint32_t cache_offset = 0;
    std::uint32_t shadow_address = 0;
    std::uint8_t segment_register = 0xFFU;
};

const char* AotGuardSlotKindName(AotGuardSlotKind kind);

// True when `cache_address` -- a host address, not an offset -- lands inside a
// guard slot's compare instruction in `placement`. Returns immediately when the
// placement is not placed or the address is outside it, so the fault path pays
// one range check on every fault that is not this one.
bool FindAotGuardCompareFault(const AotCodeCachePlacement& placement,
                              std::uint32_t cache_address,
                              AotGuardCompareFault* fault);

}  // namespace repiu::engine
