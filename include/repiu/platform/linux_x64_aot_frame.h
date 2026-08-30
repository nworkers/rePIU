#ifndef REPIU_PLATFORM_LINUX_X64_AOT_FRAME_H_
#define REPIU_PLATFORM_LINUX_X64_AOT_FRAME_H_

// Task 547. The named frame shared by the future Linux x64 AOT/DBT bridge and
// its resolver. Guest values stay 32-bit; only host-owned addresses use the
// native pointer width.

#define REPIU_LINUX_X64_FRAME_GUEST_SOURCE 64
#define REPIU_LINUX_X64_FRAME_GUEST_CONTINUATION 68
#define REPIU_LINUX_X64_FRAME_GUEST_METADATA_ESP 72
#define REPIU_LINUX_X64_FRAME_STATUS 76
#define REPIU_LINUX_X64_FRAME_CONTEXT 80
#define REPIU_LINUX_X64_FRAME_GUEST_MEMORY_BASE 88
#define REPIU_LINUX_X64_FRAME_HOST_CONTINUATION 96

#if !defined(__ASSEMBLER__)

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace repiu::platform
{

using LinuxX64GuestWord = std::uint32_t;
using LinuxX64HostPointer = std::uintptr_t;

struct LinuxX64GuestRegisterState
{
    LinuxX64GuestWord edi = 0;
    LinuxX64GuestWord esi = 0;
    LinuxX64GuestWord ebx = 0;
    LinuxX64GuestWord edx = 0;
    LinuxX64GuestWord ecx = 0;
    LinuxX64GuestWord eax = 0;
    LinuxX64GuestWord ebp = 0;
    LinuxX64GuestWord eip = 0;
    LinuxX64GuestWord esp = 0;
    LinuxX64GuestWord eflags = 0;
    LinuxX64GuestWord seg_cs = 0;
    LinuxX64GuestWord seg_ds = 0;
    LinuxX64GuestWord seg_es = 0;
    LinuxX64GuestWord seg_fs = 0;
    LinuxX64GuestWord seg_gs = 0;
    LinuxX64GuestWord seg_ss = 0;
};

struct alignas(16) LinuxX64AotDispatchFrame
{
    LinuxX64GuestRegisterState guest;
    LinuxX64GuestWord guest_source = 0;
    LinuxX64GuestWord guest_continuation = 0;
    LinuxX64GuestWord guest_metadata_esp = 0;
    LinuxX64GuestWord status = 0;
    LinuxX64HostPointer context = 0;
    LinuxX64HostPointer guest_memory_base = 0;
    LinuxX64HostPointer host_continuation = 0;
};

static_assert(sizeof(LinuxX64GuestWord) == 4);
static_assert(sizeof(LinuxX64HostPointer) == 8);
static_assert(std::is_standard_layout_v<LinuxX64AotDispatchFrame>);
static_assert(alignof(LinuxX64AotDispatchFrame) == 16);
static_assert(offsetof(LinuxX64AotDispatchFrame, guest_source) ==
              REPIU_LINUX_X64_FRAME_GUEST_SOURCE);
static_assert(offsetof(LinuxX64AotDispatchFrame, guest_continuation) ==
              REPIU_LINUX_X64_FRAME_GUEST_CONTINUATION);
static_assert(offsetof(LinuxX64AotDispatchFrame, guest_metadata_esp) ==
              REPIU_LINUX_X64_FRAME_GUEST_METADATA_ESP);
static_assert(offsetof(LinuxX64AotDispatchFrame, status) ==
              REPIU_LINUX_X64_FRAME_STATUS);
static_assert(offsetof(LinuxX64AotDispatchFrame, context) ==
              REPIU_LINUX_X64_FRAME_CONTEXT);
static_assert(offsetof(LinuxX64AotDispatchFrame, guest_memory_base) ==
              REPIU_LINUX_X64_FRAME_GUEST_MEMORY_BASE);
static_assert(offsetof(LinuxX64AotDispatchFrame, host_continuation) ==
              REPIU_LINUX_X64_FRAME_HOST_CONTINUATION);
static_assert(sizeof(LinuxX64AotDispatchFrame) == 112);

}  // namespace repiu::platform

#endif  // !__ASSEMBLER__

#endif  // REPIU_PLATFORM_LINUX_X64_AOT_FRAME_H_
