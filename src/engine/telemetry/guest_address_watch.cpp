#include "guest_address_watch.h"

#include "repiu/engine/aot_code_cache.h"
#include "repiu/platform/host_error_stream.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>

namespace repiu::engine
{

namespace
{

// Enough occurrences to see an order and a repetition, few enough that a run
// which single-steps 14,304 times does not bury the answer. The counters keep
// counting past this; only the printing stops.
constexpr std::uint32_t kGuestAddressWatchPrintLimit = 8U;

constexpr std::size_t kGuestAddressWatchEventCount = 5U;

std::atomic<std::uint32_t> g_guest_address_watch_counts
    [kGuestAddressWatchEventCount] = {};

const char* GuestAddressWatchEventName(GuestAddressWatchEvent event)
{
    switch (event)
    {
        case GuestAddressWatchEvent::kSingleStep: return "step";
        case GuestAddressWatchEvent::kDispatchRequest: return "dispatch";
        case GuestAddressWatchEvent::kCacheEntry: return "cache";
        case GuestAddressWatchEvent::kPrivilegedService: return "priv";
        case GuestAddressWatchEvent::kCacheFault: return "fault";
    }
    return "unknown";
}

}  // namespace

std::uint32_t ResolveGuestAddressWatchAddress(const char* setting)
{
    if (setting == nullptr || *setting == '\0')
    {
        return 0U;
    }
    // Base 0, so `0x010F1728` reads as hex and `17766184` as decimal. Base 0
    // also reads a leading-zero decimal as octal, which is why hex is the form
    // the documentation and every example use.
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(setting, &end, 0);
    if (end == setting)
    {
        return 0U;
    }
    return static_cast<std::uint32_t>(parsed);
}

std::uint32_t GuestAddressWatchAddress()
{
    static const std::uint32_t address =
        ResolveGuestAddressWatchAddress(std::getenv("REPIU_GUEST_WATCH"));
    return address;
}

bool GuestAddressWatchEnabled()
{
    return GuestAddressWatchAddress() != 0U;
}

void RecordGuestAddressWatch(GuestAddressWatchEvent event,
                             std::uint32_t guest_address,
                             std::uint32_t observed_address)
{
    // The gate comes first so a watch that is off costs one comparison against
    // a value already in a register, with no formatting and no writes.
    const std::uint32_t watched = GuestAddressWatchAddress();
    if (watched == 0U || guest_address != watched)
    {
        return;
    }
    const std::size_t index = static_cast<std::size_t>(event);
    if (index >= kGuestAddressWatchEventCount)
    {
        return;
    }
    const std::uint32_t occurrence =
        g_guest_address_watch_counts[index].fetch_add(
            1U, std::memory_order_relaxed) + 1U;
    if (occurrence > kGuestAddressWatchPrintLimit)
    {
        return;
    }
    // snprintf into a local buffer and one unbuffered write, for the reason
    // host_error_stream.h records: several of these hooks run inside the fault
    // handler, and a diagnostic that can block on stdio's lock is one that can
    // stop the thing it measures.
    char line[128] = {};
    const int length = std::snprintf(
        line,
        sizeof(line),
        "[repiu-watch] event=%s guest=0x%08X n=%u at=0x%08X\n",
        GuestAddressWatchEventName(event),
        guest_address,
        occurrence,
        observed_address);
    if (length > 0)
    {
        repiu::platform::WriteHostErrorStream(
            line, static_cast<std::size_t>(length));
    }
}

void RecordGuestAddressWatchCacheFault(const AotCodeCachePlacement& placement,
                                       std::uint32_t cache_address)
{
    // Gated before the lookup rather than inside RecordGuestAddressWatch: the
    // reverse address-map search is the expensive part, and an off watch must
    // not pay for it on every fault the guest takes.
    if (!GuestAddressWatchEnabled())
    {
        return;
    }
    std::uint32_t guest_address = 0U;
    if (!FindAotGuestAddress(placement, cache_address, &guest_address))
    {
        return;
    }
    RecordGuestAddressWatch(GuestAddressWatchEvent::kCacheFault,
                            guest_address,
                            cache_address);
}

}  // namespace repiu::engine
