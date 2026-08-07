#include "repiu/platform/win32/glide_async_present.h"

#include "repiu/runtime/env_toggle.h"

#include <cstdlib>

namespace repiu::platform::win32
{

bool ResolveGlideAsyncPresentEnabled(const char* setting)
{
    return repiu::runtime::ResolveOptInToggle(setting);
}

bool GlideAsyncPresentEnabled()
{
    static const bool enabled = ResolveGlideAsyncPresentEnabled(
        std::getenv("REPIU_GLIDE_ASYNC_PRESENT"));
    return enabled;
}

}  // namespace repiu::platform::win32
