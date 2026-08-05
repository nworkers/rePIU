# Task 431 작업 지시 — 틱 주입 기회 귀속

설계: [20260806-431](../design/20260806-431-tick-injection-opportunity.md) ·
측정 절차: [CD 오디오 위치 census 가이드](../guides/cd-audio-position-census.md)

## 1. 범위

버려지는 틱이 **Glide 게이트 블록 탓인지**를 귀속으로 가릅니다. **동작 불변** —
플래그 하나와 카운터 둘을 더할 뿐 주입 정책·안전점·backlog는 그대로입니다.

## 2. 변경할 파일

| 파일 | 내용 |
|---|---|
| `glide_opengl_backend.h/.cpp` | `guest_in_glide_gate` 플래그와 읽기 접근자. **게스트 스레드 경로에서만** 진입/이탈 시 설정 |
| `timer_tick_delivery.h/.cpp` | `due_in_gate`·`coalesced_in_gate` 카운터, `RecordTimerTicksDue`에 `in_gate` 인자 |
| `live_telemetry_snapshot.cpp` | tick due 시점에 게이트 플래그를 읽어 전달, census에 `safe_point_traps`·`coalesced_in_gate` 델타 |
| `cd_audio_position_census.h/.cpp` | 열 둘 추가 |
| `main.cpp` | 종료 요약에 `in-gate` 비율 |
| `timer_tick_delivery_probe.cpp` | 새 인자·카운터에 대한 단정 |

**건드리지 않을 것:** 주입 정책, 안전점 개수, `REPIU_TIMER_TICK_BACKLOG` 기본값,
rendezvous 스핀.

## 3. 구현 규칙

* 플래그는 **게스트 스레드 경로에만** 겁니다. `IsHostThread()` 분기는 제외합니다.
* 플래그는 `relaxed` store로 충분합니다. 정확한 경계가 아니라 **비율**이 목적이고,
  한 표본이 어긋나도 결론이 바뀌지 않습니다.
* census 델타는 `worker_iterations`와 **같은 방식**을 씁니다.
* 카운터는 free-running이며 census가 초기화하지 않습니다.

## 4. 검증

1. `repiu_aot_probe` 통과 — 기존 timer tick 단정 9개가 그대로 참일 것.
2. 스모크에서 새 열이 채워지고, `coalesced_in_gate ≤ coalesced`가 항상 성립할 것.
3. 종료 요약의 `in-gate` 비율이 census 델타 합계와 일치할 것.

## 5. 사용자 측정

**증상이 보이는 gameplay까지** 플레이한 뒤 `build/cd_audio_position_census.txt`와
stderr 로그를 전달해 주십시오. **cmd 리다이렉션**을 쓰십시오(PowerShell은 UTF-16으로
저장되어 도구가 읽지 못합니다).

```
set REPIU_CD_AUDIO_POSITION_CENSUS=1
set REPIU_EXECUTION_TIMEOUT_MS=0
build\win32_x86_debug\Release\repiu_loader_win32.exe pumpit1 > repiu_log.txt 2>&1
```

## 6. 완료 기준

1. 본곡 구간의 `coalesced_in_gate / coalesced` 비율이 나옵니다.
2. 설계 §5 표로 H1을 **확정 또는 기각**하고, 검산 둘을 함께 적습니다.
3. 작업 로그를 쓰고 frontier 항목 1‴a와 가이드를 갱신합니다.

---

# Task 431 Work Order — attributing the lost tick opportunities

## 1. Scope

Decide by attribution whether dropped ticks are the **Glide gate block**. **Behaviour is
unchanged** — one flag and two counters, with injection policy, safe points and the backlog all
left alone.

## 2. Files

`glide_opengl_backend` gains a `guest_in_glide_gate` flag with a reader, set and cleared **only
on the guest-thread path**; `timer_tick_delivery` gains `due_in_gate` and `coalesced_in_gate`
with an `in_gate` argument on `RecordTimerTicksDue`; `live_telemetry_snapshot` reads the flag
when ticks come due and adds the `safe_point_traps` and `coalesced_in_gate` census deltas; the
census header and dump gain two columns; `main.cpp` reports the in-gate share; and the probe
asserts the new argument and counters. **Not touched:** injection policy, safe-point count, the
`REPIU_TIMER_TICK_BACKLOG` default, and the rendezvous spin.

## 3. Implementation rules

The flag is set **only on the guest-thread path**, excluding the `IsHostThread()` branch, and
`relaxed` stores suffice because the goal is a **ratio** rather than an exact boundary — one
misattributed sample cannot change the verdict. Census deltas follow **the same rule** as
`worker_iterations`, and the counters are free-running.

## 4. Verification

The probe suite must still pass with its nine existing timer-tick assertions; a smoke run must
fill the new columns with `coalesced_in_gate` never exceeding `coalesced`; and the exit
summary's in-gate share must agree with the sum of the census deltas.

## 5. The user's measurement

Play to where the symptom shows and return `build/cd_audio_position_census.txt` with the stderr
log, using **cmd redirection** — PowerShell writes UTF-16, which the tooling cannot read.

## 6. Done when

The main-track `coalesced_in_gate / coalesced` ratio is measured, design section 5's table names
H1 confirmed or rejected **with both cross-checks recorded**, and the work log, frontier item
1‴a and the guide carry the result.
