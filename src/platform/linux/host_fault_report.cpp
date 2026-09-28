#include "repiu/platform/host_fault_report.h"

// Task 759. Linux adds nothing of its own to a fault report: an unhandled
// fault is reported by the platform fault handler (Task 578), and the fields
// Windows fills with its own numbers stay zero.

namespace repiu::platform
{

void ReportHostLanguageException(std::uint32_t host_code,
                                 const void* instruction_address)
{
    (void)host_code;
    (void)instruction_address;
}

bool QueryHostMemoryNumbers(const void* address,
                            std::uint32_t* state,
                            std::uint32_t* protect)
{
    (void)address;
    (void)state;
    (void)protect;
    return false;
}

bool FillsFaultReportDetail()
{
    return false;
}

bool ReadMemoryForFaultReport(const void* source,
                              void* destination,
                              std::size_t size,
                              std::size_t* copied)
{
    (void)source;
    (void)destination;
    (void)size;
    if (copied != nullptr)
    {
        *copied = 0U;
    }
    return false;
}

}  // namespace repiu::platform
