#pragma once

#if !defined(_WIN32)

#include <cstddef>
#include <cstdint>

// Task 757. The async-signal-safe pieces of the Linux fault report, shared by
// the common handler and the x64 diagnostics. Internal to src/platform/linux/.

namespace repiu::platform::linux_fault
{

void WriteHex(char* out, std::size_t* length, std::uint64_t value);
void WriteNamedHex(char* out, std::size_t* length, const char* name,
                   std::uint32_t value);
void WriteNamedHex64(char* out, std::size_t* length, const char* name,
                     std::uint64_t value);
void WriteByteHex(char* out, std::size_t* length, std::uint8_t value);
void WriteFaultGuestStackDump(std::uint32_t guest_esp);

}  // namespace repiu::platform::linux_fault

#endif
