#include "repiu/platform/host_crash_report.h"

// Task 759. Nothing to install on Linux: an unhandled fault is reported by
// the platform fault handler (Task 578).

namespace repiu::platform
{

void InstallHostCrashReporter()
{
}

}  // namespace repiu::platform
