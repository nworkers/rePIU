#include "repiu/hle/dos_date.h"

#include <cstdint>

namespace repiu::hle
{
namespace
{

constexpr std::uint16_t kDosMinimumYear = 1980U;
constexpr std::uint16_t kDosMaximumYear = 2099U;

bool IsLeapYear(const std::uint16_t year)
{
    return (year % 4U == 0U && year % 100U != 0U) || year % 400U == 0U;
}

std::uint8_t DaysInMonth(const std::uint16_t year,
                         const std::uint8_t month)
{
    constexpr std::uint8_t kMonthDays[12] = {
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U};
    if (month == 0U || month > 12U)
    {
        return 0U;
    }
    if (month == 2U && IsLeapYear(year))
    {
        return 29U;
    }
    return kMonthDays[month - 1U];
}

std::int32_t DateToOrdinal(const DosDate& date)
{
    std::int32_t ordinal = 0;
    for (std::uint16_t year = kDosMinimumYear; year < date.year; ++year)
    {
        ordinal += IsLeapYear(year) ? 366 : 365;
    }
    for (std::uint8_t month = 1U; month < date.month; ++month)
    {
        ordinal += DaysInMonth(date.year, month);
    }
    return ordinal + static_cast<std::int32_t>(date.day) - 1;
}

std::int32_t DosDateRangeDays()
{
    return DateToOrdinal({kDosMaximumYear, 12U, 31U}) + 1;
}

}  // namespace

bool IsDosDateValid(const DosDate& date)
{
    if (date.year < kDosMinimumYear || date.year > kDosMaximumYear)
    {
        return false;
    }
    const std::uint8_t month_days = DaysInMonth(date.year, date.month);
    return date.day != 0U && date.day <= month_days;
}

bool CalculateDosDateDayOffset(const DosDate& base,
                               const DosDate& target,
                               std::int32_t* offset_days)
{
    if (offset_days == nullptr || !IsDosDateValid(base) ||
        !IsDosDateValid(target))
    {
        return false;
    }
    *offset_days = DateToOrdinal(target) - DateToOrdinal(base);
    return true;
}

bool AddDosDateDays(const DosDate& base,
                    const std::int32_t offset_days,
                    DosDate* result)
{
    if (result == nullptr || !IsDosDateValid(base))
    {
        return false;
    }
    std::int64_t ordinal = static_cast<std::int64_t>(DateToOrdinal(base)) +
        static_cast<std::int64_t>(offset_days);
    if (ordinal < 0 || ordinal >= DosDateRangeDays())
    {
        return false;
    }

    std::uint16_t year = kDosMinimumYear;
    while (year <= kDosMaximumYear)
    {
        const std::int32_t year_days = IsLeapYear(year) ? 366 : 365;
        if (ordinal < year_days)
        {
            break;
        }
        ordinal -= year_days;
        ++year;
    }
    std::uint8_t month = 1U;
    while (month <= 12U)
    {
        const std::uint8_t month_days = DaysInMonth(year, month);
        if (ordinal < month_days)
        {
            break;
        }
        ordinal -= month_days;
        ++month;
    }
    result->year = year;
    result->month = month;
    result->day = static_cast<std::uint8_t>(ordinal + 1);
    return true;
}

bool CalculateDosDateDayOfWeek(const DosDate& date,
                               std::uint8_t* day_of_week)
{
    if (day_of_week == nullptr || !IsDosDateValid(date))
    {
        return false;
    }
    // 1980-01-01 was Tuesday, where DOS encodes Sunday as zero.
    *day_of_week = static_cast<std::uint8_t>((DateToOrdinal(date) + 2) % 7);
    return true;
}

}  // namespace repiu::hle
