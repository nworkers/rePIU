# Arena 경계 객체 memory store 작업 지시

## 목표

실제 arena 내부에서 시작해 경계를 조금 넘는 객체의 `C7 /0` 필드 초기화를 제한적으로 보존하고 다음 요구사항을 확인한다.

## 범위

* arena end 전후 64바이트 창을 사용하는 경계 객체 store 조건을 추가한다.
* 기존 `C7 /0`, `89 /r`, `D9 /2-/3` decoder와 shadow memory 기록을 재사용한다.
* 실행 관측에 맞춰 테스트를 갱신한다.
* 결과와 다음 blocker를 작업 로그에 기록한다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

# Arena-Boundary Object Memory Store Work Order

## Goal

Preserve `C7 /0` field initialization for an object that starts inside the real arena and crosses its boundary by a small amount, then identify the next requirement.

## Scope

* Add an arena-boundary object-store condition using a 64-byte window around arena end.
* Reuse the existing `C7 /0`, `89 /r`, and `D9 /2-/3` decoders and shadow-memory recording.
* Update tests to the new execution observation.
* Record the result and next blocker in the work log.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
