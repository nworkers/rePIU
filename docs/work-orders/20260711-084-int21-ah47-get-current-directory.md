# INT 21h AH=47h 현재 디렉터리 조회 작업 지시

## 목표

`piu_1st`가 DOS `INT 21h AH=47h` 현재 디렉터리 조회에서 중단되지 않도록 HLE 처리를 추가한다.

## 범위

* 공용 DOS virtual filesystem에 현재 디렉터리 문자열 조회 helper를 추가한다.
* Win32 execution trampoline에 `AH=47h` 처리 helper를 추가한다.
* 일반/traced `INT 21h` 처리기에 `AH=47h` case를 추가한다.
* 실행 결과와 테스트 기대치를 새 중단점에 맞게 갱신한다.
* 작업 결과를 작업 로그로 남긴다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

# INT 21h AH=47h Get Current Directory Work Order

## Goal

Add HLE handling so `piu_1st` does not stop at DOS `INT 21h AH=47h` get current directory.

## Scope

* Add a current-directory string helper to the shared DOS virtual filesystem.
* Add an `AH=47h` handling helper to the Win32 execution trampoline.
* Add an `AH=47h` case to both normal and traced `INT 21h` handling.
* Update the run result and test expectations to the new blocker.
* Record the result in a work log.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
