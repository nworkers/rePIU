# INT 21h AH=47h 현재 디렉터리 조회 로그 작업 지시

## 목표

`INT 21h AH=47h`가 반환한 현재 디렉터리 문자열을 Win32 loader 로그와 회귀 테스트에서 직접 확인할 수 있게 한다.

## 범위

* `ThreadContext`와 `Win32MinimalExecutionAttempt`에 getcwd 관측 필드를 추가한다.
* `HandleDosGetCurrentDirectory`에서 반환 path/result를 기록한다.
* Win32 loader 로그에 getcwd 결과를 출력한다.
* `scripts/test_all.ps1` 기대치를 갱신한다.
* 작업 로그를 남긴다.

## 검증

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

# INT 21h AH=47h Getcwd Logging Work Order

## Goal

Make the current-directory string returned by `INT 21h AH=47h` directly visible in the Win32 loader log and regression test.

## Scope

* Add getcwd observation fields to `ThreadContext` and `Win32MinimalExecutionAttempt`.
* Record the returned path/result in `HandleDosGetCurrentDirectory`.
* Print getcwd results in the Win32 loader log.
* Update `scripts/test_all.ps1` expectations.
* Add a work log.

## Verification

* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
