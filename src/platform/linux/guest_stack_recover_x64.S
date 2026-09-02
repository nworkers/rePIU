// Task 575. The two fault-recovery symbols, defined for an x86-64 host.
//
// `guest_stack_switch.S` holds the i386 versions and is fenced out of the x64
// build by Task 545, because everything in it is a 32-bit execution contract.
// Two of its symbols are nevertheless referenced by C++ that compiles on every
// host -- `RecoverToHost` and the `else` branches beside it take their
// addresses -- so an x64 link needs them to exist. They were the only two
// unresolved symbols in the whole x64 `repiu` link.
//
// **Neither is called.** On i386 the fault handler plants their address in the
// resumed context's EIP, so what runs at fault-resume time is the recovery.
//
// **Neither is reachable here.** Both sites are inside a `use_guest_stack`
// branch or on a fault path taken only while a guest thread runs, and
// `GuestEntryThreadProc` returns 4 on a non-i386 host (Task 544), so no guest
// thread starts on x64 at all.
//
// That leaves two possible definitions, and only one of them is honest.
//
// A `ret` would imitate i386 and let a run that reached here look recovered
// while the frame it returned into is rubbish. That is the failure shape this
// project keeps meeting -- something that raises nothing and is wrong -- and it
// is refused.
//
// `ud2` is the other. Arriving at all is the defect, so it ends here, with the
// address preserved in a core dump. It needs no stack alignment and no ABI,
// which matters because a planted recovery point has neither guaranteed.
//
// There is a second reason for `ud2`, and it is the more concrete one. The code
// that plants these does:
//
//     context->Eip = static_cast<RegisterField>(
//         reinterpret_cast<std::uintptr_t>(&RecoverGuestStackException));
//
// `Eip` is 32 bits and an x64 function address is 64, so that assignment
// truncates; `guest_cpu_context.cpp` then merges it into `REG_RIP` keeping the
// faulting site's top half. Where the two differ above bit 31 the result is not
// this function at all. Making the destination a stop rather than a return is
// the only thing that behaves sanely under that. Fixing the truncation belongs
// with the rest of the x64 fault-path meaning, which is a separate unit.

    .intel_syntax noprefix
    .text

// void RecoverGuestStackException()
//
// On i386: restores the host segment registers and runs the tail of
// `CallGuestEntryWithStack` so the caller's frame comes out intact. None of
// that has an x64 counterpart yet -- guest ESP lives in R15D and there is no
// stack switch to undo.
    .globl RecoverGuestStackException
    .type RecoverGuestStackException, @function
RecoverGuestStackException:
    ud2
    .size RecoverGuestStackException, .-RecoverGuestStackException

// void RecoverHostStackException()
//
// On i386: `xor eax, eax` and `ret`, because the fault happened on the host
// stack, there was no switch to undo, and the only thing owed was a defined
// return. The reasoning does not carry over: nothing on x64 has established
// what frame this would be returning into.
    .globl RecoverHostStackException
    .type RecoverHostStackException, @function
RecoverHostStackException:
    ud2
    .size RecoverHostStackException, .-RecoverHostStackException

    .section .note.GNU-stack,"",@progbits
