#ifndef REPIU_PLATFORM_WEB_THUNK_CALLING_CONVENTION_H_
#define REPIU_PLATFORM_WEB_THUNK_CALLING_CONVENTION_H_

// Task 758. wasm has no stdcall; the macro expands to nothing.
// Include thunk_calling_convention.h, not this.

#define REPIU_THUNK_RESOLVER_CALL

#endif  // REPIU_PLATFORM_WEB_THUNK_CALLING_CONVENTION_H_
