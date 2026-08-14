#include "dos_date_probe.h"

#include "dos/dos_int21_services.h"
#include "repiu/hle/dos_date.h"

#include <cstdint>
#include <iostream>
#include <memory>

namespace repiu::tools
{

bool RunDosDateProbe()
{
    using repiu::hle::DosDate;

    if (!repiu::hle::IsDosDateValid({2024U, 2U, 29U}) ||
        repiu::hle::IsDosDateValid({2023U, 2U, 29U}) ||
        repiu::hle::IsDosDateValid({1979U, 12U, 31U}) ||
        repiu::hle::IsDosDateValid({2100U, 1U, 1U}) ||
        repiu::hle::IsDosDateValid({2026U, 0U, 15U}) ||
        repiu::hle::IsDosDateValid({2026U, 8U, 0U}))
    {
        std::cerr << "dos_date_probe failure: date validation mismatch\n";
        return false;
    }

    std::int32_t offset_days = 0;
    if (!repiu::hle::CalculateDosDateDayOffset(
            {2026U, 8U, 15U}, {2026U, 8U, 15U}, &offset_days) ||
        offset_days != 0 ||
        !repiu::hle::CalculateDosDateDayOffset(
            {2026U, 8U, 15U}, {2027U, 1U, 1U}, &offset_days) ||
        offset_days != 139)
    {
        std::cerr << "dos_date_probe failure: date offset mismatch\n";
        return false;
    }

    DosDate result;
    if (!repiu::hle::AddDosDateDays(
            {2024U, 2U, 28U}, 1, &result) ||
        result.year != 2024U || result.month != 2U || result.day != 29U ||
        !repiu::hle::AddDosDateDays(
            {2024U, 2U, 28U}, 2, &result) ||
        result.year != 2024U || result.month != 3U || result.day != 1U ||
        repiu::hle::AddDosDateDays({1980U, 1U, 1U}, -1, &result))
    {
        std::cerr << "dos_date_probe failure: date shift mismatch\n";
        return false;
    }

    std::uint8_t day_of_week = 0U;
    if (!repiu::hle::CalculateDosDateDayOfWeek(
            {1980U, 1U, 1U}, &day_of_week) ||
        day_of_week != 2U ||
        !repiu::hle::CalculateDosDateDayOfWeek(
            {2026U, 8U, 15U}, &day_of_week) ||
        day_of_week != 6U)
    {
        std::cerr << "dos_date_probe failure: weekday mismatch\n";
        return false;
    }

    auto context =
        std::make_unique<repiu::platform::win32::ThreadContext>();
    CONTEXT win32_context = {};
    win32_context.Eax = 0x12342B7FU;
    win32_context.Ecx = 2024U;
    win32_context.Edx = 0x021DU;
    repiu::platform::win32::HandleDosSetSystemDate(
        &win32_context, context.get());
    if ((win32_context.Eax & 0xFFU) != 0U ||
        !context->dos_date_offset_valid)
    {
        std::cerr << "dos_date_probe failure: valid set-date was rejected\n";
        return false;
    }
    const std::int32_t valid_offset = context->dos_date_offset_days;

    win32_context.Eax = 0x12342B00U;
    win32_context.Ecx = 2023U;
    win32_context.Edx = 0x021DU;
    repiu::platform::win32::HandleDosSetSystemDate(
        &win32_context, context.get());
    if ((win32_context.Eax & 0xFFU) != 0xFFU ||
        !context->dos_date_offset_valid ||
        context->dos_date_offset_days != valid_offset)
    {
        std::cerr << "dos_date_probe failure: invalid set-date changed state\n";
        return false;
    }

    win32_context.Eax = 0x12342A00U;
    win32_context.Ecx = 0xABCD0000U;
    win32_context.Edx = 0xDCBA0000U;
    repiu::platform::win32::HandleDosGetSystemDate(
        &win32_context, context.get());
    if ((win32_context.Ecx & 0xFFFFU) != 2024U ||
        (win32_context.Edx & 0xFFFFU) != 0x021DU ||
        (win32_context.Eax & 0xFFU) != 4U)
    {
        std::cerr << "dos_date_probe failure: set/get date round trip failed\n";
        return false;
    }

    std::cout << "dos_date_probe=pass\n";
    return true;
}

}  // namespace repiu::tools
