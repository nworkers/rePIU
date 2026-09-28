#ifndef REPIU_PLATFORM_LINUX_X64_GUEST_REGISTERS_H_
#define REPIU_PLATFORM_LINUX_X64_GUEST_REGISTERS_H_

// Task 558. Where guest state lives while emitted long-mode code runs.
//
// Task 555 recorded this as an unwritten premise -- "guest GPR n is in host
// GPR n when a lowered instruction runs" -- and noted that it would become a
// decision once the mapping was settled. This header is that decision, written
// so the code states it rather than a comment somewhere claiming it.
//
// docs/design/20260901-558-x64-guest-register-placement.md

#include <cstdint>

namespace repiu::platform
{

// The identity, and forced rather than chosen: `kIdenticalBytes` exists, so
// `add eax, ebx` is copied byte for byte and can only be correct when the guest
// registers it names are the host registers of the same number. Any other
// mapping would mean no guest instruction could ever be copied.
//
// Encoded as the x86 register numbers both ISAs already use, so "the mapping is
// the identity" is expressible as an assertion rather than a table.
enum class GuestGprNumber : std::uint8_t
{
    kEax = 0,
    kEcx = 1,
    kEdx = 2,
    kEbx = 3,
    kEsp = 4,
    kEbp = 5,
    kEsi = 6,
    kEdi = 7,
};

// Guest ESP is the one register that cannot take the identity: host RSP is the
// SysV stack and stays that way (Task 546 decision 3). It lives in R15 instead.
//
// An extended register rather than a slot in the frame, for two reasons that
// memory cannot offer:
//
//   * A 32-bit encoding cannot name R8-R15 at all, because naming them needs a
//     REX prefix and 32-bit mode has no REX -- those bytes are INC/DEC there
//     (Task 557). So no copied guest instruction can reach guest ESP's home by
//     any encoding. A frame slot has no such guarantee: a guest instruction
//     with an absolute address could in principle name it.
//   * R12-R15 are callee-saved, so the SysV ABI preserves guest ESP across a
//     resolver call without the bridge saving anything. R8-R11 are
//     caller-saved and would need it at every crossing.
//
// R15 rather than R12 or R13 because those two carry encoding exceptions as a
// base -- R12 needs a SIB byte, R13 needs a disp8 at mod=00, the same quirks
// RSP and RBP have. R14 is left free for whatever needs the next reservation.
inline constexpr std::uint8_t kGuestEspHostRegister = 15;

// Task 559. The emitter's scratch, which Task 558 left free.
//
// The stack sequences need somewhere for an intermediate -- PUSHFD has to get
// the flags from the host stack into guest memory, and `push esp` has to keep
// the pre-decrement value -- and all eight guest registers are spoken for by
// the mapping above.
//
// R14 is safe for exactly the reason R15 is: no 32-bit encoding can name it,
// so no copied guest instruction can be holding anything there.
inline constexpr std::uint8_t kEmitterScratchHostRegister = 14;

static_assert(kEmitterScratchHostRegister >= 8);
static_assert(kEmitterScratchHostRegister != kGuestEspHostRegister);

// The invariant that makes addressing through it correct.
//
// An access through guest ESP is emitted as `[r15]`, and there the *whole*
// 64-bit register is the address -- unlike a `0x67`-prefixed operand, which
// computes in 32 bits and ignores the base's upper half entirely. So R15's
// upper half must be zero, and it is kept so by writing it only with 32-bit
// operations. Task 559 makes them `lea r15d, [r15 - 4]` rather than a `sub`,
// because guest PUSH and POP change no flags -- either way the 32-bit
// destination zeroes the upper half in hardware and reproduces the guest's own
// 32-bit wraparound.
inline constexpr bool kGuestEspUpperHalfIsZero = true;

// True when this guest register is held in the host register of the same
// number. Every register except ESP.
[[nodiscard]] inline constexpr bool GuestGprUsesHostRegisterOfSameNumber(
    const GuestGprNumber guest)
{
    return guest != GuestGprNumber::kEsp;
}

// The host register number holding a guest register, which is the identity
// everywhere except ESP.
[[nodiscard]] inline constexpr std::uint8_t HostRegisterForGuestGpr(
    const GuestGprNumber guest)
{
    return guest == GuestGprNumber::kEsp
        ? kGuestEspHostRegister
        : static_cast<std::uint8_t>(guest);
}

static_assert(HostRegisterForGuestGpr(GuestGprNumber::kEax) == 0);
static_assert(HostRegisterForGuestGpr(GuestGprNumber::kEdi) == 7);
// The one that is not the identity, asserted by name so a change to the
// mapping cannot pass as a typo.
static_assert(HostRegisterForGuestGpr(GuestGprNumber::kEsp) == 15);
static_assert(!GuestGprUsesHostRegisterOfSameNumber(GuestGprNumber::kEsp));
// R15 must stay outside the range a 32-bit encoding can name, which is what
// makes the reservation safe from copied guest bytes.
static_assert(kGuestEspHostRegister >= 8);

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_LINUX_X64_GUEST_REGISTERS_H_
