# Port I/O 0x02A0 allow-list 확장 작업 로그

## 요약

`piu_1st`가 Port I/O 라우터 추가 후 도달한 `OUT DX,EAX port=0x02A0 value=0x00000001`을 관측 기반 allow-list no-op으로 처리했다.

기존 정책대로 모든 Port I/O를 통과시키지 않고, 관측된 조합 하나만 추가했다. 결과는 기존 `0x02AC/0x10`과 같은 `ignored`로 기록된다.

## 변경 내용

* Port I/O allow-list에 `port=0x02A0`, `value=0x00000001`, `width=4`를 추가했다.
* `scripts/test_all.ps1`의 `piu_1st` baseline을 다음 Port I/O 관측 지점으로 갱신했다.

## 결과

`0x02A0/0x00000001` write는 통과되었고, 다음 관측 지점은 같은 `OUT DX,EAX` wrapper의 `0x02A2/0x00000000` write가 되었다.

새 관측 지점:

* exception code: `0xC0000096`
* exception address: `0x020F5726`
* exception EAX: `0x00000000`
* exception EDX: `0x000002A2`
* Port I/O observation count: `3`
* last port I/O port: `0x02A2`
* last port I/O value: `0x00000000`
* last port I/O result: `unsupported`

따라서 다음 작업은 연속된 `0x02A0` 계열 port write들을 개별 allow-list로 계속 누적할지, `0x02A0` 포트 영역을 별도 장치 후보로 묶어야 할지 판단하는 것이다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
  * 통과
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * 확인: `0x02A0` Port I/O 처리 후 `0x02A2` Port I/O blocker 도달
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * 통과

# Port I/O 0x02A0 Allow-list Extension Work Log

## Summary

Handled `OUT DX,EAX port=0x02A0 value=0x00000001`, reached by `piu_1st` after adding the Port I/O router, as an observation-based allow-listed no-op.

Following the existing policy, this does not pass every Port I/O through. It adds only one observed combination. The result is recorded as `ignored`, like the previous `0x02AC/0x10` case.

## Changes

* Added `port=0x02A0`, `value=0x00000001`, `width=4` to the Port I/O allow-list.
* Updated the `piu_1st` baseline in `scripts/test_all.ps1` to the next Port I/O observation point.

## Result

The `0x02A0/0x00000001` write is now handled, and the next observation point is the `0x02A2/0x00000000` write through the same `OUT DX,EAX` wrapper.

New observation point:

* exception code: `0xC0000096`
* exception address: `0x020F5726`
* exception EAX: `0x00000000`
* exception EDX: `0x000002A2`
* Port I/O observation count: `3`
* last port I/O port: `0x02A2`
* last port I/O value: `0x00000000`
* last port I/O result: `unsupported`

Therefore, the next task is to decide whether to keep accumulating the adjacent `0x02A0`-family port writes as individual allow-list entries, or group the `0x02A0` port area as a candidate device.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
  * Passed
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * Checked: reached the `0x02A2` Port I/O blocker after handling `0x02A0`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * Passed
