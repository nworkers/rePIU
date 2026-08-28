#include "repiu/engine/glide_async_present.h"

#include "repiu/runtime/env_toggle.h"

#include <cstdlib>

namespace repiu::engine
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

}  // namespace repiu::engine
