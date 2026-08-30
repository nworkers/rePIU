#ifndef REPIU_PLATFORM_ATOMIC_OPS_H_
#define REPIU_PLATFORM_ATOMIC_OPS_H_

// Task 503d-3. Atomic read-modify-write on plain integer words.
//
// The engine already uses std::atomic for its own counters. These are for the
// ones it cannot: `SharedLiveTelemetry` is a fixed layout mapped by a second
// process, so its fields are volatile fixed-width words. Wrapping them in
// std::atomic would change what that other process reads.
//
// The operations are named here and retain the semantics of Windows' Interlocked
// family exactly, because that is what the 153 call sites were written against.
// The overloads for fixed-width words keep shared-memory accesses at four bytes
// even when the host's `long` is wider:
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

#include <cstdint>

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

inline std::int32_t AtomicIncrement(volatile std::int32_t* target)
{
#if defined(_MSC_VER)
    return static_cast<std::int32_t>(_InterlockedIncrement(
        reinterpret_cast<volatile long*>(target)));
#else
    return __atomic_add_fetch(
        target, static_cast<std::int32_t>(1), __ATOMIC_SEQ_CST);
#endif
}

inline std::int32_t AtomicExchange(volatile std::int32_t* target,
                                   const std::int32_t value)
{
#if defined(_MSC_VER)
    return static_cast<std::int32_t>(_InterlockedExchange(
        reinterpret_cast<volatile long*>(target), static_cast<long>(value)));
#else
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
#endif
}

inline std::int32_t AtomicExchangeAdd(volatile std::int32_t* target,
                                      const std::int32_t addend)
{
#if defined(_MSC_VER)
    return static_cast<std::int32_t>(_InterlockedExchangeAdd(
        reinterpret_cast<volatile long*>(target), static_cast<long>(addend)));
#else
    return __atomic_fetch_add(target, addend, __ATOMIC_SEQ_CST);
#endif
}

inline std::uint32_t AtomicIncrement(volatile std::uint32_t* target)
{
#if defined(_MSC_VER)
    return static_cast<std::uint32_t>(_InterlockedIncrement(
        reinterpret_cast<volatile long*>(target)));
#else
    return __atomic_add_fetch(
        target, static_cast<std::uint32_t>(1), __ATOMIC_SEQ_CST);
#endif
}

inline std::uint32_t AtomicExchange(volatile std::uint32_t* target,
                                    const std::uint32_t value)
{
#if defined(_MSC_VER)
    return static_cast<std::uint32_t>(_InterlockedExchange(
        reinterpret_cast<volatile long*>(target),
        static_cast<long>(static_cast<std::int32_t>(value))));
#else
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
#endif
}

inline std::uint32_t AtomicExchangeAdd(volatile std::uint32_t* target,
                                       const std::uint32_t addend)
{
#if defined(_MSC_VER)
    return static_cast<std::uint32_t>(_InterlockedExchangeAdd(
        reinterpret_cast<volatile long*>(target),
        static_cast<long>(static_cast<std::int32_t>(addend))));
#else
    return __atomic_fetch_add(target, addend, __ATOMIC_SEQ_CST);
#endif
}

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_ATOMIC_OPS_H_
