# Allocator shadow metadata 89 store 작업 지시

## 목표

기존 allocator sentinel과 구조적으로 연결된 `89 /r` metadata store만 shadow memory로 처리하고 다음 실행 요구사항을 확인한다.

## 범위

* 기존 `89 /r` handler에 shadow sentinel 관계 및 인접 dword 조건을 추가한다.
* 성공한 DOS open 경로의 임의 store는 계속 거부한다.
* 실행 관측에 맞춰 테스트를 갱신한다.
* 작업 로그에 결과와 다음 blocker를 기록한다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

# Allocator Shadow Metadata 89 Store Work Order

## Goal

Handle only `89 /r` metadata stores structurally connected to the existing allocator sentinel through shadow memory and identify the next execution requirement.

## Scope

* Add shadow-sentinel relationship and adjacent-dword conditions to the existing `89 /r` handler.
* Continue rejecting arbitrary stores after a successful DOS open.
* Update tests to the new execution observation.
* Record the result and next blocker in the work log.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
