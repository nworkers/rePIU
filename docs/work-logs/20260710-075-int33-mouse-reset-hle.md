# INT 33h AX=0000h/0002h mouse HLE 작업 로그

## 요약

`piu_1st`가 `INT 31h AX=0400h` 이후 도달한 `INT 33h` mouse interrupt를 최소 HLE로 처리했다.

처음 관측된 호출은 `AX=0000h` mouse reset/status였다. “마우스 드라이버 없음” 정책으로 `AX=0000h`, `BX=0000h`를 반환했다. 그 뒤 guest가 `AX=0002h` hide cursor를 호출했으므로, 실제 cursor state가 없는 현재 단계에서는 no-op으로 처리했다.

## 변경 내용

* 일반 HLE dispatch에 `INT 33h` handler를 추가했다.
* trace pre-handler에 `HandleTracedMouseInterrupt33`을 추가했다.
* `AX=0000h`는 mouse driver not installed로 응답한다.
* `AX=0002h`는 hide cursor no-op으로 처리한다.
* 처리된 mouse interrupt도 마지막 interrupt vector/AX 진단 정보로 기록한다.
* `scripts/test_all.ps1`의 `piu_1st` 기대 관측 지점을 새 port I/O blocker로 갱신했다.

## 결과

`INT 33h AX=0000h/0002h`는 처리되었고, `piu_1st`는 다음 관측 지점으로 진행했다.

새 관측 지점:

* exception code: `0xC0000096`
* exception address: `0x0203505F`
* exception bytes: `66 EF`
* exception EAX: `0x00000010`
* exception EDX: `0x000002AC`
* last handled interrupt vector: `0x33`
* last handled interrupt AX: `0x0002`
* privileged instruction mnemonic: `port I/O`

따라서 다음 작업은 `OUT DX, EAX` 포트 I/O를 어떤 하드웨어 또는 DOS extender HLE 서비스로 라우팅할지 설계하는 것이다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
  * 통과
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * 확인: `INT 33h AX=0000h/0002h` 처리 후 port I/O blocker 도달
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * 통과

# INT 33h AX=0000h/0002h Mouse HLE Work Log

## Summary

Handled the `INT 33h` mouse interrupt reached by `piu_1st` after `INT 31h AX=0400h` with minimal HLE.

The first observed call was `AX=0000h`, mouse reset/status. Following the “mouse driver not installed” policy, it returns `AX=0000h`, `BX=0000h`. The guest then called `AX=0002h`, hide cursor, so the current step treats it as a no-op because there is no real cursor state yet.

## Changes

* Added an `INT 33h` handler to the general HLE dispatch.
* Added `HandleTracedMouseInterrupt33` to the trace pre-handler.
* Responded to `AX=0000h` as mouse driver not installed.
* Treated `AX=0002h` as a hide-cursor no-op.
* Recorded handled mouse interrupts in the last interrupt vector/AX diagnostics.
* Updated the `piu_1st` expected observation point in `scripts/test_all.ps1` to the new port I/O blocker.

## Result

`INT 33h AX=0000h/0002h` is now handled, and `piu_1st` progressed to the next observation point.

New observation point:

* exception code: `0xC0000096`
* exception address: `0x0203505F`
* exception bytes: `66 EF`
* exception EAX: `0x00000010`
* exception EDX: `0x000002AC`
* last handled interrupt vector: `0x33`
* last handled interrupt AX: `0x0002`
* privileged instruction mnemonic: `port I/O`

Therefore, the next task is to design how `OUT DX, EAX` port I/O should be routed to a hardware or DOS extender HLE service.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
  * Passed
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * Checked: reached the port I/O blocker after handling `INT 33h AX=0000h/0002h`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * Passed
