# Arena 외부 allocator sentinel store 작업 지시

## 목표

`piu_1st`의 arena 외부 allocator 실패 sentinel 쓰기를 제한적으로 shadow 처리하고 다음 요구사항을 확인한다.

## 범위

* 기존 `C7 /0` handler에 값과 주소 범위가 제한된 allocator sentinel 정책을 추가한다.
* `piu_1st`의 새 실행 중단점에 맞춰 검증 기대값을 갱신한다.
* 결과와 다음 blocker를 작업 로그에 기록한다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

# Out-of-Arena Allocator Sentinel Store Work Order

## Goal

Handle the `piu_1st` out-of-arena allocator failure sentinel through a constrained shadow store and identify the next requirement.

## Scope

* Add a value- and range-constrained allocator sentinel policy to the existing `C7 /0` handler.
* Update verification expectations to the new `piu_1st` stop.
* Record the result and next blocker in the work log.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
