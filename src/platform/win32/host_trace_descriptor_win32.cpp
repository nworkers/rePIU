#include "repiu/platform/host_trace_descriptor.h"

// Task 759. Win32 drops the trace dump; it has no descriptor to write to.

namespace repiu::platform
{

void WriteHostTraceDescriptor(int file_descriptor,
                              const char* data,
                              std::size_t length)
{
    (void)file_descriptor;
    (void)data;
    (void)length;
}

}  // namespace repiu::platform
