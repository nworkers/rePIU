#pragma once

#include <cstddef>
#include <cstdint>

namespace repiu::platform::win32
{

// Task 440: `grBufferSwap` blocks the guest thread until `SDL_GL_SwapWindow`
// returns. With vsync on -- the configuration the game is actually played in --
// that is 32.36M cycles, about 10.8 ms, per frame: **32.8% of guest-run**, while
// the guest itself only needs 17.9 ms to build the frame.
//
// Glide's own contract says the swap is asynchronous: the hardware queues the
// flip and returns, and the game polls `grBufferNumPending` to decide whether to
// press on. It calls that entry point exactly once per swap, so the throttle is
// already there -- our answer of zero is what makes it inert. Posting the
// present rather than waiting on it restores the original behaviour and returns
// the wait to the guest.

// Two bounds, both deliberate. One outstanding swap matches double buffering and
// keeps the guest from running more than a frame ahead; the queue bound catches
// everything else, and a post that would exceed either waits.
constexpr std::uint32_t kWin32GlideMaxOutstandingSwaps = 1U;
constexpr std::size_t kWin32GlideAsyncCommandCapacity = 64U;

// Heap-owned by the backend. `ThreadContext` -- which holds the backend -- is a
// stack local in the execution trampoline, and this state's queue plus counters
// were enough to overflow that stack at `grSstWinOpen`. One pointer here, the
// rest on the heap.
struct Win32GlideAsyncPresentState;

struct Win32GlideAsyncPresentSnapshot
{
    bool enabled = false;
    std::uint64_t posted_count = 0;
    std::uint64_t posted_swap_count = 0;
    std::uint64_t executed_count = 0;
    std::uint64_t failure_count = 0;
    std::uint64_t back_pressure_count = 0;
    std::uint64_t refused_count = 0;
    std::uint32_t pending_swap_count = 0;
    std::uint32_t max_queue_depth = 0;
};

// Opt-in until measured. Only meaningful with vsync on: at interval 0 the
// present costs 0.07 ms and there is nothing to recover.
bool ResolveGlideAsyncPresentEnabled(const char* setting);
bool GlideAsyncPresentEnabled();

}  // namespace repiu::platform::win32
