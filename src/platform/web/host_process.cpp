#include "repiu/platform/host_process.h"

#if defined(__EMSCRIPTEN__)

#include <cstdint>

#include "web_unsupported.h"

namespace repiu::platform
{
namespace
{

bool g_reported = false;

}  // namespace

// Task 500 re-executes the loader as a child so the GPU driver cannot claim the
// address range the guest image needs. A browser has no child process to launch
// and no second address space to escape into, so the technique has no form here.
//
// Whether it is even needed is separately open: the Linux frontier still records
// "is child re-execution required on Linux" as unmeasured, and the same question
// applies to wasm with a different answer waiting. Stage 5 settles it, because
// only then is there a real address-space layout to look at.
int RunChildProcessAndWait(const ChildProcessLaunch& /*launch*/,
                           std::uint32_t* host_error)
{
    web::ReportUnsupportedOnce(
        &g_reported,
        "[repiu-web] child process launch is unavailable: a browser has no "
        "process to fork. Task 513 Stage 5 decides whether the address-space "
        "reason for it applies here at all.\n");
    if (host_error != nullptr)
    {
        // ENOSYS.
        *host_error = 38U;
    }
    return kChildProcessDidNotStart;
}

}  // namespace repiu::platform

#endif  // __EMSCRIPTEN__
