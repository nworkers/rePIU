#ifndef REPIU_RUNTIME_TIMER_SAFE_POINT_INJECTION_H_
#define REPIU_RUNTIME_TIMER_SAFE_POINT_INJECTION_H_

namespace repiu::runtime
{

// Task 718. Whether Linux x64 injects timer ticks at AOT safe points, from the
// value of REPIU_LINUX_X64_SAFE_POINT_INJECTION.
//
// On by default. Tasks 716 and 717 removed the two faults that kept it opt-in,
// and without it the guest's clock stops once rendering starts, because
// render-phase ticks arrive almost entirely through safe points (5,667 of 5,729
// on Win32). `0` turns it off, to reproduce the earlier behavior; anything else,
// or no variable, leaves it on.
//
// The variable used to be tested for presence alone, so `=0` turned it on. This
// is the one value that reads as off now, so an old `=1` keeps meaning on.
inline bool ResolveTimerSafePointInjection(const char* const value)
{
    return value == nullptr || value[0] != '0' || value[1] != '\0';
}

}  // namespace repiu::runtime

#endif  // REPIU_RUNTIME_TIMER_SAFE_POINT_INJECTION_H_
