#include "port_io_delay_loop.h"

#include "../cpu_emul/guest_memory_access.h"

#include <cstdlib>
#include <cstring>

namespace repiu::engine
{
namespace
{

constexpr std::uint32_t kRegisterEax = 0U;
constexpr std::uint32_t kRegisterEdx = 2U;
constexpr std::uint32_t kRegisterEsp = 4U;

enum class WrappedLoopResult
{
    kNotCandidate,
    kRefused,
    kBatched,
};

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

WrappedLoopResult TryBatchWrappedCallLoop(ThreadContext* context,
                                          std::uint32_t in_address,
                                          std::uint32_t in_length,
                                          std::uint32_t* registers)
{
    // Restricted OpenWatcom-style input wrapper. The check is semantic and
    // address-independent: preserve EDX, take the port in EAX, clear the input
    // destination, execute IN, restore EDX, and return.
    if (context == nullptr || registers == nullptr || in_address < 5U ||
        (in_length != 1U && in_length != 2U))
    {
        return WrappedLoopResult::kNotCandidate;
    }
    const std::uint32_t wrapper_start = in_address - 5U;
    if (!IsGuestRangeReadable(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(wrapper_start)),
            5U + in_length + 2U))
    {
        return WrappedLoopResult::kNotCandidate;
    }
    const std::uint8_t* wrapper = GuestBytes(wrapper_start);
    const bool zeroes_eax =
        (wrapper[3] == 0x29U || wrapper[3] == 0x31U) &&
        wrapper[4] == 0xC0U;
    const std::uint8_t* input = wrapper + 5U;
    const bool word_input = in_length == 2U &&
        input[0] == 0x66U && input[1] == 0xEDU;
    const bool other_input = in_length == 1U &&
        (input[0] == 0xECU || input[0] == 0xEDU);
    if (wrapper[0] != 0x52U || wrapper[1] != 0x89U ||
        wrapper[2] != 0xC2U || !zeroes_eax ||
        (!word_input && !other_input) ||
        input[in_length] != 0x5AU || input[in_length + 1U] != 0xC3U)
    {
        return WrappedLoopResult::kNotCandidate;
    }

    Win32PortIoDelayLoopStats& stats = MutableStats();
    ++stats.wrapped_candidate_count;

