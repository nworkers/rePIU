# Port I/O 0x02A0 계열 초기화 write 작업 로그

## 요약

`piu_1st`가 도달한 `OUT DX,EAX port=0x02A2 value=0x00000000`을 관측 기반 allow-list no-op으로 처리하고, 기존 Port I/O allow-list 조건을 helper로 묶었다.

이번 작업은 `0x02A0` 포트 범위 전체를 여는 것이 아니라, 지금까지 관측된 정확한 port/value 조합만 `ignored`로 통과시키는 정책을 유지한다.

## 변경 내용

* `IsObservedPortInitializationWrite` helper를 추가했다.
* helper 안에 기존 `0x02AC/0x10`, `0x02A0/0x1` 조합과 새 `0x02A2/0x0` 조합을 묶었다.
* `scripts/test_all.ps1`의 `piu_1st` baseline을 다음 Port I/O 관측 지점으로 갱신했다.

## 결과

`0x02A2/0x00000000` write는 통과되었고, 다음 관측 지점은 `0x02A0/0x00000005` write가 되었다.

새 관측 지점:

* exception code: `0xC0000096`
* exception address: `0x020F5726`
* exception EAX: `0x00000005`
* exception EDX: `0x000002A0`
* Port I/O observation count: `4`
* last port I/O port: `0x02A0`
* last port I/O value: `0x00000005`
* last port I/O result: `unsupported`

다음 작업에서는 `0x02A0` 계열 초기화 write의 패턴이 충분히 분명해졌는지 보고, exact allow-list를 더 확장할지 아니면 작은 장치 후보로 문서화할지 판단한다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
  * 통과
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * 확인: `0x02A2` Port I/O 처리 후 `0x02A0/0x5` Port I/O blocker 도달
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * 통과

# Port I/O 0x02A0-family Initialization Write Work Log

## Summary

Handled `OUT DX,EAX port=0x02A2 value=0x00000000`, reached by `piu_1st`, as an observation-based allow-listed no-op and grouped the existing Port I/O allow-list predicates into a helper.

This task does not open the whole `0x02A0` port range. It keeps the policy of passing only exact observed port/value combinations as `ignored`.

## Changes

* Added `IsObservedPortInitializationWrite`.
* Grouped the existing `0x02AC/0x10`, `0x02A0/0x1`, and new `0x02A2/0x0` combinations in the helper.
* Updated the `piu_1st` baseline in `scripts/test_all.ps1` to the next Port I/O observation point.

## Result

The `0x02A2/0x00000000` write is now handled, and the next observation point is the `0x02A0/0x00000005` write.

New observation point:

* exception code: `0xC0000096`
* exception address: `0x020F5726`
* exception EAX: `0x00000005`
* exception EDX: `0x000002A0`
* Port I/O observation count: `4`
* last port I/O port: `0x02A0`
* last port I/O value: `0x00000005`
* last port I/O result: `unsupported`

The next task should decide whether the `0x02A0`-family initialization write pattern is clear enough to extend the exact allow-list further or document it as a small device candidate.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
  * Passed
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * Checked: reached the `0x02A0/0x5` Port I/O blocker after handling `0x02A2`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * Passed
