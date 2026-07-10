# Low-memory string scan observation 작업 로그

## 변경 내용

low-memory 문자열/환경 접근을 별도 관측 그룹으로 출력하도록 `Win32MinimalExecutionAttempt`와 `ThreadContext`에 low-memory access 필드를 추가했다.

처음 예상한 `HandleDosMemoryAccess`뿐 아니라, 실제 `piu_1st`가 밟는 `HandleSegmentMemoryCompareInstruction`와 `HandleSegmentMemoryLoadInstruction` 경로에도 low-memory access 기록을 연결했다. `0x020F4DC1`의 `80 3E 00`과 뒤따르는 `AC`, `A4`는 DS segment memory helper가 처리하고 있었다.

loader 출력에 low-memory access count와 마지막 address/opcode/ESI/EDI/destination/value를 추가했다. `scripts/test_all.ps1`도 low-memory access 관측을 확인하도록 갱신했다.

## 결과

`piu_1st`의 현재 안정 관측값은 다음과 같다.

* low-memory access count: `7`
* 마지막 low-memory access address: `0x020F4DD2`
* 마지막 opcode: `0xA4`
* 마지막 `ESI`: `0x00000005`
* 마지막 destination: `0x023D6E61`
* 마지막 value: `0x00`

따라서 현재 timeout 지점은 파일 open 실패 자체가 아니라, DOS/환경 low-memory 문자열 스캔을 0으로 응답하는 HLE 정책을 지나면서 계속 진행되는 초기화 흐름으로 보는 것이 맞다.

## 검증

* `cmd /c scripts\build_win32_x86.bat`
  * 결과: 성공
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * 결과: low-memory access summary 출력 확인
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * 결과: 성공

# Low-Memory String Scan Observation Work Log

## Changes

Added low-memory access fields to `Win32MinimalExecutionAttempt` and `ThreadContext` so low-memory string/environment accesses are reported as a dedicated observation group.

The instrumentation was connected not only to the initially suspected `HandleDosMemoryAccess` path, but also to the actual paths used by `piu_1st`: `HandleSegmentMemoryCompareInstruction` and `HandleSegmentMemoryLoadInstruction`. The `80 3E 00` at `0x020F4DC1` and the following `AC`/`A4` operations are handled by the DS segment-memory helpers.

The loader now prints low-memory access count plus the last address/opcode/ESI/EDI/destination/value. `scripts/test_all.ps1` was updated to check for low-memory access observations.

## Result

The current stable `piu_1st` observation is:

* low-memory access count: `7`
* last low-memory access address: `0x020F4DD2`
* last opcode: `0xA4`
* last `ESI`: `0x00000005`
* last destination: `0x023D6E61`
* last value: `0x00`

So the current timeout point is best understood as initialization continuing through DOS/environment low-memory string scanning with zero-valued HLE responses, not as the missing file open itself.

## Verification

* `cmd /c scripts\build_win32_x86.bat`
  * Result: passed
* `build\win32_x86_debug\Debug\repiu_loader_win32.exe piu_1st`
  * Result: low-memory access summary printed
* `powershell -ExecutionPolicy Bypass -File scripts\test_all.ps1`
  * Result: passed
