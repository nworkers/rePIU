#include "repiu/runtime/aot_shadow_selector_block.h"

#include "repiu/platform/virtual_memory.h"

#include <new>

namespace repiu::runtime
{
namespace
{

constexpr std::size_t kShadowSelectorPageBytes = 4096U;

// A reservation is usable only if a 32-bit operand can name every byte of the
// block. The last word is what has to fit, not the first.
bool AddressableByAbs32(const void* const base)
{
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(base);
    return address <= UINT32_MAX &&
        (UINT32_MAX - address) >= sizeof(AotShadowSelectorBlock);
}

AotShadowSelectorReservation AdoptReservation(
    const repiu::platform::MemoryReservation& reservation)
{
    AotShadowSelectorReservation result;
    result.valid = true;
    result.base = reservation.base;
    result.size = reservation.size;
    result.block = static_cast<AotShadowSelectorBlock*>(reservation.base);
    new (result.block) AotShadowSelectorBlock();
    result.message = "shadow selector block reserved";
    return result;
}

}  // namespace

AotShadowSelectorReservation ReserveAotShadowSelectorBlock()
{
    AotShadowSelectorReservation result;

    if constexpr (sizeof(void*) > 4U)
    {
        for (const std::uintptr_t candidate : kAotShadowSelectorCandidateBases)
        {
            const repiu::platform::MemoryReservation reservation =
                repiu::platform::ReserveMemory(
                    reinterpret_cast<void*>(candidate),
                    kShadowSelectorPageBytes, true,
                    repiu::platform::MemoryProtection::kReadWrite);
            if (reservation.base == nullptr)
            {
                continue;
            }
            if (AddressableByAbs32(reservation.base))
            {
                return AdoptReservation(reservation);
            }
            // The host ignored the requested address and answered with one the
            // guard operand cannot name. Keeping it would be worse than having
            // none, because the guard would read someone else's memory.
            repiu::platform::ReleaseMemory(reservation.base, reservation.size);
        }
        result.message =
            "no address below 4GiB was available for the shadow selector block";
        return result;
    }

    const repiu::platform::MemoryReservation reservation =
        repiu::platform::ReserveMemory(
            nullptr, kShadowSelectorPageBytes, true,
            repiu::platform::MemoryProtection::kReadWrite);
    if (reservation.base != nullptr && AddressableByAbs32(reservation.base))
    {
        return AdoptReservation(reservation);
    }
    if (reservation.base != nullptr)
    {
        repiu::platform::ReleaseMemory(reservation.base, reservation.size);
    }
    result.message = "shadow selector block reservation failed";
    return result;
}

void ReleaseAotShadowSelectorBlock(
    const AotShadowSelectorReservation& reservation)
{
    if (!reservation.valid || reservation.base == nullptr)
    {
        return;
    }
    repiu::platform::ReleaseMemory(reservation.base, reservation.size);
}

}  // namespace repiu::runtime
