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

// Task 758. Selection point: the definitions are per platform.
#if defined(_WIN32)
#include "repiu/platform/win32/atomic_ops_win32.h"
#else
#include "repiu/platform/linux/atomic_ops.h"
#endif

#endif  // REPIU_PLATFORM_ATOMIC_OPS_H_
