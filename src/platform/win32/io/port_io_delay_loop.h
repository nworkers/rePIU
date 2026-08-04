#pragma once

#include <cstdint>
#include <string_view>

namespace repiu::platform::win32
{

// Task 414. The pumpit3 timer ISR spends every tick in a 200-iteration loop
// whose only body is `inc r; sub eax,eax; in ax,dx`, and whose result is
// overwritten the instruction after the loop ends -- a pure delay built from
// ISA port accesses. Each of those reads costs us one CPU fault, which measures
// 41.9-49.7% of wall clock. This recognises that exact shape and advances the
// loop counter so the guest runs only its final iteration, turning 200 faults
// into two. See docs/design/20260804-414-port-io-delay-loop-batching.md.

enum class PortIoDelayLoopOutcome : std::uint32_t
{
    kBatched = 0,
    // The bytes after the IN are not `cmp r32, imm` plus a backward branch, or
    // the body holds an instruction outside the whitelist.
    kShapeMismatch,
    // The counter is EAX or EDX, which the IN itself uses.
    kRegisterConflict,
    // The IN's destination is not proven dead: the body does not zero it before
    // the read, so skipping reads could change what the guest sees.
    kResultNotDead,
    // Fewer than two iterations remain, so there is nothing to skip.
    kNothingToSkip,
    // Guest bytes could not be read.
    kUnreadable,
    kCount,
};

constexpr std::uint32_t kPortIoDelayLoopOutcomeCount =
    static_cast<std::uint32_t>(PortIoDelayLoopOutcome::kCount);

struct Win32PortIoDelayLoopStats
{
    bool enabled = false;
    std::uint32_t attempt_count = 0;
    std::uint32_t batch_count = 0;
    // Iterations the guest did not execute, and therefore faults not taken.
    std::uint64_t skipped_iteration_count = 0;
    std::uint32_t max_skipped_iterations = 0;
    std::uint32_t outcome_counts[kPortIoDelayLoopOutcomeCount] = {};
    // The loop most recently batched, for reading against the port I/O census.
    std::uint32_t last_loop_address = 0;
    std::uint32_t last_limit = 0;
};

// How far before the IN a loop body may start. The caller validates guest
// readability of exactly this window, so a longer body is refused rather than
// decoded from memory nobody checked.
constexpr std::uint32_t kMaxLoopBodyBytes = 64U;

// Bytes after the IN the matcher reads: `cmp r32, imm32` plus a short branch.
constexpr std::uint32_t kMaxLoopTailBytes = 16U;

// `REPIU_PORT_IO_DELAY_LOOP`: "0"/"off"/"false" disables batching. Default on.
bool ResolvePortIoDelayLoopEnabled(std::string_view setting);
bool PortIoDelayLoopEnabled();

// Guest-thread-only statistics. Kept here rather than in ThreadContext because
// that header is included nearly everywhere and adding a field to it forces a
// full rebuild.
const Win32PortIoDelayLoopStats& GetPortIoDelayLoopStats();

// Attempts the batch after `IN` has already been emulated at `in_address`
// (guest address) with `in_length` bytes. `registers` points at the guest's
// general-purpose registers in x86 encoding order (EAX, ECX, EDX, EBX, ESP,
// EBP, ESI, EDI), of which only the matched counter is ever written. Returns
// true when the counter was advanced.
bool TryBatchPortIoDelayLoop(std::uint32_t in_address,
                             std::uint32_t in_length,
                             std::uint32_t destination_width,
                             std::uint32_t* registers,
                             bool guest_bytes_readable);

}  // namespace repiu::platform::win32
