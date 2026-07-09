# Traced C7 memory store HLE 작업 지시

## 목표

`piu_1st`의 현재 `C7 01 FF FF FF FF` memory store 중단 지점을 관측 기반 HLE로 처리해 다음 실행 지점을 확인한다.

## 범위

* Win32 trampoline에 `C7 01 imm32` 처리기를 추가한다.
* arena 내부 destination은 실제 dword write로 처리한다.
* arena 외부 destination은 마지막 DOS open 실패 경로에서만 기록 후 전진한다.
* 같은 out-of-arena metadata 주소를 확인하는 `F7 07 imm32` test를 직전 store shadow 값으로 처리한다.
* loader 로그와 `Win32MinimalExecutionAttempt`에 memory store 관측 정보를 추가한다.
* `scripts/test_all.ps1`의 기대 관측점을 갱신한다.
* 작업 로그와 TODO를 갱신한다.

## 검증

* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# Traced C7 Memory Store HLE Work Order

## Goal

Handle the current `piu_1st` `C7 01 FF FF FF FF` memory store stop through observation-driven HLE and identify the next execution point.

## Scope

* Add a `C7 01 imm32` handler to the Win32 trampoline.
* Perform an actual dword write when the destination is inside the arena.
* For out-of-arena destinations, record and advance only on the last DOS open failure path.
* Handle `F7 07 imm32` tests against the same out-of-arena metadata address using the previous store shadow value.
* Add memory-store observation fields to loader logs and `Win32MinimalExecutionAttempt`.
* Update the expected observation point in `scripts/test_all.ps1`.
* Update the work log and TODO.

## Verification

* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
