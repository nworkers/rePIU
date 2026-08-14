#ifndef REPIU_HLE_DOS_DATE_H_
#define REPIU_HLE_DOS_DATE_H_

#include <cstdint>

namespace repiu::hle
{

struct DosDate
{
    std::uint16_t year = 1980U;
    std::uint8_t month = 1U;
    std::uint8_t day = 1U;
};

bool IsDosDateValid(const DosDate& date);

bool CalculateDosDateDayOffset(const DosDate& base,
                               const DosDate& target,
                               std::int32_t* offset_days);

bool AddDosDateDays(const DosDate& base,
                    std::int32_t offset_days,
                    DosDate* result);

bool CalculateDosDateDayOfWeek(const DosDate& date,
                               std::uint8_t* day_of_week);

}  // namespace repiu::hle

#endif  // REPIU_HLE_DOS_DATE_H_
