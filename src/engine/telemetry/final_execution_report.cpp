#include "repiu/engine/final_execution_report.h"

#include <atomic>

namespace repiu::engine
{
namespace
{

// Plain pointers behind one atomic flag rather than three atomics. The
// registration happens once, before the guest thread exists; only the flag is
// ever contended, and only to the extent that two shutdown arms could both
// reach it.
FinalExecutionReportCallback g_callback = nullptr;
void* g_user = nullptr;
std::atomic<bool> g_emitted{false};

}  // namespace

void SetFinalExecutionReport(const FinalExecutionReportCallback callback,
                             void* const user)
{
    g_callback = callback;
    g_user = user;
}

bool EmitFinalExecutionReport(const MinimalExecutionAttempt& attempt)
{
    if (g_callback == nullptr)
    {
        return false;
    }
    // exchange rather than load-then-store: the refused arm runs beside a live
    // guest thread, and a second reporter arriving between the two would print
    // the summary twice.
    if (g_emitted.exchange(true, std::memory_order_acq_rel))
    {
        return false;
    }
    g_callback(attempt, g_user);
    return true;
}

bool FinalExecutionReportEmitted()
{
    return g_emitted.load(std::memory_order_acquire);
}

void ResetFinalExecutionReport()
{
    g_callback = nullptr;
    g_user = nullptr;
    g_emitted.store(false, std::memory_order_release);
}

}  // namespace repiu::engine
