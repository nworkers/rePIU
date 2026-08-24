#ifndef REPIU_PLATFORM_THUNK_CALLING_CONVENTION_H_
#define REPIU_PLATFORM_THUNK_CALLING_CONVENTION_H_

// Task 503d-11. The calling convention the dispatch thunks call their resolvers
// with.
//
// Each of the five thunks pushes two arguments and calls a resolver declared
// `__stdcall`. That keyword is MSVC's; GCC spells the same convention as an
// attribute. Naming it once means the five declarations read the same on both
// hosts instead of carrying a conditional apiece.
//
// The convention is stdcall on both rather than cdecl on Linux, even though the
// thunk restores the stack pointer itself and so would survive either. Two
// reasons: the declaration and the definition have to agree with each other,
// and keeping the Windows convention means the assembly that calls it is the
// same assembly, which is the property this whole port is built on.
//
// Meaningless on anything but 32-bit x86, where the engine runs by
// construction; elsewhere it expands to nothing so the declaration still
// compiles.

#if defined(_MSC_VER)
#define REPIU_THUNK_RESOLVER_CALL __stdcall
#elif (defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
#define REPIU_THUNK_RESOLVER_CALL __attribute__((stdcall))
#else
#define REPIU_THUNK_RESOLVER_CALL
#endif

#endif  // REPIU_PLATFORM_THUNK_CALLING_CONVENTION_H_
