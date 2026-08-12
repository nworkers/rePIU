#pragma once

// Servicing for string instructions that read DOS low memory (below 64 KiB).
// The surrounding low-memory read fault handler emulates only MOV/MOVZX/MOVSX,
// which leaves SCAS/LODS/CMPS unserviced; pumpit8 reaches a `repne scasb` at
// linear 0x17 after libpng returns null, and DOS/4GW makes that address
// readable while rePIU does not. See
// docs/design/20260812-475-low-memory-string-instruction-servicing.md.

#include "thread_context.h"

#include <cstdint>

namespace repiu::platform::win32
{

// Upper bound on iterations serviced inside one fault. A scan that stays in
// low memory this long is pathological rather than a real string operation,
// and stopping simply leaves EIP in place so the guest can fault again.
constexpr std::uint32_t kLowMemoryStringIterationCap =
    repiu::runtime::kDosLowMemorySize;

// Attempts to service the string instruction at the current EIP. Returns false
// when the instruction is not a supported low-memory string read, leaving the
// context untouched so the caller can fall through to its own handling.
bool ServiceGuestLowMemoryStringInstruction(CONTEXT* win32_context,
                                            ThreadContext* context);

}  // namespace repiu::platform::win32
