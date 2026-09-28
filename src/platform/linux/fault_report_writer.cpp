#include "fault_report_writer.h"

#if !defined(_WIN32)

#if defined(__linux__)
#include <sys/syscall.h>
#include <sys/uio.h>
#endif
#include <unistd.h>

namespace repiu::platform::linux_fault
{

// Task 578. The unhandled-fault line, written with `write` alone.
//
// Nothing here may allocate, lock, or call into a logging library: this runs on
// a signal handler that is about to let the process die, and a handler that
// hangs replaces a diagnosable crash with an undiagnosable one.
void WriteHex(char* out, std::size_t* length, std::uint64_t value)
{
    out[(*length)++] = '0';
    out[(*length)++] = 'x';
    bool leading = true;
    for (int shift = 60; shift >= 0; shift -= 4)
    {
        const auto digit = static_cast<unsigned>((value >> shift) & 0xFU);
        if (leading && digit == 0U && shift != 0)
        {
            continue;
        }
        leading = false;
        out[(*length)++] = static_cast<char>(
            digit < 10U ? '0' + digit : 'a' + (digit - 10U));
    }
}

void WriteNamedHex(char* out, std::size_t* length, const char* name,
                   std::uint32_t value)
{
    for (const char* cursor = name; *cursor != '\0'; ++cursor)
    {
        out[(*length)++] = *cursor;
    }
    WriteHex(out, length, value);
}

void WriteNamedHex64(char* out, std::size_t* length, const char* name,
                     std::uint64_t value)
{
    for (const char* cursor = name; *cursor != '\0'; ++cursor)
    {
        out[(*length)++] = *cursor;
    }
    WriteHex(out, length, value);
}

void WriteByteHex(char* out, std::size_t* length, const std::uint8_t value)
{
    const char digits[] = "0123456789abcdef";
    out[(*length)++] = digits[(value >> 4U) & 0x0FU];
    out[(*length)++] = digits[value & 0x0FU];
}

// Task 717. Sixty-four guest stack words from ESP on a line of their own, so
// the return addresses above an unhandled fault name the call chain that led to
// it. Read through process_vm_readv like the four words above, which returns a
// short count rather than faulting when the range runs off mapped memory.
void WriteFaultGuestStackDump(const std::uint32_t guest_esp)
{
    constexpr std::size_t kWords = 64U;
    std::uint32_t words[kWords] = {};
    std::size_t count = 0U;
#if defined(__linux__) && defined(SYS_process_vm_readv)
    struct iovec local = {};
    local.iov_base = words;
    local.iov_len = sizeof(words);
    struct iovec remote = {};
    remote.iov_base = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(guest_esp));
    remote.iov_len = sizeof(words);
    const long copied = syscall(
        SYS_process_vm_readv, static_cast<long>(getpid()), &local, 1U,
        &remote, 1U, 0U);
    if (copied > 0L)
    {
        count = static_cast<std::size_t>(copied) / sizeof(std::uint32_t);
    }
#endif
    if (count == 0U)
    {
        return;
    }
    char line[kWords * 12U + 64U];
    std::size_t length = 0;
    const char prefix[] = "[repiu-fault-stack] esp=";
    for (std::size_t index = 0; index + 1U < sizeof(prefix); ++index)
    {
        line[length++] = prefix[index];
    }
    WriteHex(line, &length, guest_esp);
    for (std::size_t index = 0; index < count; ++index)
    {
        line[length++] = ' ';
        WriteHex(line, &length, words[index]);
    }
    line[length++] = static_cast<char>(10);  // newline
    const ssize_t written = write(2, line, length);
    (void)written;
}

}  // namespace repiu::platform::linux_fault

#endif
