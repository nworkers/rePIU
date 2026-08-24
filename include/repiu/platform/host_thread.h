#ifndef REPIU_PLATFORM_HOST_THREAD_H_
#define REPIU_PLATFORM_HOST_THREAD_H_

#include <cstdint>

// Task 503d-15. The operating system's own number for the calling thread.
//
// The engine asks this for one reason and reports it for another. The dispatch
// path compares it against the guest thread's, because a fault raised on any
// other thread is not the engine's to handle; and the shared telemetry block
// publishes it so a second process, and a person with a debugger attached, can
// say which thread the numbers belong to.
//
// That second use is why this is the operating system's identifier rather than
// a hash of `std::thread::id`: the number has to be the one the debugger shows.
// `GetCurrentThreadId` and `gettid` both are, on their own hosts.

namespace repiu::platform
{

[[nodiscard]] std::uint32_t CurrentThreadId();

// Task 503d-18. Creating a thread, asking whether it is still running, and
// collecting what it returned.
//
// The engine starts two: the AOT translation worker and the guest thread. Their
// shape comes from the four call sites rather than from the kernel32 table
// 3d-14 fenced -- the entry returns a `std::uint32_t`, which is neither
// Windows' `DWORD WINAPI(LPVOID)` nor POSIX's `void*(void*)` but what the
// callers actually pass and read.
using HostThreadEntry = std::uint32_t (*)(void* parameter);

struct HostThread
{
    // Windows keeps a HANDLE here; POSIX keeps a record holding the pthread_t,
    // a completion flag, and the exit code, because `pthread_t` carries none of
    // that and `pthread_join` waits when the caller must not.
    void* handle = nullptr;
    std::uint32_t id = 0;
    bool valid = false;
};

// What a thread is doing, without the ambiguity Windows has here.
//
// `GetExitCodeThread` answers 259 for a running thread, and 259 is also a legal
// exit code -- so a caller comparing against `STILL_ACTIVE`, as both of this
// engine's did, would read a thread that exited with 259 as one that never
// stops. Separating the two questions removes that, and the Windows backend
// decides `running` with a zero-length wait rather than from the code.
struct HostThreadStatus
{
    bool running = true;
    std::uint32_t exit_code = 0;
};

// `host_error` receives the host's own error number on failure, for the
// message; control flow reads the return value.
[[nodiscard]] bool CreateHostThread(HostThreadEntry entry,
                                    void* parameter,
                                    HostThread* thread,
                                    std::uint32_t* host_error);

// Asks without waiting, which is what the host poll loop needs: it has Glide
// commands to pump and timer ticks to deliver between questions.
[[nodiscard]] HostThreadStatus QueryHostThread(const HostThread& thread);

// Waits up to the timeout. Returns false if the thread was still running when
// it expired.
[[nodiscard]] bool JoinHostThread(const HostThread& thread,
                                  std::uint32_t timeout_milliseconds,
                                  std::uint32_t* exit_code);

// Releases what `CreateHostThread` allocated. The thread must have exited; a
// caller that has not established that has a bug rather than a cleanup step.
void CloseHostThread(HostThread* thread);

}  // namespace repiu::platform

#endif  // REPIU_PLATFORM_HOST_THREAD_H_
