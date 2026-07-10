# piu_1st timeout single-step trace 작업 로그

## 변경 내용

Win32 guest-stack trap 실행 경로에 진단용 single-step trace를 추가했다. trace는 guest runtime 범위 안의 `EIP`와 주요 레지스터만 원자적으로 기록하며, timeout 결과에 마지막 single-step snapshot과 trace count를 출력한다.

single-step 예외가 guest 명령 실행 직전에 발생하는 점을 이용해, 기존 HLE handler를 선처리 경로에서도 재사용하도록 연결했다. 현재 연결된 범위는 privileged trap, traced DOS `INT 21h`, segment load/store, traced memory load/store/test/FPU, 그리고 low-memory DOS memory access helper다.

timeout 경로에서도 HLE trap, DOS interrupt, segment load/store, memory store 관측 count를 `Win32MinimalExecutionAttempt`로 복사하도록 고쳤다. 이전에는 timeout snapshot만 복사되어 실제 선처리 결과가 출력에 반영되지 않았다.

loader는 마지막 single-step snapshot 주소의 relocated byte window를 출력한다.

## 결과

`piu_1st`는 기존처럼 timeout으로 종료되지만, 이제 다음 진단 정보를 안정적으로 남긴다.

* single-step trace count: `60`
* 마지막 single-step `EIP`: `0x020F4DC1`
* 마지막 byte window focus opcode: `80 3E 00`
* HLE trap count: `1`, 마지막 opcode `0xFB`
* DOS interrupt count: `254`, 마지막 AH `0x4A`
* segment load/store와 memory store count가 timeout 결과에 표시됨

`0x020F4DC1`의 `80 3E 00`은 low-memory 문자열 검사 루프 쪽으로 보인다. 다음 작업은 이 루프가 single-step budget 안에서 더 안정적으로 진행되도록, low-memory 문자열 helper와 timeout 관측 정책을 분리해서 정리하는 것이다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
  * 결과: 성공
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * 결과: timeout 출력 성공, single-step snapshot과 byte window 출력 확인
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * 결과: 성공

# piu_1st Timeout Single-Step Trace Work Log

## Changes

Added a diagnostic single-step trace to the Win32 guest-stack trap execution path. The trace records only guest-runtime `EIP` values and key registers through atomic fields, then prints the last single-step snapshot and trace count in timeout results.

Because single-step exceptions are delivered before the guest instruction executes, the existing HLE handlers are now reused from the pre-execution path. The currently connected scope includes privileged traps, traced DOS `INT 21h`, segment load/store, traced memory load/store/test/FPU, and the low-memory DOS memory access helper.

The timeout path now copies observed HLE trap, DOS interrupt, segment load/store, and memory store counts into `Win32MinimalExecutionAttempt`. Previously only the timeout snapshot was copied, so pre-execution HLE observations were not reflected in output.

The loader now prints a relocated byte window for the last single-step snapshot address.

## Result

`piu_1st` still ends through the current timeout path, but it now leaves stable diagnostic data.

* single-step trace count: `60`
* last single-step `EIP`: `0x020F4DC1`
* last byte window focus opcode: `80 3E 00`
* HLE trap count: `1`, last opcode `0xFB`
* DOS interrupt count: `254`, last AH `0x4A`
* segment load/store and memory store counts are shown in timeout results

The `80 3E 00` at `0x020F4DC1` appears to be part of a low-memory string scanning loop. The next task is to separate the low-memory string helper and timeout observation policy so the loop can progress more reliably within the diagnostic budget.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
  * Result: passed
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * Result: timeout output printed with the single-step snapshot and byte window
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * Result: passed