    // The wrapper's PUSH EDX leaves the caller's counter at [ESP] and the
    // CALL return address at [ESP+4]. Advancing the saved value means the
    // original POP restores the optimized counter without synthesizing EIP or
    // flags.
    const std::uint32_t stack_address = registers[kRegisterEsp];
    std::uint32_t saved_edx = 0U;
    std::uint32_t return_address = 0U;
    if (!ReadGuestUInt32(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(stack_address)),
            &saved_edx) ||
        !ReadGuestUInt32(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(stack_address + 4U)),
            &return_address) ||
        return_address < 5U)
    {
        CountOutcome(PortIoDelayLoopOutcome::kUnreadable);
        return WrappedLoopResult::kRefused;
    }
    stats.last_wrapped_return_address = return_address;

    const std::uint32_t call_address = return_address - 5U;
    if (!IsGuestRangeReadable(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(call_address)),
            5U + kMaxLoopTailBytes))
    {
        CountOutcome(PortIoDelayLoopOutcome::kUnreadable);
        return WrappedLoopResult::kRefused;
    }
    const std::uint8_t* call = GuestBytes(call_address);
    std::int32_t call_displacement = 0;
    std::memcpy(&call_displacement, call + 1U, sizeof(call_displacement));
    const std::uint32_t call_target = return_address +
        static_cast<std::uint32_t>(call_displacement);
    if (call[0] != 0xE8U || call_target != wrapper_start)
    {
        CountOutcome(PortIoDelayLoopOutcome::kShapeMismatch);
        return WrappedLoopResult::kRefused;
    }

    // Supported caller tail: INC EDX; CMP EDX,imm; JL body. EDX is the value
    // the wrapper saved, while EAX is overwritten by MOV EAX,imm at the loop
    // head before every skipped input, proving each skipped result dead.
    const std::uint8_t* tail = GuestBytes(return_address);
    if (tail[0] != 0x42U)
    {
        CountOutcome(PortIoDelayLoopOutcome::kShapeMismatch);
        return WrappedLoopResult::kRefused;
    }
    std::uint32_t branch_offset = 0U;
    std::int64_t limit = 0;
    if (tail[1] == 0x83U && tail[2] == 0xFAU)
    {
        limit = static_cast<std::int8_t>(tail[3]);
        branch_offset = 4U;
    }
    else if (tail[1] == 0x81U && tail[2] == 0xFAU)
    {
        std::int32_t immediate = 0;
        std::memcpy(&immediate, tail + 3U, sizeof(immediate));
        limit = immediate;
        branch_offset = 7U;
    }
    else
    {
        CountOutcome(PortIoDelayLoopOutcome::kShapeMismatch);
        return WrappedLoopResult::kRefused;
    }
    if (tail[branch_offset] != 0x7CU)
    {
        CountOutcome(PortIoDelayLoopOutcome::kShapeMismatch);
        return WrappedLoopResult::kRefused;
    }
    const std::int32_t branch_displacement =
        static_cast<std::int8_t>(tail[branch_offset + 1U]);
    const std::uint32_t branch_end =
        return_address + branch_offset + 2U;
    const std::uint32_t body_start = branch_end +
        static_cast<std::uint32_t>(branch_displacement);
    if (branch_displacement >= 0 || body_start + 5U != call_address ||
        call_address - body_start > kMaxLoopBodyBytes ||
        !IsGuestRangeReadable(
            context,
            reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(body_start)),
            5U))
    {
        CountOutcome(PortIoDelayLoopOutcome::kShapeMismatch);
        return WrappedLoopResult::kRefused;
    }
    const std::uint8_t* body = GuestBytes(body_start);
    std::uint32_t port_immediate = 0U;
    std::memcpy(&port_immediate, body + 1U, sizeof(port_immediate));
    if (body[0] != 0xB8U ||
        static_cast<std::uint16_t>(port_immediate) !=
            static_cast<std::uint16_t>(registers[kRegisterEdx]))
    {
        CountOutcome(PortIoDelayLoopOutcome::kResultNotDead);
        return WrappedLoopResult::kRefused;
    }

    // Current input has already executed. Setting the saved counter to
    // limit-2 makes the caller's INC produce limit-1, takes the branch once,
    // and lets the guest execute the final input at limit-1 before exiting.
    const std::int64_t current = static_cast<std::int32_t>(saved_edx);
    const std::int64_t final_saved_value = limit - 2;
    const std::int64_t skipped = final_saved_value - current;
    if (skipped < 1)
    {
        CountOutcome(PortIoDelayLoopOutcome::kNothingToSkip);
        return WrappedLoopResult::kRefused;
    }
    if (!WriteGuestUInt32(
            context,
            reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(stack_address)),
            static_cast<std::uint32_t>(
                static_cast<std::int32_t>(final_saved_value))))
    {
        CountOutcome(PortIoDelayLoopOutcome::kUnreadable);
        return WrappedLoopResult::kRefused;
    }

    ++stats.batch_count;
    ++stats.wrapped_batch_count;
    stats.skipped_iteration_count += static_cast<std::uint64_t>(skipped);
    if (static_cast<std::uint32_t>(skipped) > stats.max_skipped_iterations)
    {
        stats.max_skipped_iterations = static_cast<std::uint32_t>(skipped);
    }
    stats.last_loop_address = body_start;
    stats.last_limit = static_cast<std::uint32_t>(limit);
    CountOutcome(PortIoDelayLoopOutcome::kBatched);
    return WrappedLoopResult::kBatched;
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

bool TryBatchPortIoDelayLoop(ThreadContext* context,
                             std::uint32_t in_address,
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
        const WrappedLoopResult wrapped = TryBatchWrappedCallLoop(
            context, in_address, in_length, registers);
        if (wrapped != WrappedLoopResult::kNotCandidate)
        {
            return wrapped == WrappedLoopResult::kBatched;
        }
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

}  // namespace repiu::engine
