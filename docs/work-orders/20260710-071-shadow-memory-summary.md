# Shadow memory 요약 진단 작업 지시

## 목표

`piu_1st`의 post-`spr.res` shadow memory 사용 범위를 로그로 확인할 수 있게 한다.

## 범위

* `ThreadContext`와 `Win32MinimalExecutionAttempt`에 shadow write/read/byte/range 요약 필드를 추가한다.
* `WriteShadowMemory`와 `ReadShadowUInt32`에서 요약값을 갱신한다.
* loader 출력과 `scripts/test_all.ps1` 기대값을 갱신한다.
* shadow memory 내용이나 정책은 변경하지 않는다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`

# Shadow Memory Summary Work Order

## Goal

Make the post-`spr.res` shadow memory usage range visible in logs for `piu_1st`.

## Scope

* Add shadow write/read/byte/range summary fields to `ThreadContext` and `Win32MinimalExecutionAttempt`.
* Update the summary values from `WriteShadowMemory` and `ReadShadowUInt32`.
* Update loader output and `scripts/test_all.ps1` expectations.
* Do not change shadow memory contents or behavior.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
