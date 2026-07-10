# Timeout execution observation 작업 지시

## 목표

`piu_1st` timeout 시점의 guest `EIP`와 레지스터 상태를 구조체 기반 snapshot으로 기록하고 출력한다.

## 범위

* `X86ExecutionSnapshot` 구조체를 추가한다.
* `Win32MinimalExecutionAttempt`에 `timeout_snapshot`을 추가한다.
* Win32 timeout 처리에서 bounded polling 기반 thread timeout 판정을 추가한다.
* timeout snapshot 구조와 loader 출력 경로를 추가한다.
* 현재 `piu_1st` timeout 상태에서 loader를 멈추게 하는 강제 `SuspendThread`/`GetThreadContext` 캡처는 활성화하지 않는다.
* timeout 경로에서는 결과 출력 전 placed image를 해제하지 않는다.
* loader 출력에 timeout context capture와 주요 레지스터를 추가한다.
* `scripts/test_all.ps1`의 `piu_1st` 기대값을 snapshot 출력 기준으로 갱신한다.
* `ARCHITECTURE.md`와 작업 로그를 갱신한다.

## 제외 범위

* instruction-level trace 또는 sampling trace는 이번 작업에 포함하지 않는다.
* 기존 exception register 필드 통합은 후속 정리 작업으로 남긴다.
* timeout `EIP`의 특정 값을 테스트에 고정하지 않는다.

## 검증

* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# Timeout Execution Observation Work Order

## Goal

Record and print the guest `EIP` and register state at the `piu_1st` timeout point through a structure-based snapshot.

## Scope

* Add an `X86ExecutionSnapshot` structure.
* Add `timeout_snapshot` to `Win32MinimalExecutionAttempt`.
* Add bounded polling-based thread timeout classification.
* Add the timeout snapshot structure and loader output path.
* Do not enable forced `SuspendThread`/`GetThreadContext` capture because it hangs the loader in the current `piu_1st` timeout state.
* Do not release the placed image before result output on the timeout path.
* Add loader output for timeout context capture and key registers.
* Update the `piu_1st` expectation in `scripts/test_all.ps1` for snapshot output.
* Update `ARCHITECTURE.md` and the work log.

## Out Of Scope

* Instruction-level trace or sampling trace is not included in this task.
* Consolidating existing exception register fields is left as a follow-up cleanup.
* Do not pin a specific timeout `EIP` value in tests.

## Verification

* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
