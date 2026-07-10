# piu_1st timeout single-step trace 작업 지시

## 목표

`piu_1st`가 예외 없이 timeout에 도달하는 현재 상태에서, 다음 문제 지점을 찾기 위해 마지막으로 관측된 guest `EIP`를 출력한다.

## 범위

* Win32 guest-stack HLE 실행 경로에 진단용 single-step trace 모드를 추가한다.
* vectored exception handler가 `EXCEPTION_SINGLE_STEP`를 처리하여 guest runtime 범위 안의 `EIP`와 주요 레지스터를 기록한다.
* timeout 결과에 마지막 single-step trace snapshot과 trace count를 포함한다.
* loader 출력에 마지막 trace snapshot을 추가한다.
* `piu_1st` 테스트 기준을 새 진단 출력에 맞춰 갱신한다.

## 제외 범위

* 전체 instruction log 파일을 남기지는 않는다.
* forced `SuspendThread`/`GetThreadContext` timeout capture는 다시 활성화하지 않는다.
* 관측된 `EIP`를 영구 성공 기준으로 고정하지 않는다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# piu_1st Timeout Single-Step Trace Work Order

## Goal

Print the last observed guest `EIP` so the next `piu_1st` issue can be identified when the target reaches the current timeout without an exception.

## Scope

* Add a diagnostic single-step trace mode to the Win32 guest-stack HLE execution path.
* Let the vectored exception handler process `EXCEPTION_SINGLE_STEP` and record `EIP` plus key registers when the instruction pointer is inside the guest runtime range.
* Include the last single-step trace snapshot and trace count in timeout results.
* Print the last trace snapshot from the loader.
* Update the `piu_1st` test expectation for the new diagnostic output.

## Out Of Scope

* Do not write a full instruction log file.
* Do not re-enable forced `SuspendThread`/`GetThreadContext` timeout capture.
* Do not pin the observed `EIP` as a permanent success criterion.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
