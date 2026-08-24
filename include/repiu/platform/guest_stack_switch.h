#ifndef REPIU_PLATFORM_GUEST_STACK_SWITCH_H_
#define REPIU_PLATFORM_GUEST_STACK_SWITCH_H_

// Task 503d-16. The layout and the storage that the cross-stack assembly reads
// by name, in the one place both hosts read it from.
//
// The engine enters guest code by switching stacks in hand-written assembly:
// MSVC inline assembly on Windows, GAS on Linux. Two implementations of the
// same routine means two transcriptions of the same field offsets, and a wrong
// offset there does not announce itself -- it stores the guest's return stack
// pointer over the result code and the engine reports success for a run that
// never happened.
//
// So the offsets are defined once here. A `.S` goes through the C preprocessor,
// which is what lets the assembler and the C++ `static_assert`s below read the
// same numbers; the asserts are what tie those numbers back to the structure.

#define REPIU_STACK_SWITCH_ENTRY_ADDRESS 0
#define REPIU_STACK_SWITCH_INITIAL_ESP 4
#define REPIU_STACK_SWITCH_HOST_ESP 8
#define REPIU_STACK_SWITCH_GUEST_RETURN_ESP 12
#define REPIU_STACK_SWITCH_RESULT_CODE 16
#define REPIU_STACK_SWITCH_SINGLE_STEP 20
#define REPIU_STACK_SWITCH_HOST_FS 24
#define REPIU_STACK_SWITCH_HOST_DS 28
#define REPIU_STACK_SWITCH_HOST_ES 32
#define REPIU_STACK_SWITCH_HOST_GS 36
#define REPIU_STACK_SWITCH_HOST_SS 40
#define REPIU_STACK_SWITCH_GUEST_STACK_BASE 44
#define REPIU_STACK_SWITCH_GUEST_STACK_LIMIT 48
#define REPIU_STACK_SWITCH_HOST_STACK_BASE 52
#define REPIU_STACK_SWITCH_HOST_STACK_LIMIT 56

// EFLAGS.TF. Set on the guest stack before the call when a single-step trace is
// armed, so the first guest instruction already traps.
#define REPIU_STACK_SWITCH_TRAP_FLAG 0x100

// What `CallGuestEntryWithStack` leaves in EAX. Zero when the guest returned of
// its own accord; the recovery entry substitutes itself for the return and
// answers with the other value.
#define REPIU_STACK_SWITCH_RETURNED 0
#define REPIU_STACK_SWITCH_RECOVERED 2

#if !defined(__ASSEMBLER__)

#include <cstdint>

// The assembly addresses these directly rather than through a parameter, so
// they are plain C symbols with external linkage.
//
// Six of them are the host's own register and stack state, parked outside
// thread-local storage on purpose: a guest that has changed the FS selector
// makes every compiler-generated TLS access unsafe, and recovery is exactly
// when that state has to be readable.
//
// Two of the pairs are read only by the Windows assembly, which swaps the TIB's
// stack bounds around a call the kernel might deliver an exception on. Linux
// keeps no such record and leaves them zero -- the asymmetry 3d-12 established
// by taking a fault on a switched stack and watching it resume regardless.
extern "C" {

extern std::uint32_t g_recovery_host_fs;
extern std::uint32_t g_recovery_host_ds;
extern std::uint32_t g_recovery_host_es;
extern std::uint32_t g_recovery_host_gs;
extern std::uint32_t g_recovery_host_stack_base;
extern std::uint32_t g_recovery_host_stack_limit;

extern std::uint32_t g_repiu_dbt_host_esp;
extern std::uint32_t g_repiu_dbt_host_stack_base;
extern std::uint32_t g_repiu_dbt_host_stack_limit;
extern std::uint32_t g_repiu_dbt_guest_stack_base;
extern std::uint32_t g_repiu_dbt_guest_stack_limit;

}  // extern "C"

#endif  // !__ASSEMBLER__

#endif  // REPIU_PLATFORM_GUEST_STACK_SWITCH_H_
