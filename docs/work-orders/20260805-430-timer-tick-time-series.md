# Task 430 작업 지시 — 타이머 틱 시계열

설계: [20260805-430](../design/20260805-430-timer-tick-time-series.md) ·
측정 절차: [CD 오디오 위치 census 가이드](../guides/cd-audio-position-census.md)

## 1. 범위

Task 421 census 표본에 **타이머 틱 전달 두 칸**을 더해, 틱 손실이 gameplay 구간에서
일어나는지를 시간축에 귀속시킵니다. **동작 불변** — 이미 유지되는 카운터를 읽기만
합니다.

## 2. 변경할 파일

| 파일 | 내용 |
|---|---|
| `include/repiu/platform/win32/cd_audio_position_census.h` | 표본에 `timer_ticks_due`·`timer_ticks_injected` 추가 |
| `src/platform/win32/telemetry/cd_audio_position_census.cpp` | 열 두 개 + 파생 열 `tick_lag_ms` 덤프, 헤더 갱신 |
| `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | 표본 시점에 틱 카운터를 읽어 직전 표본 대비 델타로 기록 |
| `docs/guides/cd-audio-position-census.md` | 새 열과 판정 규칙, `delta_lba` 기대값 정정(8.21) |

**건드리지 않을 것:** 주입 정책, safe point, `REPIU_TIMER_TICK_BACKLOG` 기본값,
`cd_audio_wave_out.cpp`.

## 3. 구현 규칙

* 델타 계산은 `worker_iterations`와 **같은 방식**을 씁니다(직전 표본 값 보관 후 차분).
  읽는 사람이 열마다 다른 규칙을 외우지 않도록 합니다.
* `tick_lag_ms`는 덤프 시에만 계산하고 **틱 주파수를 상수로 박지 않습니다**
  (설계 §5). `Σdue = 0`이면 0을 씁니다.
* 카운터는 free-running이며 census가 초기화하지 않습니다.

## 4. 검증

1. `repiu_aot_probe`의 timer tick delivery probe가 계속 통과할 것(카운터 의미 불변).
2. 스모크 실행에서 census 파일에 새 열 셋이 나오고, 초반 표본의 `timer_ticks_due`가
   표본 간격 × 틱 주파수와 같은 크기일 것(100 ms · 240 Hz면 약 24).
3. `entries/regressions` 요약 줄이 그대로 나올 것.

## 5. 사용자 측정 (구현 후 요청)

가이드대로 **증상이 보이는 gameplay까지** 플레이한 뒤 다음을 전달해 주십시오.

1. `build/cd_audio_position_census.txt`
2. 실행 로그(stderr) — 종료 요약의 누적 틱 줄과 대조하기 위함

```
set REPIU_CD_AUDIO_POSITION_CENSUS=1
set REPIU_EXECUTION_TIMEOUT_MS=0
```

## 6. 완료 기준

1. 새 열이 gameplay 구간의 `injected/due`와 `tick_lag_ms`를 보여 줍니다.
2. 설계 §6의 표를 그대로 적용해 후보 1‴을 **확정 또는 기각**합니다.
3. 작업 로그를 쓰고 frontier 항목 1‴과 가이드를 결과로 갱신합니다.

---

# Task 430 Work Order — the timer tick time series

## 1. Scope

Add **two timer-tick fields** to Task 421's census sample so tick loss can be attributed to
the timeline rather than averaged over a whole run. **Behaviour is unchanged** — counters that
are already maintained are only read.

## 2. Files

`cd_audio_position_census.h` gains `timer_ticks_due` and `timer_ticks_injected`;
`cd_audio_position_census.cpp` dumps them plus the derived `tick_lag_ms` and updates the
header; `live_telemetry_snapshot.cpp` reads the counters at sample time and stores deltas
against the previous sample; and the census guide gains the new columns, the new readings, and
the corrected `delta_lba` expectation of 8.21. **Not touched:** injection policy, safe points,
the `REPIU_TIMER_TICK_BACKLOG` default, and `cd_audio_wave_out.cpp`.

## 3. Implementation rules

Compute deltas **the same way** `worker_iterations` already does, so a reader learns one rule
rather than one per column. Derive `tick_lag_ms` only at dump time and **never hard-code the
tick frequency** (design section 5), writing zero when `Σdue` is zero. The counters are
free-running and the census must not reset them.

## 4. Verification

The `repiu_aot_probe` timer tick delivery probe must still pass, since counter semantics do
not change; a smoke run must show the three new columns with early `timer_ticks_due` near the
sample interval times the tick rate (about 24 for 100 ms at 240 Hz); and the
`entries/regressions` summary line must still appear.

## 5. The user's measurement, once this is built

Play to where the symptom shows and send back `build/cd_audio_position_census.txt` with the
run's stderr, so the series can be checked against the exit summary's cumulative tick line.

## 6. Done when

The new columns show gameplay `injected/due` and `tick_lag_ms`; design section 6's table names
candidate 1‴ **confirmed or rejected**; and the work log, frontier item 1‴ and the guide carry
the result.
