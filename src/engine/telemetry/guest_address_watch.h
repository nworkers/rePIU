#pragma once

// Task 581: a watch on one guest address, recording which execution path
// reached it.
//
// This is diagnostic instrumentation only. It reads counters and prints lines;
// callers may provide an already safely-captured guest byte word, but the
// instrumentation never changes guest registers, guest memory, or control
// flow.
//
// It exists to answer a question Task 580 left open -- how the i386 host
// executes the `sti` at guest 0x010F1728, where the x64 host raises a #GP
// inside the code cache that nothing can service. The same counters are
// compiled on both hosts on purpose: reading them side by side is what makes
// "i386 does this, x64 does that" sayable.

#include <cstdint>
#include <optional>

namespace repiu::platform
{
struct GuestCpuContext;
}

namespace repiu::engine
{

struct AotCodeCachePlacement;

// The ways execution can arrive at one guest address. `kDispatchRequest` and
// `kCacheEntry` are separate because the difference between them -- dispatch
// asked and refused, versus dispatch accepted -- is what decides whether
// execution falls back to single-stepping or enters the cache.
enum class GuestAddressWatchEvent : std::uint32_t
{
    kSingleStep = 0,
    kDispatchRequest,
    kCacheEntry,
    kPrivilegedService,
    kCacheFault,
};

// Parses one guest address. Unset, empty, unparsable, and zero all mean OFF,
// which is why this returns the address rather than a bool: zero is the off
// state and no valid watched address is zero.
std::uint32_t ResolveGuestAddressWatchAddress(const char* setting);

// The watched address, read once from REPIU_GUEST_WATCH. Zero means off.
std::uint32_t GuestAddressWatchAddress();

// For hooks whose gate check is more expensive than a comparison -- the cache
// fault hook has to run a reverse address-map lookup before it knows whether
// the address is the watched one.
bool GuestAddressWatchEnabled();

// Records one arrival. `observed_address` is what the hook actually saw, which
// is the guest address itself except at the cache fault hook, where it is the
// cache address. Returns immediately when the watch is off or the address is
// not the watched one.
void RecordGuestAddressWatch(
    GuestAddressWatchEvent event,
    std::uint32_t guest_address,
    std::uint32_t observed_address,
    const repiu::platform::GuestCpuContext* registers = nullptr,
    std::optional<std::uint64_t> le_bytes = std::nullopt);

// The cache fault hook. Maps `cache_address` back through the placement and
// records `kCacheFault` when it lands in the watched address's block.
//
// Without this event an all-zero result is ambiguous between "execution never
// reached the address" and "execution reached it by a direct jump inside the
// cache", which moves no counter. A privileged instruction must fault, so a
// fault inside its block settles that.
void RecordGuestAddressWatchCacheFault(
    const AotCodeCachePlacement& placement,
    std::uint32_t cache_address,
    const repiu::platform::GuestCpuContext* registers = nullptr);

}  // namespace repiu::engine
