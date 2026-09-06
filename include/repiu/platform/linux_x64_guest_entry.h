#ifndef REPIU_PLATFORM_LINUX_X64_GUEST_ENTRY_H_
#define REPIU_PLATFORM_LINUX_X64_GUEST_ENTRY_H_

// Task 578. How an x86-64 host enters the guest.
//
// It does not enter the guest's own bytes. `IsDirectX86ExecutionSupported`
// stays false on x64 and means what it says -- long mode reads several of those
// encodings differently (Task 550) -- so what is entered is the *emitted*
// long-mode code cache, at the placement's entry address.
//
// The state block is indexed by x86 register number, offset 4*n, so its layout
// is Task 558's mapping written down once. Guest ESP is the exception the
// mapping already names: it lives in R15 rather than in host RSP, which stays
// the SysV stack (Task 546 decision 3), so it is passed and returned in its own
// field rather than in `gpr[4]`.
//
// The offsets are `#define`d because the assembly reads them too, and the
// static assertions below are what keep the two from drifting -- the same
// arrangement `linux_x64_aot_frame.h` uses for the dispatch frame.
//
// docs/design/20260903-578-x64-guest-entry.md

#define REPIU_X64_ENTRY_EAX 0
#define REPIU_X64_ENTRY_ECX 4
#define REPIU_X64_ENTRY_EDX 8
#define REPIU_X64_ENTRY_EBX 12
#define REPIU_X64_ENTRY_EBP 20
#define REPIU_X64_ENTRY_ESI 24
#define REPIU_X64_ENTRY_EDI 28
#define REPIU_X64_ENTRY_GUEST_ESP 32

#if !defined(__ASSEMBLER__)

#include <cstddef>
#include <cstdint>

namespace repiu::platform
{

struct LinuxX64GuestEntryState
{
    // Guest EAX..EDI by x86 register number. Index 4 is unused: guest ESP is
    // `guest_esp` below, because host RSP is not it.
    std::uint32_t gpr[8] = {};
    // Guest ESP on the way in, and whatever the run left in R15 on the way out.
    // 64 bits wide so a run that put rubbish in R15's upper half is visible
    // rather than truncated away -- Task 558 keeps that half zero, and a caller
    // that could not see it could not check the invariant.
    std::uint64_t guest_esp = 0;
};

static_assert(offsetof(LinuxX64GuestEntryState, gpr) == REPIU_X64_ENTRY_EAX);
static_assert(offsetof(LinuxX64GuestEntryState, guest_esp) ==
              REPIU_X64_ENTRY_GUEST_ESP);
static_assert(sizeof(LinuxX64GuestEntryState::gpr[0]) == 4U);

// Loads the state, enters `code` -- a host address inside the placed cache --
// and writes the state back when the run returns. Returns only when the guest
// leaves the cache normally; a boundary that cannot be serviced ends the run
// through the fault handler instead.
extern "C" void RepiuLinuxX64GuestEntry(void* code,
                                        LinuxX64GuestEntryState* state);

// Entered by a Linux signal return, not called by C++. Its ret consumes the
// host return address left by RepiuLinuxX64GuestEntry's call.
extern "C" void RepiuLinuxX64GuestExit();

}  // namespace repiu::platform

#endif  // !defined(__ASSEMBLER__)

#endif  // REPIU_PLATFORM_LINUX_X64_GUEST_ENTRY_H_
