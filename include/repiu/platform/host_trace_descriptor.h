#ifndef REPIU_PLATFORM_HOST_TRACE_DESCRIPTOR_H_
#define REPIU_PLATFORM_HOST_TRACE_DESCRIPTOR_H_

#include <cstddef>

namespace repiu::platform
{

// Writes a trace dump's bytes to the descriptor the dump was given. Linux
// writes them; Win32 has no descriptor to write to and drops them, as it
// always did.
void WriteHostTraceDescriptor(int file_descriptor,
                              const char* data,
                              std::size_t length);

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_HOST_TRACE_DESCRIPTOR_H_
