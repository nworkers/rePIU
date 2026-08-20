#include "repiu/platform/win32/active_jamma_bindings.h"

#include "win32_host_key_translation.h"

namespace repiu::platform::win32
{
namespace
{

input::ResolvedJammaBindings& MutableActiveJammaBindings()
{
    // Function-local static so the built-in defaults are ready on first use
    // regardless of static initialization order, and so a tool that never
    // loads configuration still gets a usable mapping.
    static input::ResolvedJammaBindings bindings = []
    {
        input::ResolvedJammaBindings defaults =
            input::DefaultJammaBindings();
        ResolveWin32VirtualKeys(&defaults);
        return defaults;
    }();
    return bindings;
}

}  // namespace

void SetActiveJammaBindings(const input::ResolvedJammaBindings& bindings)
{
    input::ResolvedJammaBindings& active = MutableActiveJammaBindings();
    active = bindings;
    // Resolved here rather than at the call site so no caller can install a
    // binding set whose virtual keys were never filled in, which would leave
    // the polling path silently dead while the window path still worked.
    ResolveWin32VirtualKeys(&active);
}

const input::ResolvedJammaBindings& ActiveJammaBindings()
{
    return MutableActiveJammaBindings();
}

}  // namespace repiu::platform::win32
