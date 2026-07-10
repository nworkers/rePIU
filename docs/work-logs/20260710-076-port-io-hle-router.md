# Port I/O HLE 라우터 작업 로그

## 요약

`piu_1st`가 `INT 33h AX=0000h/0002h` 이후 도달한 `OUT DX,EAX` Port I/O를 관측 가능한 HLE 라우터로 처리했다.

이번 작업은 특정 하드웨어 장치를 구현하지 않았다. 대신 `66 EF` (`OUT DX,EAX`) 명령을 해석하고, 관측된 `port=0x02AC`, `value=0x00000010`, `width=4` 조합만 allow-list no-op으로 통과시켰다.

## 변경 내용

* `Win32PortIoObservation` 구조체를 추가했다.
* 실행 trampoline 내부에 Port I/O 관측 상태를 추가했다.
* single-step trace와 vectored exception 경로에서 `66 EF`를 처리하도록 연결했다.
* `port=0x02AC`, `value=0x00000010`만 `ignored` 결과로 처리한다.
* 그 외 `66 EF`는 unsupported로 기록하고 다음 관측 지점으로 남긴다.
* loader 로그에 Port I/O 관측 정보를 출력한다.
* `scripts/test_all.ps1`의 `piu_1st` baseline을 새 unsupported Port I/O 지점으로 갱신했다.

## 결과

첫 Port I/O는 allow-list no-op으로 처리되었고, `piu_1st`는 다음 Port I/O 관측 지점으로 진행했다.

새 관측 지점:

* exception code: `0xC0000096`
* exception address: `0x020F5726`
* exception bytes: `66 EF`
* exception EAX: `0x00000001`
* exception EDX: `0x000002A0`
* Port I/O observation count: `2`
* last port I/O address: `0x020F5726`
* last port I/O opcode: `0x66EF`
* last port I/O direction: `out`
* last port I/O port: `0x02A0`
* last port I/O width: `4`
* last port I/O value: `0x00000001`
* last port I/O handled: `false`
* last port I/O result: `unsupported`

따라서 다음 작업은 `port=0x02A0`, `value=0x00000001` 조합을 같은 Port I/O 라우터에서 no-op으로 허용할지, 특정 장치 의미를 부여해야 하는지 결정하는 것이다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
  * 통과
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * 확인: `0x02AC` Port I/O 처리 후 `0x02A0` Port I/O blocker 도달
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * 통과

# Port I/O HLE Router Work Log

## Summary

Handled the `OUT DX,EAX` Port I/O reached by `piu_1st` after `INT 33h AX=0000h/0002h` through an observable HLE router.

This task did not implement a specific hardware device. Instead, it decodes `66 EF` (`OUT DX,EAX`) and allows only the observed `port=0x02AC`, `value=0x00000010`, `width=4` combination as an allow-listed no-op.

## Changes

* Added `Win32PortIoObservation`.
* Added Port I/O observation state to the execution trampoline.
* Connected `66 EF` handling to the single-step trace and vectored exception paths.
* Treated only `port=0x02AC`, `value=0x00000010` as handled with result `ignored`.
* Recorded other `66 EF` cases as unsupported for the next observation point.
* Printed Port I/O observation details in the loader log.
* Updated the `piu_1st` baseline in `scripts/test_all.ps1` to the new unsupported Port I/O point.

## Result

The first Port I/O was handled as an allow-listed no-op, and `piu_1st` progressed to the next Port I/O observation point.

New observation point:

* exception code: `0xC0000096`
* exception address: `0x020F5726`
* exception bytes: `66 EF`
* exception EAX: `0x00000001`
* exception EDX: `0x000002A0`
* Port I/O observation count: `2`
* last port I/O address: `0x020F5726`
* last port I/O opcode: `0x66EF`
* last port I/O direction: `out`
* last port I/O port: `0x02A0`
* last port I/O width: `4`
* last port I/O value: `0x00000001`
* last port I/O handled: `false`
* last port I/O result: `unsupported`

Therefore, the next task is to decide whether the `port=0x02A0`, `value=0x00000001` combination should also be allowed as a no-op through the same Port I/O router, or whether it requires a specific device-level meaning.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
  * Passed
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * Checked: reached the `0x02A0` Port I/O blocker after handling `0x02AC`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * Passed
