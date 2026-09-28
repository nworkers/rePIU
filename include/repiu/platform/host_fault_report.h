#ifndef REPIU_PLATFORM_HOST_FAULT_REPORT_H_
#define REPIU_PLATFORM_HOST_FAULT_REPORT_H_

#include <cstddef>
#include <cstdint>

// Task 759. What a fault report asks of the host itself. Win32 answers all of
// it; Linux answers "nothing", and the report's fields stay as they were
// (Task 503d-15 left this detail to Windows rather than put different numbers
// under the same names).

namespace repiu::platform
{

// Task 503d-15. When `host_code` is the code the host raises a language
// exception with (0xE06D7363, an MSVC C++ throw), prints the host stack and
// the loaded modules to stderr. A guest fault never has that code.
void ReportHostLanguageException(std::uint32_t host_code,
                                 const void* instruction_address);

// The host's own numbers for the state and the protection of the page at
// `address`, for a report whose reader knows them as the host's constants.
// False where the host has none.
bool QueryHostMemoryNumbers(const void* address,
                            std::uint32_t* state,
                            std::uint32_t* protect);

// Whether this host's fault report carries the faulting registers and the
// memory they point at.
bool FillsFaultReportDetail();

// Reads up to `size` bytes at `source` in this process without faulting;
// `*copied` receives how many were read. False when the read failed.
bool ReadMemoryForFaultReport(const void* source,
                              void* destination,
                              std::size_t size,
                              std::size_t* copied);

// What a structured exception filter answers to run its handler. It is the
// host's number (EXCEPTION_EXECUTE_HANDLER on Windows), and only the filter's
// caller reads it.
inline constexpr int kFaultFilterRunsHandler = 1;

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_HOST_FAULT_REPORT_H_
