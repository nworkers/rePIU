# Task 432 설계 — timer tick backlog를 기본값으로

선행: [431 게이트 귀속](20260806-431-tick-injection-opportunity.md) ·
[430 틱 시계열](20260805-430-timer-tick-time-series.md) ·
**정정 대상:** [366 timer tick 전달](20260730-366-timer-tick-delivery-and-frame-pacing.md)

## 1. 사슬이 닫혔고, 수정은 이미 코드에 있습니다

```mermaid
flowchart LR
    A["421 음악 위치 정확"] --> B["430 게스트 시계 = 실시간의 51%"]
    B --> C["431 손실의 93.9%가<br/>Glide 게이트 블록 중"]
    C --> D["게이트 안엔 안전점 없음<br/>= bool 하나에 합쳐져 소멸"]
    D --> E["<b>432 backlog로 개수를 보존</b>"]
    style E fill:#1e8449,color:#fff
```

Task 366이 만든 bounded backlog(`REPIU_TIMER_TICK_BACKLOG`)가 정확히 그 `bool`을
개수로 바꿉니다. **새로 만들 것이 없고, 기본값만 뒤집으면 됩니다.**

## 2. 근거 — 사용자 확인 + gameplay 실측

**사용자가 `REPIU_TIMER_TICK_BACKLOG=1`로 gameplay를 했고 점프 증상이 사라졌습니다**
(2026-08-06). 같은 실행의 본곡 구간 64.53초·표본 591개입니다.

| 지표 | backlog OFF | backlog ON | 비고 |
|---|---:|---:|---|
| 전달률 `injected/due` | 50.6% | **99.98%** | — |
| `coalesced` | 2,710 | **0** | — |
| `coalesced_in_gate` | 2,544 | **0** | 431이 지목한 손실이 사라짐 |
| `tick_lag_ms` 증가 | **+11,365 ms** | **−11 ms** | 게스트 시계가 실시간에 고정 |
| 음악 | 74.98 LBA/s | 75.00 LBA/s | 원래 정확했음 |
| 안전점 trap | 121.4/초 | **240.2/초** | **틱당 정확히 1회** |
| `max_backlog` | 1 | **12** | 상한 64에 못 미침 |

## 3. Task 366의 대가 판정은 낡았습니다 — 그 기전이 재현되지 않습니다

366은 같은 스위치로 **프레임 −16.4%** 를 재고 "속도를 기대하고 켜지 말 것"이라고
적었습니다. 그 비용의 정체를 366 스스로 이렇게 특정해 두었습니다.

> 비싼 것은 주입이 아니라 **safe point가 상시 armed 상태로 유지되는 것**입니다.

**그 상시 armed가 지금은 일어나지 않습니다.** 근거 둘:

| 366 당시 | 현재 | 뜻 |
|---|---|---|
| `max_backlog` = **64(상한 고착)**, 초과 폐기 1,029~1,076 | `max_backlog` = **12**, `dropped` 28 | backlog가 게이트 사이에 **비워짐** |
| safe-point trap **+20.1%** | trap = **틱당 정확히 1회**(240.2/초) | 과잉 trap **없음** |

즉 366의 판정 **T3(호스트가 240 Hz를 따라잡지 못함)** 이 성립하지 않습니다. 그 사이
Tasks 414(tick당 포트 읽기 200→2)·415·417(세대 실패 0)·419(프레임 +27.7%)가 실행
속도를 올렸기 때문입니다. **366의 결론은 그 시점의 기록으로 유지하되, 현재 빌드에는
적용되지 않음을 명시합니다.**

**짝 A/B(각 45초, attract 구간):** 프레임 2,243 → 2,216(**−1.2%**). 실행 간 편차와
구분되지 않는 크기이며, **"비용이 0"이 아니라 "−16.4%가 아니다"** 까지가 근거입니다.

## 4. 변경

`TimerTickBacklogEnabled()`의 기본값을 **켬**으로 바꾸고, `REPIU_TIMER_TICK_BACKLOG=0`
으로 끌 수 있게 합니다. 끄는 경로는 **회귀 대조군으로 반드시 남깁니다** — 366이 남겨
둔 opt-in이 이번 판정을 가능하게 했던 것과 같은 이유입니다.

| 설정 | 이전 | 이후 |
|---|---|---|
| 미설정 | 꺼짐 | **켜짐** |
| `1`·`on`·`true` | 켜짐 | 켜짐 |
| `0`·`off`·`false` | 꺼짐 | 꺼짐 |
| 그 외 문자열 | 꺼짐 | **켜짐**(기본값으로 되돌림) |

**정확성 우선 원칙에 부합합니다**(AGENTS.md). 실제 하드웨어에서 PIT는 게스트가
인터럽트를 받지 못하는 동안에도 계속 세고, 받을 수 있게 되면 밀린 것이 전달됩니다.
개수를 보존하는 쪽이 원래 동작이고, `bool` 한 개는 그 근사였습니다.

