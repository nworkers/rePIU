#pragma once

#include "repiu/engine/execution_time_profile.h"

#include <cstdint>

namespace repiu::engine
{

// Task 511. The execution time attribution, printed while the run is going.
//
// Everything this engine measures has until now been reported by the loader
// after the guest thread stopped. On Linux a run that reaches rendering never
// stops it -- twenty-one runs across Tasks 509 and 510 all reported
// `stopped=0`, and that arm ends at `_Exit` (Task 508). So the attribution that
// would say where a frame's time goes could not be read on the host where the
// question was being asked.
//
// **This runs on the guest thread**, which is the thread that writes every
// counter it reads (the scopes in the boundary handler). That is the whole
// reason it needs no lock and cannot tear: writer and reader are the same
// thread. A reporter on any other thread would need both.
//
// It is called at one place only -- the `grBufferSwap` gate, once a frame --
// because a clock read on every gate entry would make the instrument part of
// what it measures. Task 353 set that rule and the profile's own comments cite
// it.
//
// Reports are spaced by `REPIU_LIVE_PROFILE_INTERVAL_MS`. Unset or zero is off,
// and off costs one comparison a frame.

// Task 516: what the AOT cache did, in the shape the reporter needs.
//
// The counters live on `ThreadContext`, which belongs to the execution engine.
// Passing that type in would make this reporter depend on the engine and invert
// the layering, so the caller fills this instead -- a plain struct of the values
// it already has in hand.
//
// The five reasons sum to `boundary`. That identity is the check that says the
// split can be trusted, and a report that breaks it is a finding rather than a
// number to interpret.
struct Win32LiveAotCounters
{
    std::uint32_t cache_entry = 0;
    std::uint32_t boundary = 0;
    std::uint32_t boundary_return = 0;
    std::uint32_t boundary_indirect = 0;
    std::uint32_t boundary_direct = 0;
    std::uint32_t boundary_conditional = 0;
    std::uint32_t boundary_other = 0;
    std::uint32_t reentry = 0;
    std::uint32_t legacy_fallback = 0;
    // Task 263(b)'s residency proxy: straight-line guest instructions from a
    // cache entry to its first control transfer, and how many entries were
    // sampled. Their ratio says how far the cache runs before it lets go.
    std::uint32_t residency_instructions = 0;
    std::uint32_t residency_samples = 0;
    // Task 517: the path that lets a reentry happen without a trap.
    //
    // 516 measured 1.010 breakpoints per cache reentry on Linux against
    // Windows' 0.0432, and named what Windows uses for the other 96%: the Glide
    // gate sites it patches to jump straight from the cache to the host. Its
    // thunk exists on both hosts and its enable predicate defaults to true on
    // both, so whether it patches is the question rather than whether it is
    // allowed to.
    //
    // `patched` is how many sites were rewritten; `entry` and `success` are how
    // often the rewritten path was taken and worked. Sites patched with no
    // entries would mean the patch never runs; no sites at all would mean the
    // patching itself is refused.
    std::uint32_t glide_patched_sites = 0;
    std::uint32_t glide_verified_sites = 0;
    std::uint32_t glide_entry = 0;
    std::uint32_t glide_success = 0;
    std::uint32_t glide_target_miss = 0;
    std::uint32_t glide_terminal_failure = 0;
    // Task 518: what the patch resolved to.
    //
    // 517 established that the patched sites are entered and succeed on Linux,
    // and that a successful entry still costs a trap there while on Windows it
    // does not. These two say what the site was pointed at: a resolved target
    // is one the patch could name, and a relinked cache target is one inside
    // the code cache, which is what lets the jump stay in the cache instead of
    // leaving it.
    std::uint32_t glide_resolved_target = 0;
    std::uint32_t glide_relinked_cache_target = 0;
    std::uint32_t glide_relink_content = 0;
    std::uint32_t glide_relink_fixup = 0;
};

// True when the interval is set to something usable. Cheap; the environment is
// read once.
[[nodiscard]] bool LiveExecutionProfileReportEnabled();

// Emits one line if the interval has elapsed since the last one. `profile` may
// be null, which is the case when the profile itself was never enabled -- there
// is then nothing to report and this returns immediately.
//
// `frames` is the run's presented-frame count, carried so the line can state
// cycles per frame rather than leaving the reader to divide by a number printed
// somewhere else.
void ReportLiveExecutionProfileIfDue(
    const Win32ExecutionTimeProfile* profile,
    std::uint64_t frames,
    const Win32LiveAotCounters& aot);

}  // namespace repiu::engine
