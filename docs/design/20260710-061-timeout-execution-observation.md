# Timeout execution observation 설계

## 목표

`piu_1st`가 새 opcode 예외 없이 `minimal execution timeout`에 도달하는 현재 상태에서, timeout 순간 guest 실행 위치와 레지스터 상태를 기록한다.

이 작업의 목적은 게임 로직을 재구현하는 것이 아니라, 원본 32-bit x86 코드가 어디에서 계속 실행 중인지 확인해 다음 HLE 요구사항을 판단하는 것이다.

## 설계

Win32 minimal execution 경로에 `X86ExecutionSnapshot` 구조체를 추가한다. snapshot은 `EIP`, 범용 레지스터, `EFLAGS`, segment selector를 하나의 값 객체로 묶는다.

`Win32MinimalExecutionAttempt`는 timeout 전용 `timeout_snapshot`을 가진다. 기존 exception register 필드는 출력 호환성과 변경 범위 축소를 위해 이번 작업에서 유지한다. 이후 정리 작업에서 exception 상태도 같은 snapshot 구조로 옮길 수 있다.

timeout 처리 순서는 다음과 같다.

1. guest thread 생성 전에 timeout 처리에 필요한 Kernel32 API 함수 포인터를 고정된 내부 포인터로 사용한다.
2. `WaitForSingleObject`의 blocking wait 대신 `GetExitCodeThread` 기반 bounded polling으로 thread 종료와 timeout을 판정한다. 이 timeout은 현재 진단 경로를 빠져나오기 위한 실행 상한이며 wall-clock 정밀 타이머가 아니다.
3. `Win32MinimalExecutionAttempt`에 `timeout_snapshot` 출력 경로를 추가한다.
4. 강제 `SuspendThread`/`GetThreadContext` 캡처는 현재 `piu_1st` timeout 상태에서 loader를 다시 멈추게 하므로 이번 작업에서는 활성화하지 않는다.
5. `EIP` 주변 byte window는 기존 relocated image byte window 출력 경로가 있으므로 이번 작업에서는 snapshot 구조와 출력만 고정한다.
6. timeout 결과를 반환한 뒤 loader가 관찰 결과를 출력하고 프로세스 종료로 guest thread 정리를 끝낸다. timeout 경로에서는 guest가 아직 실행 중일 수 있으므로 placed image를 명시적으로 해제하지 않는다.
7. 다음 작업은 guest thread를 직접 멈추지 않는 sampling watcher 또는 HLE 경유 trace 방식으로 마지막 실행 위치를 얻는 것이다.

`SuspendThread` 또는 `GetThreadContext`가 실패해도 timeout 자체는 기존처럼 기록한다. 실패 이유는 message에 추가하지 않고, loader 출력의 `timeout context captured: false`로 구분한다. timeout 종료 실패 메시지는 기존 의미를 유지한다.

## 검증

* `repiu_loader_win32.exe piu_1st` 출력에 timeout snapshot이 표시되는지 확인한다.
* `scripts/test_all.ps1`의 `piu_1st` 기대값에 timeout context capture 여부와 timeout `EIP` 출력을 추가한다.

# Timeout Execution Observation Design

## Goal

Record the guest execution location and register state when `piu_1st` reaches the current `minimal execution timeout` without a new opcode exception.

The purpose is not to reimplement game logic, but to identify where the original 32-bit x86 code is still running so the next HLE requirement can be classified.

## Design

Add an `X86ExecutionSnapshot` structure to the Win32 minimal execution path. The snapshot groups `EIP`, general registers, `EFLAGS`, and segment selectors into one value object.

`Win32MinimalExecutionAttempt` gets a dedicated `timeout_snapshot`. Existing exception register fields remain for output compatibility and to keep this task scoped. A later cleanup can move exception state to the same snapshot structure.

The timeout handling sequence is:

1. Use internally captured Kernel32 API function pointers for timeout handling before the guest thread is created.
2. Classify thread exit and timeout with `GetExitCodeThread` bounded polling instead of a blocking `WaitForSingleObject`. This timeout is an execution bound for escaping the diagnostic path, not a precise wall-clock timer.
3. Add the `timeout_snapshot` output path to `Win32MinimalExecutionAttempt`.
4. Forced `SuspendThread`/`GetThreadContext` capture is not enabled in this task because it makes the loader hang again in the current `piu_1st` timeout state.
5. Keep byte-window work out of this step because relocated image byte-window output already exists for exception analysis; this task fixes the snapshot structure and output path.
6. Return the timeout result so the loader can print the observation, then let process termination clean up the guest thread. The timeout path does not explicitly release the placed image because the guest may still be running.
7. The next task should collect the last execution location through a sampling watcher or HLE-routed trace that does not directly stop the guest thread at timeout.

If `SuspendThread` or `GetThreadContext` fails, the timeout is still recorded as before. The failure is represented by `timeout context captured: false` in loader output rather than changing the existing timeout message. Existing timeout termination failure messages keep their current meaning.

## Verification

* Confirm that `repiu_loader_win32.exe piu_1st` prints the timeout snapshot.
* Add timeout context capture and timeout `EIP` checks to the `piu_1st` expectation in `scripts/test_all.ps1`.
