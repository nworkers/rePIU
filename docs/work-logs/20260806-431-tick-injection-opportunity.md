# Task 431 작업 로그 — 귀속 계측 완성, 스모크는 게이트를 가리킵니다

설계: [20260806-431](../design/20260806-431-tick-injection-opportunity.md) ·
작업 지시: [20260806-431](../work-orders/20260806-431-tick-injection-opportunity.md) ·
선행 판정: [Task 430 §9](20260805-430-timer-tick-time-series.md)

## 1. 한 줄 결과

**H1 확정입니다.** 사용자 실행의 본곡 구간에서 버려진 틱의 **93.9%가 게스트 스레드가
Glide 게이트에 블록돼 있던 동안** 발생했습니다. 그 구간은 게스트 코드를 실행하지 않아
안전점 자체가 도달 불가이므로, 그 틱들은 **늦은 것이 아니라 전달 불가능**했습니다.
판정은 [§7](#7-판정--h1-확정)에 있습니다.

## 2. 만든 것

| 파일 | 내용 |
|---|---|
| `glide_opengl_backend.h/.cpp` | `guest_in_glide_gate_` 플래그 + 접근자. 게스트 스레드 경로에만, **RAII**(tail이 rethrow하므로) |
| `timer_tick_delivery.h/.cpp` | `due_in_gate_total`·`coalesced_in_gate_total`, `RecordTimerTicksDue`에 `in_gate` 인자 |
| `live_telemetry_snapshot.cpp` | due 시점에 게이트 플래그 전달, census 델타 3개 |
| `cd_audio_position_census.h/.cpp` | `safe_point_traps`·`ticks_coalesced`·`ticks_coalesced_in_gate` 열 |
| `main.cpp` | 종료 요약 `in-gate due/coalesced/coalesced-share` |
| `timer_tick_delivery_probe.cpp` | 부분집합 단정 추가(10번째) |

**동작 불변** — 게스트 경로 비용은 relaxed store 2회이고, 주입 정책·안전점 개수·
`REPIU_TIMER_TICK_BACKLOG` 기본값·rendezvous 스핀은 전부 그대로입니다.

## 3. 검증

| 항목 | 결과 |
|---|---|
| `repiu_aot_probe` | exit 0, timer tick 단정 **10개 전부 `true`** |
| 부분집합 불변식 `in_gate ≤ coalesced` | **404행 중 0행 위반** |
| census 합계 대 종료 요약 | 88.3% 대 88% (잔차는 마지막 표본 이후분) |

## 4. 검증 중에 결함 하나를 잡았습니다 — 분모가 틀렸습니다

첫 스모크에서 `in_gate > (due − injected)`인 행이 **404행 중 47행** 나왔습니다.
카운터 버그가 아니라 **제가 고른 분모가 틀렸습니다.**

구간별로 `due − injected`는 그 구간의 coalesced와 **같지 않습니다.** 이번 구간의 주입이
**직전 구간에 armed된 틱**을 소비할 수 있어 뺄셈이 분모를 과소평가하고, 비율이 100%를
넘습니다.

집계값(종료 요약)은 처음부터 `coalesced_total`을 분모로 썼으므로 영향이 없었지만,
**가이드가 독자에게 계산하라고 지시하는 것은 구간별 비율**이었습니다. 그래서
`ticks_coalesced`를 별도 열로 싣고 분모를 그것으로 바꿨으며, 설계 §4와 가이드에 함정을
명시했습니다. 수정 후 위반 행은 **0**입니다.

**교훈:** 파생 비율은 분자와 분모를 **같은 정의로 함께 실어야** 합니다. 한쪽만 싣고
나머지를 독자가 빼서 만들게 하면, 그 뺄셈이 성립하는지는 아무도 검산하지 않습니다.

## 5. 스모크가 가리키는 것 (판정 아님)

pumpit1 45초 두 번, 본곡 구간(`start_lba 20545`) 포함.

| 관측 | 1차 | 2차 |
|---|---:|---:|
| `coalesced_in_gate / coalesced` | **90%** | **88%** |
| `due_in_gate / due` | 56.5% | 63.1% |
| 전달률 `injected/due` | 61.6% | 53.4% |

**교차 검산 둘 다 성립합니다.** `due_in_gate/due` 56.5%는 Task 418이 잰 gate =
guest-run의 54~55%와 일치하고, 안전점 trap 5,474 대 injected 5,453은 "기회 = 안전점"
전제를 재확인합니다.

**그러나 이것은 판정이 아닙니다.** 제가 조작한 실행이고, Task 430에서 사용자 실행(51%)과
제 스모크(43%)가 서로 달랐습니다. 설계 §5 표는 **증상이 보인 실행**에 적용합니다.

## 7. 판정 — H1 확정

사용자 측정(2026-08-06, pumpit1, 33초, census 297표본).

```
timer tick delivery due/injected/coalesced/dropped/deferred: 7523/4024/3475/24/0
timer tick in-gate    due/coalesced/coalesced-share:          4872/3213/92%
AOT timer safe-point  trap/injected/deferred:                 4010/3988/22
```

항등식 성립(`7,523 = 4,024 + 3,475 + 24 + 0`).

**본곡 구간(gen 7, wall 10,016~32,782 ms, 22.77초, 표본 208개):**

| 지표 | 값 | 설계 §5 기준 |
|---|---|---|
| 음악 | **74.98 LBA/s** | 정확(오차 0.03%) |
| 전달률 `injected/due` | **50.6%** | (Task 430의 51%와 일치) |
| **`coalesced_in_gate / coalesced`** | **2,544 / 2,710 = 93.9%** | ≥ 80% → **H1 확정** |
| `tick_lag_ms` | 3,898 → **15,263** (**+11,365 ms**) | 게스트 시계 = 실시간의 **50.1%** |

**교차 검산 둘 다 성립합니다.**

| 검산 | 값 | 판정 |
|---|---|---|
| `safe_point_traps` ≈ `injected` | 2,765 대 2,781 = **0.99** | "기회 = 안전점" 전제 확인 |
| 본곡 `safe_point_traps`/초 | **121.4** (240 아님) | 모순 없음 — 기회가 실제로 부족 |

## 8. 사슬이 닫혔습니다

```mermaid
flowchart LR
    A["Task 421<br/>음악 위치 정확<br/>(오차 0)"] --> B["Task 430<br/>게스트 시계 = 실시간의 51%<br/>28.3초에 13.9초 뒤처짐"]
    B --> C["Task 431<br/>손실의 <b>93.9%</b>가<br/>Glide 게이트 블록 중"]
    C --> D["게이트 안에는 안전점이 없음<br/>= 늦은 것이 아니라 <b>전달 불가</b>"]
    style D fill:#c0392b,color:#fff
```

`InvokeOnHostThread`가 게스트 스레드를 `host_command_cv_.wait()`로 세우는 동안 게스트
코드는 한 줄도 실행되지 않습니다. 안전점은 캐시 안 INT3 967개이므로 그 창에서는 어느
것도 밟히지 않고, due는 계속 240 Hz로 쌓여 `bool` 하나에 합쳐집니다.

## 9. 다음 — 수정 축

**게이트 경계에서 밀린 틱을 배출**하는 것이 축입니다. 이미 `HandleGlideGateBoundary`
직후에 `InjectPendingInterrupts` 호출이 있지만(`execution_trampoline.cpp:3351`)
**pending이 `bool`이라 한 번에 한 개만 전달**합니다.

**Task 366의 전면 backlog와는 다릅니다.** 366은 안전점을 상시 armed로 두어 프레임을
16.4% 잃었는데, 이제 손실이 **게이트 창에 집중**돼 있음을 알므로 배출을 그 경계에만
붙일 수 있습니다. 상시 armed가 필요 없습니다.

## 6. 다음(측정 전 기록)

사용자 gameplay 측정 후 설계 §5로 H1을 확정/기각하고, frontier 항목 1‴a와 가이드를
결과로 갱신합니다.

---

# Task 431 Work Log — the attribution is built, and the smoke points at the gate

## 1. Result in one line

**H1 is confirmed.** In the user's main-track window, **93.9% of dropped ticks landed while the
guest thread was blocked in the Glide gate** — a window that runs no guest code and therefore
reaches no safe point, so those ticks were **undeliverable rather than late**. The verdict is in
section 7.

## 2. Built

A `guest_in_glide_gate_` flag on the Glide backend, set on the guest-thread path only and
scoped by RAII because the tail rethrows; `due_in_gate_total` and `coalesced_in_gate_total` with
an `in_gate` argument on `RecordTimerTicksDue`; the poll thread passing the flag at the moment
ticks come due; three new census columns; an exit-summary line; and a tenth probe assertion.
**Behaviour is unchanged** — two relaxed stores on the guest path, with injection policy,
safe-point count, the `REPIU_TIMER_TICK_BACKLOG` default and the rendezvous spin all untouched.

## 3. Verification

The probe passes with **all ten** timer-tick assertions true; the subset invariant
`in_gate ≤ coalesced` holds in **all 404 rows**; and the census share agrees with the exit
summary at 88.3% against 88%.

## 4. A defect caught during verification — the denominator was wrong

The first smoke had `in_gate > (due − injected)` in **47 of 404 rows**. Not a counter bug: **the
denominator I chose was wrong.** Per interval, `due − injected` is *not* that interval's
coalesced count, because an injection here can consume a tick armed in the **previous**
interval, so the subtraction understates it and the share exceeds 100%. The aggregate was
always right, since the exit summary divides by `coalesced_total` — but **what the guide told
readers to compute was the per-interval ratio**. `ticks_coalesced` is now carried as its own
column, the denominator is that, and both the design and the guide name the trap. After the fix,
zero rows violate.

**Lesson:** ship a derived ratio's numerator **and** denominator under the same definition. Ship
only one and leave the reader to subtract for the other, and nobody ever checks whether that
subtraction is valid.

## 5. What the smoke indicates (not a verdict)

Two 45-second pumpit1 runs covering the main track: `coalesced_in_gate / coalesced` at **90%**
and **88%**, `due_in_gate / due` at 56.5% and 63.1%, delivery at 61.6% and 53.4%. **Both
cross-checks hold** — 56.5% of owed ticks arriving in-gate matches Task 418's gate at 54-55% of
guest-run, and 5,474 safe-point traps against 5,453 safe-point injections re-confirms that the
opportunity is the safe point. **This is still not the verdict**: these are runs I drove, and in
Task 430 the user's run (51%) and my smoke (43%) disagreed. Design section 5's table applies to
a run where the symptom is visible.

## 7. Verdict — H1 confirmed

User measurement, 2026-08-06, pumpit1, 33 seconds, 297 samples:
`due/injected/coalesced/dropped/deferred: 7523/4024/3475/24/0` with the identity holding,
`in-gate due/coalesced/share: 4872/3213/92%`, and safe-point `trap/injected/deferred:
4010/3988/22`.

Over the main track (generation 7, wall 10,016-32,782 ms, 22.77 s, 208 samples) the music holds
**74.98 LBA/s** — exact to 0.03% — while delivery sits at **50.6%**, matching Task 430's 51%,
and `tick_lag_ms` climbs from 3,898 to **15,263, a growth of 11,365 ms**, putting the guest
clock at **50.1% of real time**. The deciding quantity, `coalesced_in_gate / coalesced`, is
**2,544 / 2,710 = 93.9%**, above the pre-registered 80%: **H1 confirmed.**

**Both cross-checks hold.** `safe_point_traps` against `injected` is 2,765 to 2,781, a ratio of
**0.99**, confirming the opportunity really is the safe point; and main-track traps run at
**121.4 per second** rather than near 240, so there is no contradiction — the opportunities
genuinely are scarce.

## 8. The chain is closed

The music position is exact (Task 421); the guest clock runs at 51% of real time (Task 430); and
**93.9% of that loss occurs while the guest is blocked in the Glide gate** (Task 431). While
`InvokeOnHostThread` parks the guest thread on `host_command_cv_.wait()`, not one line of guest
code executes, so none of the 967 in-cache INT3 safe points can be reached while ticks keep
coming due at 240 Hz and merging into a single boolean.

## 9. Next — the fix axis

The axis is **draining owed ticks at the gate boundary**. A call to `InjectPendingInterrupts`
already sits right after `HandleGlideGateBoundary` (`execution_trampoline.cpp:3351`), but
**pending is a `bool`, so it delivers exactly one**. This is **not** Task 366's blanket backlog,
which cost 16.4% of frames by holding safe points armed continuously: now that the loss is known
to be **concentrated in the gate window**, the drain can be attached to that boundary alone,
with no continuous arming.

## 6. Next (recorded before the measurement)

After the user's gameplay measurement, name H1 confirmed or rejected by design section 5 and
carry the result into frontier item 1‴a and the guide.
