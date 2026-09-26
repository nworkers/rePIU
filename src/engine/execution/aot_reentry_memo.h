#pragma once

// Task 741. The per-address memo an HLE reentry consults before it decodes
// anything. See ThreadContext::AotReentryMemoEntry for what it holds and when
// it is invalidated. `REPIU_AOT_REENTRY_MEMO=0` turns it off for A/B.

#include "thread_context.h"
#include "guest_memory_access.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace repiu::engine
{

enum class AotReentryMemoKind : std::uint32_t
{
    kSegmentProbe = 0,
    kSpanSafety = 1,
    kLongModeIdentity = 2,
};

inline bool AotReentryMemoEnabled()
{
    static const bool enabled = []() {
        const char* const value = std::getenv("REPIU_AOT_REENTRY_MEMO");
        return value == nullptr || std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

// The slot for `guest_eip`, reset to empty if it held another address, an
// older generation or different code bytes. Null when the eight bytes at the
// address cannot be read, in which case the caller computes uncached; the
// callers' own checks then report the unreadable address the way they
// always did.
inline ThreadContext::AotReentryMemoEntry* LookupAotReentryMemo(
    ThreadContext* const context, const std::uint32_t guest_eip)
{
    constexpr std::uint32_t kCapacity = ThreadContext::kAotReentryMemoCapacity;
    static_assert((kCapacity & (kCapacity - 1U)) == 0U,
                  "the memo index needs a power-of-two capacity");
    const auto* const code = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(guest_eip));
    if (!IsGuestRangeReadable(context, code, sizeof(std::uint64_t)))
    {
        return nullptr;
    }
    std::uint64_t fingerprint = 0U;
    std::memcpy(&fingerprint, code, sizeof(fingerprint));
    const std::uint32_t index =
        ((guest_eip >> 1U) * 0x9E3779B1U) >> 22U;
    auto& entry = context->aot_reentry_memo[index & (kCapacity - 1U)];
    if (entry.guest_eip != guest_eip ||
        entry.generation != context->aot_reentry_memo_generation ||
        entry.fingerprint != fingerprint)
    {
        entry.guest_eip = guest_eip;
        entry.generation = context->aot_reentry_memo_generation;
        entry.fingerprint = fingerprint;
        entry.computed = 0U;
    }
    return &entry;
}

inline void CountAotReentryMemo(ThreadContext* const context,
                                const AotReentryMemoKind kind, const bool hit)
{
    const auto index = static_cast<std::uint32_t>(kind);
    if (hit)
    {
        ++context->aot_reentry_memo_hits[index];
    }
    else
    {
        ++context->aot_reentry_memo_misses[index];
    }
}

}  // namespace repiu::engine
