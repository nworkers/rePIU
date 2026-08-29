#ifndef REPIU_PLATFORM_ATOMIC_OPS_H_
#define REPIU_PLATFORM_ATOMIC_OPS_H_

// Task 503d-3. Atomic read-modify-write on plain `long`.
//
// The engine already uses std::atomic for its own counters. These are for the
// ones it cannot: `SharedLiveTelemetry` is a fixed layout mapped by a
// second process, so its fields are `volatile long` and have to stay that way.
// Wrapping them in std::atomic would change what that other process reads.
//
// So the operations are named here and the field type is left alone. The
// semantics are Windows' Interlocked family exactly, because that is what the
// 153 call sites were written against:
//
//   AtomicIncrement    returns the value *after* incrementing
//   AtomicExchange     returns the value *before* the store
//   AtomicExchangeAdd  returns the value *before* the addition
//
// Both implementations are inline. These sit on boundary counters that run
// millions of times, and a function call around a locked instruction would be
// pure overhead.
//
// See docs/design/20260822-503-linux-execution-engine.md.

#if defined(_MSC_VER)
// The intrinsics rather than <windows.h>: this header is included from
// platform-neutral positions and must not drag the Win32 headers along.
#include <intrin.h>
#endif

namespace repiu::platform
{

inline long AtomicIncrement(volatile long* target)
{
#if defined(_MSC_VER)
    return _InterlockedIncrement(target);
#else
    return __atomic_add_fetch(target, 1L, __ATOMIC_SEQ_CST);
#endif
}

inline long AtomicExchange(volatile long* target, const long value)
{
#if defined(_MSC_VER)
    return _InterlockedExchange(target, value);
#else
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
#endif
}

inline long AtomicExchangeAdd(volatile long* target, const long addend)
{
#if defined(_MSC_VER)
    return _InterlockedExchangeAdd(target, addend);
#else
    return __atomic_fetch_add(target, addend, __ATOMIC_SEQ_CST);
#endif
}

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_ATOMIC_OPS_H_
