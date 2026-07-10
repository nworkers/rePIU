# Shadow memory 요약 진단 작업 로그

## 변경 내용

`piu_1st`의 `spr.res` open 실패 이후 shadow memory 사용 범위를 확인할 수 있도록 요약 진단 필드를 추가했다.

`Win32MinimalExecutionAttempt`와 `ThreadContext`에 다음 필드를 추가했다.

* shadow memory write count
* shadow memory read hit count
* shadow memory byte count
* shadow memory range valid
* shadow memory min/max address

`WriteShadowMemory`는 shadow write call count와 min/max address를 갱신한다. `shadow memory byte count`는 attempt 복사 시점의 `shadow_memory.size()`로 계산한다. `ReadShadowUInt32`는 shadow memory에서 4바이트를 모두 찾은 경우 read hit count를 증가시킨다.

loader와 `scripts/test_all.ps1`도 shadow memory summary가 출력되는지 확인하도록 갱신했다.

## 결과

최근 `piu_1st` 실행에서는 다음 범위가 관측되었다.

* shadow memory write count: 약 `3600`-`3900`
* shadow memory read hit count: 약 `1500`-`1600`
* shadow memory byte count: 약 `13KB`-`14KB`
* shadow memory min address: `0x025E7000`
* shadow memory max address: `0x02670E57`

shadow write가 단일 변수 수준이 아니라 runtime arena 끝 이후 넓은 범위로 확장된다. 따라서 다음 작업은 shadow memory를 계속 임시 보관하는 것보다, DOS/4GW resize 이후 확장된 guest memory range를 실제로 commit/protect하는 방향을 검토하는 것이 적절하다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
  * 결과: 성공
  * 참고: 기존 spdlog code page 경고는 유지됨
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * 결과: 성공
  * 확인: shadow memory write/read/byte/range 출력
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * 첫 실행 결과: `piu_1st` process timeout 발생
  * 재실행 결과: 성공

# Shadow Memory Summary Work Log

## Changes

Added summary diagnostics so the post-`spr.res` shadow memory usage range is visible for `piu_1st`.

Added the following fields to `Win32MinimalExecutionAttempt` and `ThreadContext`.

* shadow memory write count
* shadow memory read hit count
* shadow memory byte count
* shadow memory range valid
* shadow memory min/max address

`WriteShadowMemory` now updates the shadow write call count and min/max address. `shadow memory byte count` is computed from `shadow_memory.size()` when copying observations into the attempt. `ReadShadowUInt32` increments the read hit count when all four bytes are found in shadow memory.

The loader and `scripts/test_all.ps1` now check that the shadow memory summary is printed.

## Result

Recent `piu_1st` runs observed this range.

* shadow memory write count: about `3600`-`3900`
* shadow memory read hit count: about `1500`-`1600`
* shadow memory byte count: about `13KB`-`14KB`
* shadow memory min address: `0x025E7000`
* shadow memory max address: `0x02670E57`

The shadow writes are not limited to a single variable; they extend broadly past the current runtime arena end. The next appropriate task is to investigate committing/protecting the expanded guest memory range after DOS/4GW resize instead of continuing to rely on shadow memory.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
  * Result: passed
  * Note: the existing spdlog code page warning remains
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * Result: passed
  * Checked: shadow memory write/read/byte/range were printed
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * First run: `piu_1st` process timeout
  * Rerun: passed
