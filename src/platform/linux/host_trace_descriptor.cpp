#include "repiu/platform/host_trace_descriptor.h"

// Task 759. Linux writes the trace dump to the descriptor.

#include <unistd.h>

namespace repiu::platform
{

void WriteHostTraceDescriptor(int file_descriptor,
                              const char* data,
                              std::size_t length)
{
    const ssize_t written = write(file_descriptor, data, length);
    (void)written;
}

}  // namespace repiu::platform