## 5. 위험과 한계

| 항목 | 판단 |
|---|---|
| gameplay 프레임 대가 **미측정** | 짝 A/B는 attract 구간이었습니다. 다만 trap이 틱당 1회로 최소이고 사용자가 gameplay에서 증상 해소를 확인했으므로 진행합니다. **본곡 구간 짝 측정은 후속으로 남깁니다** |
| `dropped` 28~31 잔존 | 상한 초과가 아니라 벡터 미설치 구간(부팅기)의 정리분입니다. `max_backlog` 12가 상한과 멀어 상한 조정은 불필요 |
| pumpit1 외 타이틀 | pumpit3는 미측정. 회귀 확인 대상 |

## 6. 검증

1. probe 통과 — 기존 단정 10개 불변(정책 단정은 **기본값 변화에 맞춰 갱신**).
2. 미설정 스모크에서 `backlog-enabled=true`, `coalesced=0`.
3. `REPIU_TIMER_TICK_BACKLOG=0` 스모크에서 이전 동작(`coalesced` > 0) 복원.
4. pumpit3 스모크로 회귀 없음 확인.

---

# Task 432 Design — make the timer tick backlog the default

## 1. The chain is closed and the fix already exists

The music position is exact (421), the guest clock ran at 51% of real time (430), and 93.9% of
that loss occurred while the guest was blocked in the Glide gate where no safe point is
reachable (431), so owed ticks collapsed into a single boolean. Task 366's bounded backlog
turns exactly that boolean into a count. **Nothing new is needed; only the default changes.**

## 2. Evidence — user confirmation plus a gameplay measurement

**The user played with `REPIU_TIMER_TICK_BACKLOG=1` and the jumping is gone** (2026-08-06).
Over that run's main track — 64.53 seconds, 591 samples — delivery goes from 50.6% to
**99.98%**, `coalesced` and `coalesced_in_gate` from 2,710 and 2,544 to **zero**, and
`tick_lag_ms` from **+11,365 ms** of growth to **−11 ms**, which is the guest clock locked to
real time. The music was always right, at 74.98 then 75.00 LBA/s. Safe-point traps go from
121.4/s to **240.2/s — exactly one per owed tick** — and `max_backlog` reaches **12** against a
cap of 64.

## 3. Task 366's cost verdict is stale, because its mechanism no longer occurs

Task 366 measured **−16.4% frames** on this switch and wrote "do not enable it expecting a
speedup". It also identified the cost precisely: *what is expensive is not the injection but
**the safe point being held armed continuously***. **That continuous arming no longer happens.**
Then, `max_backlog` pinned at its cap of **64** with 1,029-1,076 ticks dropped past it and
safe-point traps up **20.1%**; now `max_backlog` is **12** with 28 dropped and traps at
**exactly one per tick**. Task 366's **T3** reading — that the host cannot keep up with 240 Hz —
does not hold on this build, because Tasks 414 (port reads per tick from 200 to two), 415, 417
(zero generation failures) and 419 (+27.7% frames) raised execution speed in between. **366's
conclusion stays as the record of its moment, marked as not applying to the current build.**

A paired 45-second A/B in the attract phase reads **2,243 → 2,216 frames (−1.2%)**, a
difference indistinguishable from run-to-run variation: the claim supported is **not "zero
cost" but "not −16.4%"**.

## 4. The change

`TimerTickBacklogEnabled()` defaults to **on**, with `REPIU_TIMER_TICK_BACKLOG=0` turning it
off. **The off path is kept deliberately as the regression control** — it is precisely because
Task 366 left this as an opt-in that the present verdict was possible. Unset and unrecognised
values now resolve to on; `0`, `off` and `false` resolve to off.

This follows the **accuracy-over-optimisation** rule in AGENTS.md: on real hardware the PIT
keeps counting while the guest cannot take interrupts, and the owed ticks arrive once it can.
Preserving the count is the original behaviour, and the single boolean was an approximation of
it.

## 5. Risks and limits

The **gameplay frame cost is unmeasured** — the paired A/B was in the attract phase — but traps
are already minimal at one per tick and the user confirmed the symptom gone in gameplay, so this
proceeds with **a paired main-track measurement left as follow-up**. The residual 28-31
`dropped` are boot-phase cleanup with no vector installed rather than cap overflow, and
`max_backlog` of 12 is far enough from 64 that the cap needs no adjustment. pumpit3 is
unmeasured and is the regression target.

## 6. Verification

The probe must pass with its assertions updated for the new default; an unset smoke must report
`backlog-enabled=true` with `coalesced=0`; a `REPIU_TIMER_TICK_BACKLOG=0` smoke must restore the
old behaviour with `coalesced` above zero; and a pumpit3 smoke must show no regression.
