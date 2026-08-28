#include "repiu/platform/guest_cpu_context.h"

#if defined(__EMSCRIPTEN__)

#include "web_unsupported.h"

namespace repiu::platform
{
namespace
{

bool g_reported = false;

void Report()
{
    web::ReportUnsupportedOnce(
        &g_reported,
        "[repiu-web] guest CPU context is unavailable: wasm has no x86 register "
        "file to convert to or from. Task 513 Stage 3 owns the guest registers "
        "itself instead of borrowing the host's.\n");
}

}  // namespace

// Both directions convert between a host thread's register context and the
// engine's `GuestCpuContext`. On wasm there is no host register context to
// convert -- not an empty one, none.
//
// Which is also why this file is small and Stage 3 is not. When the host stops
// holding the guest's registers, the engine has to hold them, and that is the
// register file the interpreter and the translator are both built around.
bool LoadGuestCpuContext(const void* /*host_context*/,
                         GuestCpuContext* /*registers*/)
{
    Report();
    return false;
}

bool StoreGuestCpuContext(const GuestCpuContext& /*registers*/,
                          void* /*host_context*/)
{
    Report();
    return false;
}

}  // namespace repiu::platform

#endif  // __EMSCRIPTEN__
