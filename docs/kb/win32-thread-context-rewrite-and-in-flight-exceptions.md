# Win32 thread context 재작성과 전달 중인 예외 / Rewriting a Win32 thread context while an exception is in flight

## 한국어

### 왜 이 프로젝트에 필요한가

Win32 host는 종료할 때 guest thread를 `SuspendThread`로 멈추고, `GetThreadContext`로 위치를 읽고,
`SetThreadContext`로 EIP·ESP를 회수 진입점으로 바꾼 뒤 `ResumeThread`로 재개합니다(Task 507). legacy
backend는 guest를 trap flag로 한 명령씩 실행하므로 guest thread는 매 명령마다 single-step 예외를
받습니다. 즉 thread가 **예외 전달 경로 안에 있는 시간**이 깁니다.

### 관찰된 동작 (이 프로젝트에서 확인)

thread를 멈춘 순간 커널이 이미 그 thread에 예외를 전달하는 중이었다면, `SetThreadContext`는 그
예외를 **취소하지 않습니다.** 재개하면 예외는 그대로 user mode로 전달되고, 두 모양이 모두
관찰되었습니다(Task 733).

| 전달된 context | 결과 |
|---|---|
| 재작성한 context(새 EIP·ESP) | 예외 처리기가 재작성한 ESP 위에서 실행됨. 그 스택이 작으면 넘침 |
| 원래 context(옛 EIP·ESP) | 재작성이 사라지고 thread가 원래 위치에서 계속 실행됨 |

`EXCEPTION_RECORD::ExceptionAddress`는 옛 EIP일 수도, 새 EIP일 수도 있었습니다. 따라서
ExceptionAddress만으로는 이 경우를 가려낼 수 없습니다. 판단 기준은 **context의 EIP**입니다.

### 설계 규칙

* suspend·context 재작성으로 thread의 흐름을 바꾸는 코드는, 재개 직후 도착하는 예외가 옛 흐름에
  속할 수 있다고 가정해야 합니다.
* 이 프로젝트는 재작성 뒤 도착한 예외 중 context EIP가 재작성 목표이거나 옛 흐름(guest 코드)인 것을
  작은 vectored handler에서 가로채 재작성을 다시 적용합니다(`DecideShutdownRedirectGuard`). 큰 frame을
  가진 처리기를 작은 대상 스택 위에서 돌리지 않는 것이 핵심입니다.
* POSIX host는 해당 thread 자신의 signal handler 안에서 context를 바꾸므로 이 경쟁이 없습니다
  ([POSIX signal handler와 상태 공유](posix-signal-handler-state-sharing.md)).

### 참고

* [SuspendThread](https://learn.microsoft.com/windows/win32/api/processthreadsapi/nf-processthreadsapi-suspendthread) —
  "primarily designed for use by debuggers", 동기화 목적의 사용을 경고합니다.
* [SetThreadContext](https://learn.microsoft.com/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadcontext)
* [AddVectoredExceptionHandler](https://learn.microsoft.com/windows/win32/api/errhandlingapi/nf-errhandlingapi-addvectoredexceptionhandler) —
  `First`가 0이 아니면 목록 맨 앞에 등록되므로, 나중에 등록한 handler가 먼저 호출됩니다.

위 문서들은 전달 중인 예외와의 상호작용을 명시하지 않습니다. 위 표는 이 프로젝트의 관찰이며
([Task 733 설계](../design/20260922-733-win32-legacy-shutdown-crash-attribution.md)), 일반 보장이 아닙니다.

---

## English

### Why the project needs it

At shutdown the Win32 host stops the guest thread with `SuspendThread`, reads where it is with
`GetThreadContext`, rewrites EIP and ESP to the recovery entry with `SetThreadContext`, and resumes it
with `ResumeThread` (Task 507). The legacy backend executes the guest one instruction at a time through
the trap flag, so the guest thread takes a single-step exception on every instruction and spends a
long time **inside exception delivery**.

### Observed behavior (confirmed in this project)

If the kernel was already delivering an exception to the thread when it was suspended,
`SetThreadContext` **does not cancel it**. On resume the exception is still delivered to user mode,
and both of these shapes were observed (Task 733):

| Context delivered | Result |
|---|---|
| The rewritten context (new EIP/ESP) | The exception handlers run on the rewritten ESP; a small stack overflows |
| The original context (old EIP/ESP) | The rewrite is lost and the thread continues where it was |

`EXCEPTION_RECORD::ExceptionAddress` was sometimes the old EIP and sometimes the new one, so it cannot
identify the case. The **context's EIP** is what to judge.

### Design rules

* Code that redirects a thread by suspending it and rewriting its context must assume an exception
  arriving right after resume may belong to the old flow.
* This project intercepts, in a small vectored handler, exceptions arriving after the rewrite whose
  context EIP is the rewrite target or the old flow (guest code), and reapplies the rewrite
  (`DecideShutdownRedirectGuard`). The point is never to run a large-frame handler on the small target
  stack.
* POSIX hosts change the context inside the thread's own signal handler and have no such race
  ([Sharing state with a POSIX signal handler](posix-signal-handler-state-sharing.md)).

### References

The Microsoft pages above for `SuspendThread` (which warns it is "primarily designed for use by
debuggers"), `SetThreadContext`, and `AddVectoredExceptionHandler` (a nonzero `First` inserts at the
head, so a handler registered later is called first). None of them specifies the interaction with an
in-flight exception; the table above is this project's observation
([Task 733 design](../design/20260922-733-win32-legacy-shutdown-crash-attribution.md)), not a general
guarantee.
