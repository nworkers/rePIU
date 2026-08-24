// Task 503d. Instantiations of the dispatch-thunk bridge, so the macro can be
// held to its contract before the five real dispatchers reach Linux.
//
// The probe entries are not stand-ins written to resemble the real thing: they
// are the same macro, expanded with a resolver the probe supplies. Whatever is
// wrong with them here is wrong in all five.

#include "../../platform/linux/stack_bridge.inc.S"

    .text

REPIU_STACK_BRIDGE RepiuStackBridgeProbeThunk, RepiuStackBridgeProbeResolver, g_repiu_bridge_context, g_repiu_bridge_host_esp

// Task 503d-12. The same macro with a refusal that resumes rather than returns,
// which is what four of the five real thunks do. The byte count is arbitrary
// here -- what is under test is that the thunk reads the dispatch address the
// site pushed, adds the count, and returns there instead of to its caller.
REPIU_STACK_BRIDGE RepiuStackBridgeProbeFallbackThunk, RepiuStackBridgeProbeResolver, g_repiu_bridge_context, g_repiu_bridge_host_esp, 15

// The dispatch site the refusal has to land inside: it pushes an address and
// calls, exactly as generated code does. The pushed address is fifteen bytes
// short of the landing pad, so a correct refusal arrives there and a refusal
// that left the return address alone arrives at the line after the call.
    .globl RepiuStackBridgeFallbackHarness
    .type RepiuStackBridgeFallbackHarness, @function
RepiuStackBridgeFallbackHarness:
    mov eax, offset .Lstack_bridge_fallback_landing
    sub eax, 15
    push eax
    call RepiuStackBridgeProbeFallbackThunk
    // The bridge was crossed and returned normally. eax carries whatever the
    // resolver left in the frame's saved EAX.
    add esp, 4
    ret

.Lstack_bridge_fallback_landing:
    // Reached by the refusal path. The pushed dispatch address is still on the
    // stack, as it is at a real site's fallback label.
    add esp, 4
    mov eax, 0xFA11BACC
    ret
    .size RepiuStackBridgeFallbackHarness, .-RepiuStackBridgeFallbackHarness

    .section .note.GNU-stack,"",@progbits
