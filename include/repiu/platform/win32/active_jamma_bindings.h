#ifndef REPIU_PLATFORM_WIN32_ACTIVE_JAMMA_BINDINGS_H_
#define REPIU_PLATFORM_WIN32_ACTIVE_JAMMA_BINDINGS_H_

#include "repiu/input/jamma_input_bindings.h"

namespace repiu::platform::win32
{

// The bindings in force for this run.
//
// Threading contract: written exactly once, from the host entry point, after
// the target profile resolves and before the guest thread and the SDL window
// exist. Read-only from then on, which is why the guest thread and the SDL
// host thread can both read it with no lock -- there is nothing to race with.
// Do not add a setter that runs after startup without revisiting that.
//
// Before the store is set, and in probes and tools that never call the setter,
// this reads back the built-in defaults, so every path has a valid mapping
// whether or not configuration was loaded.
void SetActiveJammaBindings(const input::ResolvedJammaBindings& bindings);

const input::ResolvedJammaBindings& ActiveJammaBindings();

}  // namespace repiu::platform::win32

#endif  // REPIU_PLATFORM_WIN32_ACTIVE_JAMMA_BINDINGS_H_
