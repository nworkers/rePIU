# relocated arena 선점 예약 작업 지시

## 목표

`piu_1st` 실행이 relocated image 배치 전 `VirtualAlloc` error `487`로 중단되지 않도록, relocated arena 후보 선택 시점에 실제 reserve/commit을 완료한다.

## 범위

* Win32 relocated image placement가 이미 확보된 arena를 사용할 수 있게 API를 확장한다.
* loader의 relocated base 선택 흐름을 실제 arena reserve/commit 성공 기준으로 바꾼다.
* 기존 placement 로그와 release 흐름을 유지한다.
* `piu_1st` 실행으로 현재 blocker가 다시 Port I/O 지점인지 확인한다.
* `scripts/test_all.ps1 -SkipSetup` 또는 가능한 범위의 빌드/실행 검증을 수행한다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`

# Relocated Arena Precommit Work Order

## Goal

Prevent `piu_1st` from stopping before relocated image placement with `VirtualAlloc` error `487` by completing the real reserve/commit during relocated arena candidate selection.

## Scope

* Extend the Win32 relocated image placement API so it can use an already acquired arena.
* Change the loader's relocated base selection flow to use real arena reserve/commit success as the decision point.
* Keep the existing placement logging and release flow.
* Run `piu_1st` and confirm that the current blocker is again the Port I/O point.
* Run `scripts/test_all.ps1 -SkipSetup` or the appropriate build/run verification available in this environment.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
