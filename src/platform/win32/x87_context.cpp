#include "repiu/platform/win32/x87_context.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include "repiu/platform/guest_cpu_context.h"

namespace repiu::platform::win32
{

#if defined(_WIN32)
bool PushX87Float(repiu::platform::GuestCpuContext* context, float value)
{
    if (context == nullptr || !std::isfinite(value))
    {
        return false;
    }

    std::uint64_t significand = 0;
    std::uint16_t sign_and_exponent = 0;
    const bool is_zero = value == 0.0F;
    if (!is_zero)
    {
        const bool negative = std::signbit(value);
        int binary_exponent = 0;
        const long double fraction = std::frexp(
            std::fabs(static_cast<long double>(value)), &binary_exponent);
        const long double normalized = fraction * 2.0L;
        significand = static_cast<std::uint64_t>(
            std::ldexp(normalized, 63));
        const int biased_exponent = binary_exponent - 1 + 16383;
        if (biased_exponent <= 0 || biased_exponent >= 0x7FFF)
        {
            return false;
        }
        sign_and_exponent = static_cast<std::uint16_t>(biased_exponent);
        if (negative)
        {
            sign_and_exponent |= 0x8000U;
        }
    }

    const std::uint16_t old_status = static_cast<std::uint16_t>(
        context->FloatSave.StatusWord);
    const std::uint16_t old_top =
        static_cast<std::uint16_t>((old_status >> 11U) & 0x07U);
    const std::uint16_t new_top =
        static_cast<std::uint16_t>((old_top + 7U) & 0x07U);
    std::uint8_t* destination =
        context->FloatSave.RegisterArea + new_top * 10U;
    std::memcpy(destination, &significand, sizeof(significand));
    std::memcpy(destination + sizeof(significand),
                &sign_and_exponent,
                sizeof(sign_and_exponent));

    std::uint16_t status = static_cast<std::uint16_t>(
        old_status & ~0x3800U);
    status |= static_cast<std::uint16_t>(new_top << 11U);
    context->FloatSave.StatusWord = status;

    std::uint16_t tag = static_cast<std::uint16_t>(
        context->FloatSave.TagWord);
    const std::uint16_t shift = static_cast<std::uint16_t>(new_top * 2U);
    tag &= static_cast<std::uint16_t>(~(0x03U << shift));
    if (is_zero)
    {
        tag |= static_cast<std::uint16_t>(0x01U << shift);
    }
    context->FloatSave.TagWord = tag;
    return true;
}
#endif

}  // namespace repiu::platform::win32
