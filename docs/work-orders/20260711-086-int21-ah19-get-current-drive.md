# INT 21h AH=19h 현재 드라이브 조회 작업 지시

## 목표

`piu_1st`가 DOS `INT 21h AH=19h` 현재 drive 조회에서 중단되지 않도록 HLE 처리를 추가한다.

## 범위

* `AH=19h` 처리 helper를 추가해 `AL=2`를 반환한다.
* 일반/traced `INT 21h` 처리기에 `AH=19h` case를 추가한다.
* `ThreadContext`와 `Win32MinimalExecutionAttempt`에 get drive 관측 필드를 추가한다.
* Win32 loader 로그와 `scripts/test_all.ps1` 기대치를 갱신한다.
* 실행 결과를 작업 로그로 남긴다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

# INT 21h AH=19h Get Current Drive Work Order

## Goal

Add HLE handling so `piu_1st` does not stop at DOS `INT 21h AH=19h` get current drive.

## Scope

* Add an `AH=19h` helper that returns `AL=2`.
* Add an `AH=19h` case to both normal and traced `INT 21h` handling.
* Add get-drive observation fields to `ThreadContext` and `Win32MinimalExecutionAttempt`.
* Update Win32 loader logging and `scripts/test_all.ps1` expectations.
* Record the result in a work log.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
