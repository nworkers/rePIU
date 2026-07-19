# 작업 지시서: 로더 타임아웃 종료 시 Segfault(exit 139) 해결

## 1. 개요
현재 AOT 모드 등에서 실행이 장기화되어 `PollThreadUntilExit` 함수 내에서 타임아웃이 발생할 경우, 호스트 스레드는 `TerminateThread`를 이용해 게스트 스레드를 즉시 종료합니다. 하지만 게스트 스레드가 호스트 자원(예: 힙 록)을 점유한 상태에서 강제 종료되면 이어지는 종료 요약(Summary) 출력 시 `logger.info` 내부에서 교착 상태에 빠지거나 힙 오염에 의해 Segfault(exit 139)가 발생합니다.

이를 해결하기 위해 타임아웃 시 스레드를 즉시 종료하는 대신, 안전한 시점(순수 게스트 코드나 AOT 캐시 내부 실행 중)에 `SuspendThread`와 스택/EIP 조작을 통해 호스트 예외 핸들러나 복구 진입점(`RecoverToHost`)으로 리디렉션하여 클린 종료를 유도합니다.

## 2. 작업 내용
- **`src/platform/win32/execution/execution_trampoline.cpp` 수정**
  - `RunWin32ExecutionThread`의 `WAIT_TIMEOUT` 분기 내부 수정
  - `SuspendThread` 및 `GetThreadContext` 수행
  - `IsGuestInstructionPointer` 또는 `IsAotCacheAddress`로 안전성 확인 후 `RecoverToHost` 및 `SetThreadContext` 적용
  - EIP 변경 후 `ResumeThread`를 통해 클린 종료 대기
  - 타임아웃 상황이므로 텔레메트리나 상세 정보를 포함하기 위해 `CopySnapshotFromContextRecord` 등을 통해 `attempt->timeout_snapshot`을 조작 전 컨텍스트로 업데이트
  - 일정 시간 이내에 종료되지 않거나 조작 불가 상태(호스트 HLE 내부)인 경우에만 기존처럼 `TerminateThread` 폴백 수행

## 3. 검증 계획
- 변경 사항 컴파일 확인 (`ninja`)
- 로더 타임아웃 시 139 대신 정상 종료 요약 로그가 출력되는지 확인

## 4. 참조 문서
- Task 235 (current-execution-frontier.md 잔여건)
## Work Order: Fix Loader Timeout Teardown Segfault (exit 139)

## 1. Overview
When execution in AOT mode takes too long and `PollThreadUntilExit` reaches a timeout, the host thread forcefully terminates the guest thread using `TerminateThread`. However, if the guest thread held host resources (like a heap lock) when it was forcefully killed, printing the termination summary with `logger.info` will deadlock or cause a heap-corruption Segfault (exit 139).

To solve this, instead of immediately killing the thread upon timeout, we will use `SuspendThread` and modify the stack/EIP to redirect to the host exception handler/recovery entry (`RecoverToHost`) at a safe moment (when executing pure guest code or inside the AOT cache), inducing a clean shutdown.

## 2. Work Details
- **Modify `src/platform/win32/execution/execution_trampoline.cpp`**
  - Modify the `WAIT_TIMEOUT` branch in `RunWin32ExecutionThread`.
  - Perform `SuspendThread` and `GetThreadContext`.
  - Confirm safety using `IsGuestInstructionPointer` or `IsAotCacheAddress`, then apply `RecoverToHost` and `SetThreadContext`.
  - Wait for clean shutdown via `ResumeThread`.
  - Update `attempt->timeout_snapshot` to the context before modification via `CopySnapshotFromContextRecord` so that telemetry is included for the timeout condition.
  - Fallback to `TerminateThread` only if the thread fails to exit within a certain timeframe or if it's in an unsafe state (inside a host HLE call).

## 3. Verification Plan
- Verify compilation with `ninja`.
- Confirm that the normal termination summary log is output upon loader timeout, instead of an exit 139.

## 4. References
- Task 235 (current-execution-frontier.md remaining items)
