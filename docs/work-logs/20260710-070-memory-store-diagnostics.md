# Memory store 진단 확장 작업 로그

## 변경 내용

`piu_1st`의 `spr.res` 실패 이후 memory store HLE 흐름을 해석하기 쉽도록 마지막 memory store 진단 필드를 확장했다.

`Win32MinimalExecutionAttempt`와 `ThreadContext`에 다음 필드를 추가했다.

* last memory store opcode
* last memory store byte width
* last memory store source kind

`HandleTracedMemoryStoreInstruction`은 다음 source kind를 기록한다.

* `mov-imm32`
* `mov-imm16`
* `mov-reg32`

`HandleTracedFpuMemoryInstruction`은 `fpu-m32`로 기록한다. 기존 `applied` 의미는 유지했다. `applied=false`는 `spr.res` open 실패 이후 실제 guest writable range가 아닌 shadow memory로 기록된 store를 뜻한다.

loader 출력과 `scripts/test_all.ps1` 기대값도 opcode, width, source kind를 확인하도록 갱신했다.

## 결과

최근 `piu_1st` 수동 실행에서는 마지막 memory store가 다음과 같이 출력되었다.

* opcode: `0x89`
* width: `4`
* source kind: `mov-reg32`
* applied: `false`

전체 테스트 실행에서는 timing 차이로 마지막 store가 `0xC7`, `mov-imm32`, value `0x3F800000`, `applied=false`로 관측되었다. 마지막 store는 timeout 시점에 따라 달라질 수 있으므로 테스트는 특정 source kind 하나에 고정하지 않고, 진단 필드가 출력되는지만 확인한다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
  * 결과: 성공
  * 참고: 기존 spdlog code page 경고는 유지됨
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * 결과: 성공
  * 확인: memory store opcode/width/source kind 출력
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * 결과: 성공

# Memory Store Diagnostics Work Log

## Changes

Expanded the last memory store diagnostics so the post-`spr.res` `piu_1st` memory store HLE flow is easier to interpret.

Added the following fields to `Win32MinimalExecutionAttempt` and `ThreadContext`.

* last memory store opcode
* last memory store byte width
* last memory store source kind

`HandleTracedMemoryStoreInstruction` records these source kinds:

* `mov-imm32`
* `mov-imm16`
* `mov-reg32`

`HandleTracedFpuMemoryInstruction` records `fpu-m32`. The existing `applied` meaning is unchanged. `applied=false` means the store was written to shadow memory after the `spr.res` open failure rather than to a real guest writable range.

The loader output and `scripts/test_all.ps1` expectations now check opcode, width, and source kind.

## Result

A recent manual `piu_1st` run printed the last memory store as:

* opcode: `0x89`
* width: `4`
* source kind: `mov-reg32`
* applied: `false`

The full test run observed a different last store because of timeout timing: `0xC7`, `mov-imm32`, value `0x3F800000`, `applied=false`. Since the last store can vary with timeout timing, the test checks that the diagnostic fields are printed instead of pinning one source kind.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
  * Result: passed
  * Note: the existing spdlog code page warning remains
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * Result: passed
  * Checked: memory store opcode/width/source kind were printed
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * Result: passed
