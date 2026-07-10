# DOS path request trace 작업 지시

## 목표

`piu_1st`의 로드 실패 원인을 확인하기 위해 `chdir`, `getcwd`, `getdrive`, `open` 호출 순서를 로그로 남긴다.

## 범위

* `Win32MinimalExecutionAttempt`와 `ThreadContext`에 DOS path trace 관측 구조를 추가한다.
* `chdir`, `getcwd`, `getdrive`, `open` 처리 지점에서 trace entry를 기록한다.
* Win32 loader 로그에 trace entry를 출력한다.
* 현재 테스트 기준에 trace 출력 검증을 추가한다.
* 작업 로그에 관측 결과를 남긴다.

## 제외 범위

* DOS current-directory 의미 변경
* root asset fallback
* file read 구현
* drive별 current-directory table

## 검증

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`

# DOS Path Request Trace Work Order

## Goal

Log the call order of `chdir`, `getcwd`, `getdrive`, and `open` to investigate the `piu_1st` load failure.

## Scope

* Add DOS path trace observation structures to `Win32MinimalExecutionAttempt` and `ThreadContext`.
* Record trace entries from `chdir`, `getcwd`, `getdrive`, and `open`.
* Print trace entries in the Win32 loader log.
* Add trace output expectations to the current test.
* Record the observation result in the work log.

## Out Of Scope

* Changing DOS current-directory semantics
* Root asset fallback
* File read implementation
* Per-drive current-directory tables

## Verification

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
