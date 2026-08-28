#pragma once

// Task 513 Stage 1. The one place the wasm stubs say why they refuse.
//
// Five of the thirteen platform headers describe things WebAssembly does not
// have: hardware fault delivery with a register context, an x86 register
// context at all, an addressable guest stack, page protection, and child
// processes. Stage 1 builds the core for wasm32 without them.
//
// They report and return false rather than doing nothing quietly. The
// 2026-08-27 session was caught three times reading one success signal as
// success -- `exit 0` that was a give-up, `opened=1` that a dummy fallback also
// returned, a counter surge that was work rather than progress. A stub that
// imitates success is exactly that shape, and it would surface in Stage 3 as
// execution that appears to start and then does nothing.
//
// Once per reason, not once per call: these sit on paths the engine retries,
// and a message per call would bury the first one.

#if defined(__EMSCRIPTEN__)

#include <cstddef>

#include "repiu/platform/host_error_stream.h"

namespace repiu::platform::web
{

inline void ReportUnsupportedOnce(bool* already_reported, const char* message)
{
    if (already_reported == nullptr || *already_reported)
    {
        return;
    }
    *already_reported = true;

    std::size_t length = 0;
    while (message != nullptr && message[length] != '\0')
    {
        ++length;
    }
    WriteHostErrorStream(message, length);
}

}  // namespace repiu::platform::web

#endif  // __EMSCRIPTEN__
