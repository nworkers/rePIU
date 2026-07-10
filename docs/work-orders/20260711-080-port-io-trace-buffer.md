# Port I/O trace buffer 작업 지시

## 목표

`piu_1st`의 `0x02A0` 계열 Port I/O 의미 분석을 위해 제한된 연속 trace를 수집한다.

## 범위

* `Win32PortIoObservation`에 고정 크기 trace buffer를 추가한다.
* Port I/O 기록 시 trace buffer에도 순서대로 저장한다.
* loader 출력에 Port I/O trace entry 목록을 추가한다.
* `0x02A0..0x02AF` 범위의 4-byte `OUT DX,EAX`는 trace buffer 용량 안에서 `trace-ignored`로 진행시킨다.
* trace 용량 초과 또는 범위 밖 Port I/O는 중단점으로 유지한다.
* `piu_1st` 테스트 기준을 새 관측 결과에 맞춰 갱신한다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

# Port I/O Trace Buffer Work Order

## Goal

Collect a bounded continuous trace for `piu_1st` `0x02A0`-family Port I/O meaning analysis.

## Scope

* Add a fixed-size trace buffer to `Win32PortIoObservation`.
* Record each Port I/O event into that trace buffer.
* Print the Port I/O trace entries from the loader.
* Continue 4-byte `OUT DX,EAX` operations in the `0x02A0..0x02AF` range as `trace-ignored` while trace capacity remains.
* Keep trace-capacity overflow and out-of-range Port I/O as stopping points.
* Update the `piu_1st` test expectation for the new observation result.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
