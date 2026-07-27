#pragma once

namespace repiu::tools
{

// Differential check that the Task 324 hash index returns exactly what the
// original linear scan returned, across duplicate guest addresses, retired
// generations, mixed active flags, forced hash collisions, dynamic appends, and
// an invalidated index. A wrong answer here jumps guest execution to the wrong
// translation generation, so equivalence matters more than speed.
bool RunAotCacheAddressIndexProbe();

}  // namespace repiu::tools
