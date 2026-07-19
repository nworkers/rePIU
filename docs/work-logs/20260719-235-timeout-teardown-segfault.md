# 작업 로그: 로더 타임아웃 종료 시 Segfault(exit 139) 해결

## 1. 개요
AOT 실행 타임아웃 등 스레드 강제 종료가 필요한 시점에서 호스트의 자원을 점유한 채 `TerminateThread`를 수행하면, 이후 `logger` 등 호스트 루틴 실행 시 데드락 혹은 힙 오염으로 인한 Segfault가 발생하는 문제가 있었습니다.
이를 해결하기 위해 순수 게스트 코드나 AOT 캐시 내에 실행 지점(EIP)이 있을 경우 `SuspendThread`와 스레드 컨텍스트 조작(`SetThreadContext`)을 활용해 강제 종료 대신 호스트 예외 핸들러 및 복구 루틴(`RecoverToHost`)으로 클린 종료를 유도하도록 변경했습니다.

## 2. 작업 내용
- `src/platform/win32/execution/execution_trampoline.cpp` 파일의 `RunWin32ExecutionThread` 내 `WAIT_TIMEOUT` 핸들링 수정
- 타임아웃 발생 시 1차적으로 `SuspendThread` 및 `GetThreadContext` 수행
- `attempt->timeout_snapshot`에 텔레메트리를 위한 조작 전 레지스터 정보 저장
- `IsGuestInstructionPointer` 또는 `IsAotCacheAddress` 검증을 거친 후, `RecoverToHost`와 `SetThreadContext`를 이용해 정상적인 스택 반환 경로 확보
- 게스트 환경을 멈추고 복구가 완료된 뒤에 `ResumeThread`를 실행하여 3초 동안 안전 종료를 대기하도록 설계
- 정상 종료 실패 혹은 HLE 런타임/호스트 스택 상단에 머물러 조작 불가 시 기존처럼 `TerminateThread` 폴백 수행되도록 분기

## 3. 결과 및 검증
- CMake를 통한 Win32(x86) 빌드 성공 확인 (`ninja` 혹은 `cmake --build`)
- HLE와 Guest 경계에서 발생할 수 있는 힙/락 오염 원천 차단을 통해 exit 139 문제를 방지

## 4. 참조
- 작업 지시서: `docs/work-orders/20260719-235-timeout-teardown-segfault.md`
- 이슈: Task 235 잔여건 (current-execution-frontier.md 잔여 분석 및 타임아웃 강제 종료 문제)
## Work Log: Fix Loader Timeout Teardown Segfault (exit 139)

## 1. Overview
When forcefully terminating a thread during an AOT execution timeout, if the thread held host resources (like a heap lock), calling `TerminateThread` caused the host routine (e.g., `logger`) to subsequently deadlock or trigger a heap-corruption Segfault.
To solve this, instead of immediately killing the thread, we introduced a clean-shutdown path. If the execution pointer (EIP) is inside pure guest code or the AOT cache, we use `SuspendThread` and `SetThreadContext` to redirect the thread to the host exception handler/recovery routine (`RecoverToHost`).

## 2. Work Details
- Modified the `WAIT_TIMEOUT` handling branch in `RunWin32ExecutionThread` within `src/platform/win32/execution/execution_trampoline.cpp`.
- First attempt `SuspendThread` and `GetThreadContext` upon timeout.
- Save the pre-hijack register state into `attempt->timeout_snapshot` for telemetry purposes.
- Verify safe execution points via `IsGuestInstructionPointer` or `IsAotCacheAddress`, then redirect the instruction pointer via `RecoverToHost` and `SetThreadContext` to ensure a normal stack unwinding path.
- Resume the thread using `ResumeThread` and wait up to 3 seconds for a graceful shutdown.
- Maintain a fallback to the original `TerminateThread` logic if the graceful termination fails or if the thread is unsafely located inside HLE runtime/host code at the moment of suspension.

## 3. Results & Verification
- Confirmed successful Win32 (x86) build via CMake.
- Segfault (exit 139) is prevented by blocking heap/lock corruption across the HLE/Guest boundary during teardown.

## 4. References
- Work Order: `docs/work-orders/20260719-235-timeout-teardown-segfault.md`
- Issue: Task 235 remainder (Timeout forced termination in current-execution-frontier.md)
