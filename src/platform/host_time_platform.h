#pragma once

#include <ctime>

// Task 758. What host_time.cpp needs from one platform: the calendar
// conversion of one second. Defined in win32/ and linux/ (the web build uses
// the linux/ one). Internal to src/platform/.

namespace repiu::platform::host_time_platform
{

// Local broken-down time of `seconds`. False when the host cannot convert it.
bool ConvertLocalTime(const std::time_t& seconds, std::tm* converted);

}  // namespace repiu::platform::host_time_platform
