#include "port_io_delay_loop.h"

#include <cstdlib>
#include <cstring>

namespace repiu::platform::win32
{
namespace
{

constexpr std::uint32_t kRegisterEax = 0U;
constexpr std::uint32_t kRegisterEdx = 2U;
constexpr std::uint32_t kRegisterEsp = 4U;

// The loop body is decoded from the branch target up to the IN. Only these
// forms are accepted; anything else ends the match, because a body that can
// touch memory or another port cannot have its iterations skipped.
struct LoopBody
{
    bool valid = false;
    // Counter register and its per-iteration step, exactly +1 or -1.
    std::uint32_t counter_register = 0;
    std::int32_t step = 0;
    bool zeroes_destination = false;
};

Win32PortIoDelayLoopStats& MutableStats()
{
    static Win32PortIoDelayLoopStats stats;
    return stats;
}

void CountOutcome(PortIoDelayLoopOutcome outcome)
{
    Win32PortIoDelayLoopStats& stats = MutableStats();
    const std::uint32_t index = static_cast<std::uint32_t>(outcome);
    if (index < kPortIoDelayLoopOutcomeCount)
    {
        ++stats.outcome_counts[index];
    }
}

const std::uint8_t* GuestBytes(std::uint32_t address)
{
    return reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(address));
}

// Decodes [body_start, body_end) and reports what the body does. `destination`
// is the register the IN writes, whose zeroing is what proves the skipped reads
// are dead.
LoopBody DecodeLoopBody(std::uint32_t body_start,
                        std::uint32_t body_end,
                        std::uint32_t destination)
{
    LoopBody body;
    std::uint32_t address = body_start;
    std::uint32_t counters = 0;
    while (address < body_end)
    {
        const std::uint8_t opcode = GuestBytes(address)[0];
        if (opcode >= 0x40U && opcode <= 0x47U)
        {
            // inc r32
            const std::uint32_t reg = opcode - 0x40U;
            if (reg == kRegisterEsp)
            {
                return body;
            }
            body.counter_register = reg;
            body.step = 1;
            ++counters;
            address += 1U;
            continue;
        }
        if (opcode >= 0x48U && opcode <= 0x4FU)
        {
            // dec r32
            const std::uint32_t reg = opcode - 0x48U;
            if (reg == kRegisterEsp)
            {
                return body;
            }
            body.counter_register = reg;
            body.step = -1;
            ++counters;
            address += 1U;
            continue;
        }
        if ((opcode == 0x29U || opcode == 0x31U) && address + 1U < body_end)
        {
            // sub r,r or xor r,r -- accepted only in the self-zeroing form,
            // where source and destination are the same register.
            const std::uint8_t modrm = GuestBytes(address)[1];
            if ((modrm & 0xC0U) != 0xC0U)
            {
                return body;
            }
            const std::uint32_t source = (modrm >> 3) & 0x07U;
            const std::uint32_t target = modrm & 0x07U;
            if (source != target)
            {
                return body;
            }
            if (target == destination)
            {
                body.zeroes_destination = true;
            }
            else if (target == body.counter_register && counters != 0U)
            {
                // Zeroing the counter inside the body would break the step
                // arithmetic this batch depends on.
                return body;
            }
            address += 2U;
            continue;
        }
        return body;
    }
    // Exactly one counter, and it must not be the register the IN writes.
    if (counters != 1U || address != body_end ||
        body.counter_register == destination)
    {
        return body;
    }
    body.valid = true;
    return body;
}

}  // namespace

bool ResolvePortIoDelayLoopEnabled(std::string_view setting)
{
    return !(setting == "0" || setting == "off" || setting == "false");
}

bool PortIoDelayLoopEnabled()
{
    static const bool enabled = [] {
        const char* value = std::getenv("REPIU_PORT_IO_DELAY_LOOP");
        return value == nullptr ||
            ResolvePortIoDelayLoopEnabled(std::string_view(value));
    }();
    return enabled;
}

const Win32PortIoDelayLoopStats& GetPortIoDelayLoopStats()
{
    return MutableStats();
}

