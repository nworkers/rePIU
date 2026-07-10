# Runtime arena slack 확장 작업 지시

## 목표

`piu_1st`의 post-`spr.res` shadow memory write를 줄이기 위해 relocated runtime arena의 관측 기반 expansion slack을 늘린다.

## 범위

* Win32 loader의 runtime arena expansion slack을 `0x00100000`으로 조정한다.
* 변경된 arena reserve size에 맞게 테스트 기대값을 갱신한다.
* `piu_1st` 실행에서 shadow memory write/range 변화를 확인한다.
* shadow memory 안전망은 유지한다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# Runtime Arena Slack Expansion Work Order

## Goal

Increase the relocated runtime arena's observed expansion slack to reduce post-`spr.res` shadow memory writes in `piu_1st`.

## Scope

* Change the Win32 loader runtime arena expansion slack to `0x00100000`.
* Update test expectations for the changed arena reserve size.
* Verify the shadow memory write/range changes in a `piu_1st` run.
* Keep the shadow memory safety net.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
