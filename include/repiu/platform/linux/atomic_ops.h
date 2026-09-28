#ifndef REPIU_PLATFORM_LINUX_ATOMIC_OPS_H_
#define REPIU_PLATFORM_LINUX_ATOMIC_OPS_H_

// Task 758. The GCC/Clang half of atomic_ops.h: the __atomic builtins, used by
// the Linux and web builds. Include atomic_ops.h, not this.

#include <cstdint>

namespace repiu::platform
{

inline long AtomicIncrement(volatile long* target)
{
    return __atomic_add_fetch(target, 1L, __ATOMIC_SEQ_CST);
}

inline long AtomicExchange(volatile long* target, const long value)
{
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}

inline long AtomicExchangeAdd(volatile long* target, const long addend)
{
    return __atomic_fetch_add(target, addend, __ATOMIC_SEQ_CST);
}

inline std::int32_t AtomicIncrement(volatile std::int32_t* target)
{
    return __atomic_add_fetch(
        target, static_cast<std::int32_t>(1), __ATOMIC_SEQ_CST);
}

inline std::int32_t AtomicExchange(volatile std::int32_t* target,
                                   const std::int32_t value)
{
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}

inline std::int32_t AtomicExchangeAdd(volatile std::int32_t* target,
                                      const std::int32_t addend)
{
    return __atomic_fetch_add(target, addend, __ATOMIC_SEQ_CST);
}

inline std::uint32_t AtomicIncrement(volatile std::uint32_t* target)
{
    return __atomic_add_fetch(
        target, static_cast<std::uint32_t>(1), __ATOMIC_SEQ_CST);
}

inline std::uint32_t AtomicExchange(volatile std::uint32_t* target,
                                    const std::uint32_t value)
{
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}

inline std::uint32_t AtomicExchangeAdd(volatile std::uint32_t* target,
                                       const std::uint32_t addend)
{
    return __atomic_fetch_add(target, addend, __ATOMIC_SEQ_CST);
}

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_LINUX_ATOMIC_OPS_H_
