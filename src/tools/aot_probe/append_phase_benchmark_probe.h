#pragma once

#include "repiu/exe/dos4gw_loader.h"

namespace repiu::tools
{

// Task 331. Measures the five phases of `AppendDynamicAotTranslation` over
// the real image in whichever configuration the probe was built as, so the
// append distribution Tasks 328 and 329 measured in Debug can be re-attributed
// in Release without running the game.
//
// The append path needs guest bytes that live at their own addresses, because
// Task 329 replaced the arena snapshot with a direct reference. The probe
// therefore reserves a live arena, relocates the image into it, and drives real
// appends into a real placement.
//
// Two translation sizes are measured: the image entry, which is far larger than
// any in-game translation, and a window-limited translation sized near the 1,039
// instructions Task 328 measured as the in-game average. Two sizes also separate
// each phase's fixed cost per append from its per-instruction cost.
bool RunAppendPhaseBenchmarkProbe(const exe::Dos4gwLoadResult& load);

}  // namespace repiu::tools
