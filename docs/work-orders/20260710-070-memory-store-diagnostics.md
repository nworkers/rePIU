# Memory store 진단 확장 작업 지시

## 목표

`piu_1st`의 `spr.res` 실패 이후 진행을 해석하기 쉽도록 memory store HLE 로그에 opcode, byte width, source kind를 추가한다.

## 범위

* `Win32MinimalExecutionAttempt`와 `ThreadContext`에 마지막 memory store opcode/width/source kind를 추가한다.
* `HandleTracedMemoryStoreInstruction`와 `HandleTracedFpuMemoryInstruction`에서 source kind를 분류해 기록한다.
* loader 출력과 테스트 기대값을 갱신한다.
* 기존 applied/shadow memory 동작은 변경하지 않는다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# Memory Store Diagnostics Work Order

## Goal

Add opcode, byte width, and source kind to memory store HLE logs so the post-`spr.res` `piu_1st` progress is easier to interpret.

## Scope

* Add last memory store opcode/width/source kind to `Win32MinimalExecutionAttempt` and `ThreadContext`.
* Classify source kind in `HandleTracedMemoryStoreInstruction` and `HandleTracedFpuMemoryInstruction`.
* Update loader output and test expectations.
* Keep the existing applied/shadow memory behavior unchanged.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
