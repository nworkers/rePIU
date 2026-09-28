#ifndef REPIU_PLATFORM_WIN32_ATOMIC_OPS_WIN32_H_
#define REPIU_PLATFORM_WIN32_ATOMIC_OPS_WIN32_H_

// Task 758. The MSVC half of atomic_ops.h: the Interlocked intrinsics.
// Include atomic_ops.h, not this.

// The intrinsics rather than <windows.h>: this header is included from
// platform-neutral positions and must not drag the Win32 headers along.
#include <intrin.h>

#include <cstdint>

namespace repiu::platform
{

inline long AtomicIncrement(volatile long* target)
{
    return _InterlockedIncrement(target);
}

inline long AtomicExchange(volatile long* target, const long value)
{
    return _InterlockedExchange(target, value);
}

inline long AtomicExchangeAdd(volatile long* target, const long addend)
{
    return _InterlockedExchangeAdd(target, addend);
}

inline std::int32_t AtomicIncrement(volatile std::int32_t* target)
{
    return static_cast<std::int32_t>(_InterlockedIncrement(
        reinterpret_cast<volatile long*>(target)));
}

inline std::int32_t AtomicExchange(volatile std::int32_t* target,
                                   const std::int32_t value)
{
    return static_cast<std::int32_t>(_InterlockedExchange(
        reinterpret_cast<volatile long*>(target), static_cast<long>(value)));
}

inline std::int32_t AtomicExchangeAdd(volatile std::int32_t* target,
                                      const std::int32_t addend)
{
    return static_cast<std::int32_t>(_InterlockedExchangeAdd(
        reinterpret_cast<volatile long*>(target), static_cast<long>(addend)));
}

inline std::uint32_t AtomicIncrement(volatile std::uint32_t* target)
{
    return static_cast<std::uint32_t>(_InterlockedIncrement(
        reinterpret_cast<volatile long*>(target)));
}

inline std::uint32_t AtomicExchange(volatile std::uint32_t* target,
                                    const std::uint32_t value)
{
    return static_cast<std::uint32_t>(_InterlockedExchange(
        reinterpret_cast<volatile long*>(target),
        static_cast<long>(static_cast<std::int32_t>(value))));
}

inline std::uint32_t AtomicExchangeAdd(volatile std::uint32_t* target,
                                       const std::uint32_t addend)
{
    return static_cast<std::uint32_t>(_InterlockedExchangeAdd(
        reinterpret_cast<volatile long*>(target),
        static_cast<long>(static_cast<std::int32_t>(addend))));
}

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_WIN32_ATOMIC_OPS_WIN32_H_
