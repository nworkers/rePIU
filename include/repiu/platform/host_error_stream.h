#ifndef REPIU_PLATFORM_HOST_ERROR_STREAM_H_
#define REPIU_PLATFORM_HOST_ERROR_STREAM_H_

#include <cstddef>

// Task 503d-14. Bytes onto the host's error stream, without the C runtime.
//
// The native phase sampler formats its line with `snprintf` and then writes it
// with `WriteFile` rather than `fwrite`, and that combination is deliberate:
// `snprintf` writes into a caller-provided buffer and takes no lock, while the
// stdio path does. The sampler runs on its own thread beside a guest thread it
// suspends and resumes, and a diagnostic that can block on a lock the suspended
// thread might hold is a diagnostic that can stop the thing it is measuring.
//
// So the counterpart is the same shape rather than `std::fwrite`: one write
// syscall, no buffering, nothing to flush. Output is unbuffered on both hosts,
// which also means a line already written survives a crash that follows it --
// the property a sampler's output exists for.
//
// Partial writes and failures are not reported. Every caller is a diagnostic
// with nothing to do about either.

namespace repiu::platform
{

void WriteHostErrorStream(const char* bytes, std::size_t count);

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_HOST_ERROR_STREAM_H_
