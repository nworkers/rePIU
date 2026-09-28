#ifndef REPIU_PLATFORM_LINUX_X86_THUNK_CALLING_CONVENTION_H_
#define REPIU_PLATFORM_LINUX_X86_THUNK_CALLING_CONVENTION_H_

// Task 758. GCC and Clang spell stdcall as an attribute on i386.
// Include thunk_calling_convention.h, not this.

#define REPIU_THUNK_RESOLVER_CALL __attribute__((stdcall))

#endif  // REPIU_PLATFORM_LINUX_X86_THUNK_CALLING_CONVENTION_H_
