# 20260803-407 Arena 실행 진입 추적 작업 지시 / Work Order

설계: [20260803-407](../design/20260803-407-arena-execution-entry-trace.md)

## 한국어

### 목적

port I/O 지연 루프가 **어떻게 처음 arena 자유 실행 상태에 들어가는지** 기록한다.
동작은 바꾸지 않는다.

### 범위

| 파일 | 변경 |
|---|---|
| `src/platform/win32/execution/thread_context.h` | 직전 VEH 상태 3개, 진입 trace |
| `src/platform/win32/execution/execution_trampoline.cpp` | 관문에서 직전 상태 갱신 |
| `src/platform/win32/io/port_io_emulator.cpp` | 진입 조건일 때 trace 기록 |
| `include/repiu/platform/win32/execution_trampoline.h` | 스냅샷 필드 |
| `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | 복사 |
| `src/host/win32/main.cpp` | 요약 로그 2종 |

### 단계

1. `ThreadContext`에 `last_veh_code/eip/in_cache`, `prev_veh_code/eip/in_cache`,
   `kArenaPortIoEntryTraceCapacity = 16`, `ArenaPortIoEntryTraceEntry`, 배열, count,
   overflow를 추가한다.
2. `execution_trampoline.cpp`의 `RecordVehExceptionCensus` 호출 **직후**에 `prev_*`를
   `last_*`로부터 밀고 `last_*`를 이번 예외로 갱신한다. `win32_context`가 이미 유효한
   지점이어야 한다.
3. `HandlePortIoInstruction`에서 `!from_aot_cache && context->prev_veh_code != 0xC0000096`
   일 때 한 항목을 기록한다. TF는 `win32_context->EFlags & 0x100`.
4. 스냅샷과 로그를 갱신한다.
   * `Win32 arena port I/O entry trace entries/overflow: {}/{}`
   * `Win32 arena port I/O entry #{} guest/prev-code/prev-eip/prev-in-cache/tf/reentry/legacy/step: ...`
5. Debug와 Release로 `repiu_loader_win32`, `repiu_aot_probe`를 빌드한다.

### 검증

* `repiu_aot_probe` 종료 코드 0.
* pumpit3 45초 3회에서 trace를 읽고 설계 §판정 기준으로 결론을 낸다.
  **항목이 0이면 정의가 틀린 것이므로 그대로 기록한다.**
* 같은 실행에서 Task 405/406 census 구조가 유지되는지 확인(동작 불변).
* pumpit1 45초 1회 회귀.

### 완료 조건

진입 시점의 직전 예외 종류를 실측으로 확보하고 다음 축을 지목한다.

---

## English

### Purpose

Record **how the port I/O delay loop first enters free-running arena execution**. No behaviour
change.

### Steps

1. Add `last_veh_{code,eip,in_cache}`, `prev_veh_{code,eip,in_cache}`, a sixteen-entry
   `ArenaPortIoEntryTraceEntry` array, and count and overflow counters to `ThreadContext`.
2. Immediately after the `RecordVehExceptionCensus` call, shift `prev_*` from `last_*` and set
   `last_*` from this exception, at a point where `win32_context` is already valid.
3. In `HandlePortIoInstruction`, record one entry when
   `!from_aot_cache && context->prev_veh_code != 0xC0000096`, taking the trap flag from
   `win32_context->EFlags & 0x100`.
4. Mirror into the snapshot and print
   `Win32 arena port I/O entry trace entries/overflow` plus a per-entry
   `guest/prev-code/prev-eip/prev-in-cache/tf/reentry/legacy/step` line.
5. Build `repiu_loader_win32` and `repiu_aot_probe` in Debug and Release.

### Verification

The probe exits zero; three 45-second pumpit3 runs yield the trace and the design's decision
rule names the cause — **if the trace records nothing, the definition is wrong and that is
recorded as such**; the Task 405 and 406 census structure is unchanged in the same runs; and
one pumpit1 run checks for regression.

### Done when

The previous exception kind at entry is measured and the next axis is named.
