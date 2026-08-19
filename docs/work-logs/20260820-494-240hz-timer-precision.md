# 20260820-494 240Hz 타이머 인터럽트 정밀도 개선 작업 로그 / Work log

## 한국어

### 결과

원본 PIT divisor `4972`의 약 240Hz tick schedule에 next-deadline 조회를 추가하고,
Win32 telemetry poll loop가 다음 tick 1ms 이내에서는 zero-timeout command pump와
`Sleep(0)`을 사용하도록 변경했습니다. pending tick도 동일한 경로를 사용하므로
safe-point 전달이 기존 고정 1ms 대기로 지연되지 않습니다. 원본 guest INT 8 ISR,
IRETD, pending/backlog semantics는 변경하지 않았습니다.

### 변경 파일

- `include/repiu/hle/pit_timer.h`, `src/hle/pit_timer.cpp`
  - `PitIrqSchedule::NanosecondsUntilNextTick` 추가
  - epoch와 tick offset 합산 시 overflow 포화 처리
- `src/platform/win32/telemetry/live_telemetry_snapshot.cpp`
  - deadline 임박 시 wait/spin 정책 추가
- `src/tools/aot_probe/pit_timer_probe.cpp`
  - next-deadline 회귀 검증 추가
- 설계 문서, 작업 지시서 및 본 로그 추가

### 검증

- `git diff --check`: 통과
- Win32 x86 Debug `repiu_aot_probe` 직렬 빌드: 통과
- `repiu_aot_probe.exe --jamma-input-timeline`: 통과
  - `pit_timer_probe=true, divisor=4972, frequency_hz=240`
  - timer tick delivery policy/coalescing/backlog 전체 true
  - JAMMA input timeline probe true

첫 병렬 빌드는 앞선 제한시간 종료 후 남은 MSBuild 프로세스가 같은 PDB를 사용하여
MSVC `C1041`로 실패했습니다. 해당 빌드 프로세스를 종료하고 `/m:1` 직렬 빌드로
재실행하여 코드 오류가 아님을 확인했습니다. 전체 runtime 장시간 지터 수치는 실제
게임 실행 측정이 필요하며, 이번 작업에서는 합성 probe와 빌드 검증까지 수행했습니다.

## English

### Result

Added `PitIrqSchedule::NanosecondsUntilNextTick` for the original PIT divisor `4972`
and changed the Win32 telemetry poll loop to use a zero-timeout command pump and
`Sleep(0)` within 1ms of the next edge. Pending ticks use the same path, removing the
fixed 1ms wait from safe-point delivery. The original guest INT 8 ISR, IRETD, and
pending/backlog semantics remain unchanged.

### Verification

- `git diff --check`: passed.
- Win32 x86 Debug `repiu_aot_probe` serial build: passed.
- `repiu_aot_probe.exe --jamma-input-timeline`: passed, including the PIT timer,
  timer delivery, and JAMMA timeline probes.

The first parallel build failed with MSVC `C1041` because timed-out build processes
continued writing the same PDB. After terminating those build processes, the serial
`/m:1` build passed. A long-running real-game jitter measurement remains for runtime
validation; this task verified the synthetic probes and build path.
