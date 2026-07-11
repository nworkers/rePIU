# Arena 경계 객체 shadow chain 작업 지시

## 목표

최초 arena 경계 객체에서 연속된 고정 크기 객체 초기화를 제한된 shadow chain으로 보존하고 다음 요구사항을 확인한다.

## 범위

* ThreadContext에 boundary object base/frontier 상태를 추가한다.
* `66 C7`, `C7`, `89`, `D9` store에 동일한 chain 조건을 적용한다.
* 전체 chain을 arena end 이후 4 KiB로 제한한다.
* 실행 관측과 테스트를 갱신하고 작업 로그를 남긴다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

# Arena-Boundary Object Shadow Chain Work Order

## Goal

Preserve contiguous fixed-size object initialization following the first arena-boundary object through a constrained shadow chain and identify the next requirement.

## Scope

* Add boundary-object base/frontier state to `ThreadContext`.
* Apply the same chain conditions to `66 C7`, `C7`, `89`, and `D9` stores.
* Validate the initial `ESI * EDX` array span and use it as the chain limit.
* Count handled memory stores as diagnostic progress while retaining the existing quiet and execution limits.
* Update execution observations and tests, then leave a work log.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
