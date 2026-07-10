# FS segment word memory load HLE 작업 지시

## 목표

`piu_1st`가 `66 65 8B /r` FS segment word load 지점에서 중단되지 않도록 제한 HLE를 추가한다.

## 범위

* FS low-offset word read helper를 추가한다.
* `66 65 8B /r` 디코더와 16-bit register write helper를 추가한다.
* segment HLE 경로에 새 handler를 연결한다.
* 실행 결과와 테스트 기대치를 새 중단점에 맞게 갱신한다.
* 작업 로그를 남긴다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

# FS Segment Word Memory Load HLE Work Order

## Goal

Add limited HLE so `piu_1st` does not stop at the `66 65 8B /r` FS segment word load point.

## Scope

* Add an FS low-offset word read helper.
* Add a `66 65 8B /r` decoder and 16-bit register write helper.
* Wire the new handler into the segment HLE path.
* Update the run result and test expectations to the new blocker.
* Add a work log.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
