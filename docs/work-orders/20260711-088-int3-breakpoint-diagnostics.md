# INT3 breakpoint diagnostics 작업 지시

## 목표

`piu_1st`가 `INT3` opcode `0xCC`에서 중단될 때 이를 unknown opcode가 아니라 guest breakpoint trap으로 분류하고, 종료 전 진단 정보를 확장한다.

## 범위

* privileged instruction 분류기에 `INT3`/breakpoint 분류를 추가한다.
* loader의 classification 출력에서 breakpoint trap을 별도 블로커 메시지로 표시한다.
* Win32 execution attempt에 예외 스냅샷을 추가하고 출력한다.
* `scripts/test_all.ps1`의 `piu_1st` 기대값을 새 로그에 맞춘다.
* 작업 로그를 남긴다.

## 제외 범위

* `INT3` 이후 실행 계속 옵션
* DOS 파일 오픈 실패 원인 수정
* DOS open trace buffer 확장

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

# INT3 Breakpoint Diagnostics Work Order

## Goal

When `piu_1st` stops at the `INT3` opcode `0xCC`, classify it as a guest breakpoint trap instead of an unknown opcode and expand the diagnostic dump before exit.

## Scope

* Add `INT3`/breakpoint classification to the privileged instruction classifier.
* Print breakpoint traps with a dedicated blocker message in the loader classification output.
* Add and print an exception snapshot in the Win32 execution attempt.
* Update the `piu_1st` expectations in `scripts/test_all.ps1`.
* Add a work log.

## Out Of Scope

* Continuing execution after `INT3`
* Fixing the DOS file-open failure cause
* Expanding the DOS open trace buffer

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
