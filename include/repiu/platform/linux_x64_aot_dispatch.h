#ifndef REPIU_PLATFORM_LINUX_X64_AOT_DISPATCH_H_
#define REPIU_PLATFORM_LINUX_X64_AOT_DISPATCH_H_

// Task 562. The x64 return-dispatch contract: how emitted code asks where a
// guest address lives in the cache.
//
// Everything Tasks 560 and 561 emitted had a target known at emit time, so one
// rel32 finished it. A `ret` does not: its target is a guest address that
// appears on the guest stack at run time, and where control must go is a cache
// address. Nothing at emit time joins those, so the emitted slot jumps to a
// thunk and the thunk asks.

#include "repiu/platform/linux_x64_aot_frame.h"

#include <cstdint>

namespace repiu::platform
{

// What a resolver answers with: the host address to continue at, or zero.
//
// Zero is not an error channel, it is the fail-closed answer -- the thunk traps
// on it, which is Task 553's rule that anything long mode cannot complete
// reaches a boundary rather than guessing.
using LinuxX64DispatchResolver = std::uintptr_t (*)(
    void* context, LinuxX64AotDispatchFrame* frame);

// Where the thunk looks for its three pieces.
//
// Globals, and deliberately temporary ones. x64 has no engine runtime yet --
// Task 544's guest entry is still fail-closed -- so there is no ThreadContext
// to hang these off. This unit's job is to establish the contract and prove it
// by execution; when the engine reaches x64 these become fields it owns, and
// the thunk's three loads become loads from it. Nothing here should be read as
// the final shape.
//
// The frame is not owned here either. A caller installs one that outlives every
// dispatch it will take.
void InstallLinuxX64Dispatch(LinuxX64AotDispatchFrame* frame,
                             void* context,
                             LinuxX64DispatchResolver resolver);

void ClearLinuxX64Dispatch();

// The address of the temporary frame-pointer global used by emitted diagnostic
// sequences. A cache image may be farther than rel32 from the dispatch code, so
// the emitter materializes this address with movabs when tracing is enabled.
[[nodiscard]] std::uintptr_t LinuxX64DispatchFramePointerAddress();

// The address emitted return slots jump to. Taken as a value rather than
// called: the emitter writes it into the slot with `movabs`, because the
// distance from the code cache to this function is not something the placement
// policy guarantees.
[[nodiscard]] std::uintptr_t LinuxX64ReturnThunkAddress();

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_LINUX_X64_AOT_DISPATCH_H_
