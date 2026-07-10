# Port I/O trace buffer 작업 로그

## 변경 내용

`piu_1st`의 `0x02A0` 계열 Port I/O 의미 분석을 위해 Win32 Port I/O 진단 구조에 고정 크기 trace buffer를 추가했다.

추가한 내용은 다음과 같다.

* `Win32PortIoTraceEntry`
* `kWin32PortIoTraceCapacity = 16`
* `Win32PortIoObservation::trace_stored_count`
* `Win32PortIoObservation::trace_limit_reached`
* `Win32PortIoObservation::trace`

Port I/O 기록 시 마지막 관측값뿐 아니라 trace buffer에도 순서대로 저장한다. loader는 저장된 trace entry를 순서대로 출력한다.

기존 exact allow-list는 유지했다. 아직 의미가 확정되지 않은 `0x02A0..0x02AF` 범위의 4-byte `OUT DX,EAX`는 trace buffer 용량 안에서 `trace-ignored`로 진행시킨다. trace buffer가 가득 찬 뒤 같은 계열의 미확정 Port I/O가 계속되면 `trace-limit`으로 중단한다.

## 결과

`piu_1st`는 기존 `0x02A0/0x00000005` 지점 하나에서 멈추지 않고, 제한된 Port I/O 시퀀스를 출력했다.

현재 관측된 핵심 패턴은 다음과 같다.

* `0x02AC <- 0x00000010`
* `0x02A0 <- 0x00000001`
* `0x02A2 <- 0x00000000`
* `0x02A0 <- 0x00000005`
* `0x02A2 <- 0x00000000`
* `0x02A0 <- 0x00000009`
* `0x02A2 <- 0x00000000`
* `0x02A0 <- 0x0000000D`
* 이후 같은 형태로 `0x02A0` 값이 `+4`씩 증가하고 `0x02A2 <- 0`이 반복된다.

trace buffer 용량 이후의 현재 중단 지점은 다음과 같다.

* exception address: `0x020F4386`
* last port I/O port: `0x02A0`
* last port I/O value: `0x000000FE`
* last port I/O result: `trace-limit`
* observed count: `18`
* stored trace count: `16`

이 결과는 `0x02A0/0x02A2`가 단일 exact allow-list라기보다 작은 register block 또는 indexed initialization sequence일 가능성을 높인다. 다음 작업에서는 이 패턴을 근거로 `0x02A0` 계열 small device 후보를 문서화하거나, 더 긴 trace/상위 caller 분석으로 넘어갈 수 있다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
  * 결과: 통과
  * 참고: 기존 spdlog code page warning `C4819`는 유지됨
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * 결과: 통과
  * 확인: Port I/O trace 16개 저장 및 `trace-limit` 중단 확인
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
  * 결과: 통과

sandbox 내부 빌드는 기존과 같이 CMake stamp 파일 접근 거부로 실패했으므로, 빌드와 전체 테스트는 승인된 외부 권한으로 수행했다.

# Port I/O Trace Buffer Work Log

## Changes

Added a fixed-size trace buffer to the Win32 Port I/O diagnostics so the `piu_1st` `0x02A0`-family sequence can be analyzed.

Added:

* `Win32PortIoTraceEntry`
* `kWin32PortIoTraceCapacity = 16`
* `Win32PortIoObservation::trace_stored_count`
* `Win32PortIoObservation::trace_limit_reached`
* `Win32PortIoObservation::trace`

Each Port I/O record now updates both the last observation and the ordered trace buffer. The loader prints stored trace entries in order.

The existing exact allow-list remains in place. Not-yet-understood 4-byte `OUT DX,EAX` operations in the `0x02A0..0x02AF` range continue as `trace-ignored` while trace capacity remains. Once the buffer is full, the same family stops as `trace-limit`.

## Result

`piu_1st` no longer stops at only the previous `0x02A0/0x00000005` point and now prints a bounded Port I/O sequence.

The key observed pattern is:

* `0x02AC <- 0x00000010`
* `0x02A0 <- 0x00000001`
* `0x02A2 <- 0x00000000`
* `0x02A0 <- 0x00000005`
* `0x02A2 <- 0x00000000`
* `0x02A0 <- 0x00000009`
* `0x02A2 <- 0x00000000`
* `0x02A0 <- 0x0000000D`
* Then the same shape continues, with the `0x02A0` value increasing by `+4` and `0x02A2 <- 0` repeating.

Current stop after trace capacity:

* exception address: `0x020F4386`
* last port I/O port: `0x02A0`
* last port I/O value: `0x000000FE`
* last port I/O result: `trace-limit`
* observed count: `18`
* stored trace count: `16`

This makes the `0x02A0/0x02A2` pair look more like a small register block or indexed initialization sequence than isolated exact allow-list entries. A next task can document a `0x02A0`-family small-device candidate or continue with a longer trace/caller analysis.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
  * Result: passed
  * Note: the existing spdlog code page warning `C4819` remains
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * Result: passed
  * Checked: 16 Port I/O trace entries stored and stopped at `trace-limit`
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1 -SkipSetup`
  * Result: passed

The sandboxed build failed with the existing CMake stamp-file access denial, so the build and full test run were executed with approved external permissions.
