# DOS environment redacted log 작업 지시

## 목표

DOS environment HLE가 guest에 전달한 environment entry를 로그에서 추적할 수 있도록 하되, 실제 값은 출력하지 않는다.

## 범위

* Win32 실행 attempt에 마지막 DOS environment 접근 요약 필드를 추가한다.
* guest가 DS low-memory environment block을 읽을 때 해당 offset이 속한 environment entry 이름과 value 길이를 기록한다.
* loader는 `NAME=<redacted>` 형식과 value byte 길이를 출력한다.
* 자동 테스트는 redacted environment 로그가 출력되는지 확인하도록 갱신한다.

## 정책

* environment value 원문은 구조체와 로그에 저장하지 않는다.
* 변수 이름, entry offset, 읽은 offset, value byte 길이만 진단값으로 남긴다.
* environment block 범위 밖 읽기는 기존처럼 0을 반환하고 별도 entry 로그로 기록하지 않는다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# DOS Environment Redacted Log Work Order

## Goal

Make DOS environment HLE transfers visible in logs without printing actual environment values.

## Scope

* Add last DOS environment access summary fields to the Win32 execution attempt.
* When the guest reads the DS low-memory environment block, record the environment entry name and value length for the accessed offset.
* Make the loader print the entry as `NAME=<redacted>` with the value byte length.
* Update automated tests to check that the redacted environment log is printed.

## Policy

* Do not store or print raw environment values.
* Record only the variable name, entry offset, read offset, and value byte length.
* Reads outside the environment block still return `0` and are not recorded as an entry access.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
