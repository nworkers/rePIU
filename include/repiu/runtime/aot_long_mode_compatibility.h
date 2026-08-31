#ifndef REPIU_RUNTIME_AOT_LONG_MODE_COMPATIBILITY_H_
#define REPIU_RUNTIME_AOT_LONG_MODE_COMPATIBILITY_H_

#include <cstddef>
#include <cstdint>

namespace repiu::runtime
{

// Task 550. Whether a guest instruction's own 32-bit bytes may be executed by an
// x86-64 host, which is the question `kCopy` answers by assuming on i386.
//
// On an i386 host, copying the guest's bytes into the code cache is the identity
// and needs no argument. On x86-64 it is not, and the failures do not all
// announce themselves: some of these encodings are simply gone from long mode
// and raise #UD, but others decode as a *different instruction* and run. A one
// byte `inc eax` becomes a REX prefix on whatever follows it; an absolute
// `disp32` becomes RIP-relative. Neither raises anything.
//
// So this is written fail-closed. `kIdenticalBytes` is returned only where
// identity is shown; everything unproven is refused. The design lists what is
// known to diverge and cites the manuals:
// docs/design/20260831-550-linux-x64-long-mode-byte-compatibility.md
enum class LongModeByteCompatibility
{
    // Not shown to be identical. The default, and what an instruction nobody
    // has looked at gets.
    kUnsupported,
    // The meaning survives but the encoding does not: the same operation at a
    // different width, which the x64 emitter has to lower for itself. Every
    // stack instruction is here, because long mode has no 32-bit PUSH or POP.
    kNeedsReencode,
    // The bytes mean the same thing in long mode and may be copied.
    kIdenticalBytes,
};

// Why an instruction was refused. Reported separately from the verdict because
// "quietly a different instruction" and "does not exist" are different risks and
// a probe that folds them together hides the one worth fearing.
enum class LongModeDivergence
{
    kNone,
    // Decodes as a different instruction in long mode, without raising.
    kSilentlyDifferent,
    // Removed from long mode; raises #UD.
    kInvalidInLongMode,
    // Same operation, 64-bit width, because long mode offers no 32-bit form.
    kOperandWidth,
    // Has a memory operand. Long mode's default address size is 64, so the
    // bytes address through 64-bit registers; a 0x67 prefix restores 32-bit
    // computation but only matches while the target lives below 4 GiB, which is
    // a placement decision this classifier does not get to make.
    kAddressSize,
    // ModRM mod=00 rm=101: absolute disp32 in 32-bit mode, RIP-relative in long
    // mode. Named apart from `kAddressSize` because it is an addressing form
    // rather than a property of the opcode, and because it is the single most
    // common silent divergence in code that reads globals.
    kRipRelativeDisplacement,
    // Reaches a segment register, whose meaning long mode changes.
    kSegmentRegister,
    // Task 555. Names the stack pointer, in any role. Task 546's decision 3
    // keeps host RSP as the SysV stack and holds guest ESP as state, so in
    // long mode `ESP` is the low half of the *host's* stack pointer and not
    // the guest's stack at all.
    //
    // Named apart from the others because it is the one register the project
    // has already decided is wrong, rather than one it has not yet decided is
    // right -- and because the two shapes differ in cost. Reading through it
    // (`mov eax,[esp+8]`) yields wrong data; writing it (`add esp,16`)
    // destroys the host stack pointer.
    kStackPointerRegister,
};

// Task 552. How a `kNeedsReencode` instruction is to be rewritten, where this
// unit knows how.
//
// Naming the transform rather than only the problem is what keeps the
// judgement and the rewrite from drifting apart: `LowerLongModeBytes` below
// produces exactly what the verdict promises, so a classifier that said one
// thing and an emitter that did another cannot both be believed.
//
// **The premise every lowering here stands on** (written down in Task 555,
// because leaving it unwritten is what produced the stack-pointer hole):
//
//     At the moment a lowered instruction runs, guest GPR *n* is in host
//     GPR *n*.
//
// That is what makes a `0x67` prefix mean anything. Without the premise that
// `[ebx+4]` addresses through the guest's `EBX`, prefixing it is pointless.
//
// The x64 emitter's register mapping is **not decided yet**, so for most
// registers this is undecided rather than established -- and the one register
// the project has already decided *against* is the stack pointer, which
// `kStackPointerRegister` above now refuses. When the mapping is settled, this
// premise stops being a premise and becomes a decision to cite.
enum class LongModeLowering
{
    // No rewrite is described. Either none is needed or this unit does not know
    // one -- the verdict says which.
    kNone,
    // Prepend a 0x67 address-size prefix. Long mode's default address size is
    // 64, so the unprefixed bytes read a 64-bit register as the base; with the
    // prefix the address is computed in 32 bits and zero-extended, which is the
    // guest's own arithmetic and its wraparound. Correct only because guest
    // memory is placed below 4 GiB, which Task 546's decision 4 now settles.
    kAddressSizePrefix,
    // Prepend 0x67 *and* rewrite ModRM into the SIB absolute form. `mod=00,
    // rm=101` is RIP-relative in long mode and a prefix does not turn that off
    // -- it only truncates, leaving EIP-relative, which is still relative. An
    // absolute address is expressible only as `mod=00, rm=100` with SIB
    // `base=101, index=100`.
    kAbsoluteToSib,
    // Task 557. Move the register out of the opcode and into a ModRM byte:
    // `40+r` becomes `FF /0` and `48+r` becomes `FF /1`, both with `mod=11`.
    //
    // The single-byte forms are the whole of long mode's REX prefix range, so
    // copying one deletes an instruction and rewrites its successor. The group
    // form is the same operation, touches the same flags, and exists unchanged
    // in both modes -- and `NeedsWidthReencode` already lets `FF /0` and `/1`
    // through, filtering only `/2` and `/3`.
    kIncDecToModRm,
    // Task 559. A stack instruction becomes a sequence that names guest ESP in
    // R15D, because long mode has no 32-bit PUSH or POP and a 0x66 prefix asks
    // for 16 bits rather than 32 -- there is no encoding that gets back.
    //
    // The ESP adjustment inside these is always a LEA, never SUB or ADD: guest
    // PUSH and POP change no flags, and an arithmetic adjustment would quietly
    // change the ones the guest's next branch reads.
    kStackSequence,
};

struct LongModeCompatibilityResult
{
    LongModeByteCompatibility compatibility =
        LongModeByteCompatibility::kUnsupported;
    LongModeDivergence divergence = LongModeDivergence::kNone;
    LongModeLowering lowering = LongModeLowering::kNone;
};

// Decodes `bytes` as a 32-bit instruction and judges it. A sequence that does
// not decode is `kUnsupported`, not an error: the caller is asking whether it
// may copy these bytes, and "they are not an instruction" is a "no".
[[nodiscard]] LongModeCompatibilityResult ClassifyLongModeBytes(
    const std::uint8_t* bytes, std::size_t byte_count);

// Produces the lowered bytes for an instruction `ClassifyLongModeBytes` named a
// lowering for. Writes at most `kMaxLoweredBytes` and reports how many.
//
// Returns false for anything it was not given a lowering for, which includes
// every `kIdenticalBytes` instruction: those are copied rather than lowered,
// and asking this function to "lower" one would blur the distinction the
// classifier exists to draw.
inline constexpr std::size_t kMaxLoweredBytes = 24;

// `instruction_count` reports how many instructions the lowered bytes are, and
// may be null when the caller does not care. It exists because Task 553's
// verification checks that an emitted entry decodes to the number of
// instructions the emitter meant -- the check that catches a byte string of the
// right length that decodes as something else. A stack sequence is several
// instructions, so "one" stopped being the answer in Task 559 and the emitter
// has to be told the real one rather than the rule being dropped.
[[nodiscard]] bool LowerLongModeBytes(const std::uint8_t* bytes,
                                      std::size_t byte_count,
                                      std::uint8_t* lowered,
                                      std::size_t* lowered_count,
                                      std::size_t* instruction_count = nullptr);

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_AOT_LONG_MODE_COMPATIBILITY_H_