bool TryBatchPortIoDelayLoop(std::uint32_t in_address,
                             std::uint32_t in_length,
                             std::uint32_t destination_width,
                             std::uint32_t* registers,
                             bool guest_bytes_readable)
{
    if (!PortIoDelayLoopEnabled() || registers == nullptr)
    {
        return false;
    }
    Win32PortIoDelayLoopStats& stats = MutableStats();
    stats.enabled = true;
    ++stats.attempt_count;
    if (!guest_bytes_readable)
    {
        CountOutcome(PortIoDelayLoopOutcome::kUnreadable);
        return false;
    }
    (void)destination_width;

    // After the IN: `cmp r32, imm8` (83 /7 ib) or `cmp r32, imm32` (81 /7 id),
    // then a backward conditional branch.
    const std::uint32_t compare_address = in_address + in_length;
    const std::uint8_t* compare = GuestBytes(compare_address);
    std::uint32_t branch_address = 0;
    std::uint32_t counter_register = 0;
    std::int64_t limit = 0;
    if (compare[0] == 0x83U && (compare[1] & 0xF8U) == 0xF8U)
    {
        counter_register = compare[1] & 0x07U;
        limit = static_cast<std::int8_t>(compare[2]);
        branch_address = compare_address + 3U;
    }
    else if (compare[0] == 0x81U && (compare[1] & 0xF8U) == 0xF8U)
    {
        counter_register = compare[1] & 0x07U;
        std::int32_t immediate = 0;
        std::memcpy(&immediate, compare + 2, sizeof(immediate));
        limit = immediate;
        branch_address = compare_address + 6U;
    }
    else
    {
        CountOutcome(PortIoDelayLoopOutcome::kShapeMismatch);
        return false;
    }

    const std::uint8_t* branch = GuestBytes(branch_address);
    const std::uint8_t condition = branch[0];
    // Signed "still below the limit" forms only. The unsigned forms would need
    // unsigned terminal-value arithmetic below, and getting that subtly wrong
    // would skip real iterations, so they are simply not matched.
    if (condition != 0x7CU && condition != 0x7EU)
    {
        CountOutcome(PortIoDelayLoopOutcome::kShapeMismatch);
        return false;
    }
    const std::int32_t displacement = static_cast<std::int8_t>(branch[1]);
    const std::uint32_t body_start =
        branch_address + 2U + static_cast<std::uint32_t>(displacement);
    // The caller guarantees readability of a fixed window before the IN, so a
    // body reaching further back is refused rather than decoded blind.
    if (displacement >= 0 || body_start >= in_address ||
        in_address - body_start > kMaxLoopBodyBytes)
    {
        CountOutcome(PortIoDelayLoopOutcome::kShapeMismatch);
        return false;
    }

    if (counter_register == kRegisterEax || counter_register == kRegisterEdx)
    {
        CountOutcome(PortIoDelayLoopOutcome::kRegisterConflict);
        return false;
    }

    const LoopBody body =
        DecodeLoopBody(body_start, in_address, kRegisterEax);
    if (!body.valid || body.counter_register != counter_register)
    {
        CountOutcome(PortIoDelayLoopOutcome::kShapeMismatch);
        return false;
    }
    if (!body.zeroes_destination)
    {
        // Without the zeroing the skipped reads could be observed, so the batch
        // is refused rather than assumed harmless.
        CountOutcome(PortIoDelayLoopOutcome::kResultNotDead);
        return false;
    }

    // The guest is between the IN and the compare, so the counter already holds
    // this iteration's value. The loop continues while the compare passes, and
    // one more iteration adds `step`, so the value that leaves exactly one
    // iteration is `limit - step`.
    const std::int64_t current =
        static_cast<std::int32_t>(registers[counter_register]);
    const std::int64_t final_iteration_value = limit - body.step;
    const std::int64_t remaining =
        (final_iteration_value - current) * body.step;
    if (remaining < 1)
    {
        CountOutcome(PortIoDelayLoopOutcome::kNothingToSkip);
        return false;
    }

    registers[counter_register] =
        static_cast<std::uint32_t>(static_cast<std::int32_t>(
            final_iteration_value));
    ++stats.batch_count;
    stats.skipped_iteration_count += static_cast<std::uint64_t>(remaining);
    if (static_cast<std::uint32_t>(remaining) > stats.max_skipped_iterations)
    {
        stats.max_skipped_iterations = static_cast<std::uint32_t>(remaining);
    }
    stats.last_loop_address = body_start;
    stats.last_limit = static_cast<std::uint32_t>(limit);
    CountOutcome(PortIoDelayLoopOutcome::kBatched);
    return true;
}

}  // namespace repiu::platform::win32
