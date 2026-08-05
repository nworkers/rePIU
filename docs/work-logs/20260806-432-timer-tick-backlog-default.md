# Task 432 작업 로그 — 수정은 이미 코드에 있었고, 기본값만 틀렸습니다

설계: [20260806-432](../design/20260806-432-timer-tick-backlog-default.md) ·
작업 지시: [20260806-432](../work-orders/20260806-432-timer-tick-backlog-default.md) ·
사슬: [421](20260805-421-423-cd-audio-and-stall-root-cause.md) →
[430](20260805-430-timer-tick-time-series.md) →
[431](20260806-431-tick-injection-opportunity.md) → 432

## 1. 한 줄 결과

**노트·BGA 점프 축이 닫혔습니다.** 사용자가 `REPIU_TIMER_TICK_BACKLOG=1`로 gameplay를
하고 **증상 해소를 확인**했으며, 기본값을 켬으로 바꿨습니다. **새 기구는 만들지
않았습니다** — Task 366이 만들어 두고 opt-in으로 남긴 backlog가 그대로 답이었습니다.

## 2. 사슬

| Task | 확인 | 방법 |
|---|---|---|
| 421~423 | 음악 위치는 정확(오차 0) | 위치 census |
| 430 | 게스트 시계 = 실시간의 **51%** | 틱 시계열(`tick_lag_ms`) |
| 431 | 손실의 **93.9%** 가 Glide 게이트 블록 중 | 손실 시점의 게스트 위치 귀속 |
| **432** | **backlog로 개수 보존 → 99.98%, 증상 소멸** | 기본값 전환 |

게이트 안에서는 게스트 코드가 실행되지 않아 안전점(캐시 내 INT3 967개)이 도달 불가이고,
그동안 due는 240 Hz로 쌓여 `bool` 하나에 합쳐졌습니다. backlog는 그 `bool`을 개수로
바꿉니다.

## 3. 근거 — gameplay 실측 (사용자 실행, 본곡 64.53초·표본 591개)

| 지표 | OFF | ON |
|---|---:|---:|
| 전달률 `injected/due` | 50.6% | **99.98%** |
| `coalesced` / `coalesced_in_gate` | 2,710 / 2,544 | **0 / 0** |
| `tick_lag_ms` 증가 | **+11,365 ms** | **−11 ms** |
| 음악 | 74.98 LBA/s | 75.00 LBA/s |
| 안전점 trap | 121.4/초 | **240.2/초** |
| `max_backlog` | 1 | **12**(상한 64) |

`traps/초 = 240.2`가 핵심입니다 — **틱당 정확히 1회**이고 과잉 trap이 없습니다.

## 4. Task 366의 대가 판정이 왜 낡았는가

366은 프레임 **−16.4%** 를 재고 opt-in으로 남겼습니다. 그 비용의 기전을 366이 **스스로
특정**해 두었습니다.

> 비싼 것은 주입이 아니라 **safe point가 상시 armed 상태로 유지되는 것**입니다.

그 상시 armed는 **backlog가 상한에 고착될 때만** 일어납니다(366 판정 T3).

| | 366 당시 | 현재 |
|---|---|---|
| `max_backlog` | **64 (상한 고착)** | **12** |
| 상한 초과 폐기 | 1,029~1,076 | 27~64 |
| safe-point trap | **+20.1%** | **틱당 1회**(과잉 없음) |

Tasks 414(tick당 포트 읽기 200→2)·415·417(세대 실패 0)·419(프레임 +27.7%)가 그 사이
실행 속도를 올려 backlog가 게이트 사이에 비워집니다. **366의 결론은 그 시점 기록으로
유지하고, 두 문서 머리말에 무효 범위를 적었습니다.**

## 5. 검증

| 항목 | 결과 |
|---|---|
| `repiu_aot_probe` | exit 0, `policy=true`, 단정 10개 전부 `true` |
| 기본값 pumpit1 | `enabled=true`, `coalesced=0`, 전달률 **99.67%** |
| opt-out `=0` | `enabled=false`, `coalesced=3,256` — **이전 동작 복원** |
| **pumpit3 회귀** | `enabled=true`, `coalesced=0`, 전달률 **99.26%** — 회귀 없이 같은 이득 |
| 프레임(pumpit1) | 기본 2,042 대 opt-out 2,076 = **−1.6%** |

프레임 −1.6%는 앞선 짝 A/B의 −1.2%와 일관되며 **실행 간 편차와 구분되지 않습니다.**
근거로 주장하는 것은 "비용 0"이 아니라 **"−16.4%가 아니다"** 까지입니다.

## 6. 미측정으로 남긴 것

* **본곡 구간의 짝 프레임 측정.** 위 A/B는 전부 attract 구간입니다. 게이트 점유가 가장
  높은 구간의 대가는 별도로 재야 합니다.
* pumpit3 gameplay 도달 여부. 스모크는 attract까지입니다.

## 7. 회고

* **고칠 것이 이미 코드에 있었습니다.** 저는 게이트 경계 주입을 새로 만들 준비를 했고,
  직접 디스패치 thunk가 게스트 ESP를 프레임으로 돌려주지 않아 **가장 뜨거운 경로를
  뜯어야 한다**는 것까지 확인한 뒤에야 backlog A/B를 돌렸습니다. **환경변수 하나로 되는
  것을 먼저 시도했어야 합니다.**
* **제가 "backlog로는 안 된다"고 단정했던 것이 틀렸습니다.** 기회 120회/초를 독립적
  상한으로 읽었는데, 그 120회는 `bool`의 **결과**였습니다. 원인과 결과를 뒤집어 읽으면
  해법을 스스로 지웁니다.
* **옛 측정의 유효 범위를 의심한 것이 값을 했습니다.** 366의 −16.4%를 그대로 받아들였다면
  이 축은 닫히지 않았습니다. 그 로그가 **비용의 기전까지 적어 둔 덕분에** 그 기전이
  지금 성립하지 않음을 확인할 수 있었습니다 — 결론만 적었다면 불가능했습니다.
* **366이 스위치를 남긴 것이 결정적이었습니다.** 그래서 이번에도 `=0` 경로를 남겼습니다.

---

# Task 432 Work Log — the fix was already in the code; only the default was wrong

## 1. Result in one line

**The note and BGA jumping axis is closed.** The user played with
`REPIU_TIMER_TICK_BACKLOG=1` and **confirmed the symptom is gone**, and the default is now on.
**No new mechanism was built** — Task 366's backlog, left in as an opt-in, was the answer.

## 2. The chain

The music position was exact (421); the guest clock ran at **51%** of real time (430); **93.9%**
of that loss occurred while the guest was blocked in the Glide gate (431); and preserving the
count fixes it (432). Inside the gate no guest code runs, so none of the 967 in-cache INT3 safe
points is reachable while ticks keep coming due at 240 Hz and collapsing into one boolean. The
backlog turns that boolean into a count.

## 3. Evidence — the user's gameplay window, 64.53 s over 591 samples

Delivery goes from 50.6% to **99.98%**, `coalesced` and `coalesced_in_gate` from 2,710 and 2,544
to **zero**, and `tick_lag_ms` from **+11,365 ms** of growth to **−11 ms** — the guest clock
locked to real time. Safe-point traps go from 121.4/s to **240.2/s, exactly one per owed tick**,
with `max_backlog` at **12** against a cap of 64.

## 4. Why Task 366's cost verdict is stale

Task 366 measured **−16.4% frames** and kept the switch opt-in. It also **named the mechanism
itself**: *what is expensive is not the injection but the safe point being held armed
continuously* — which happens **only when the backlog pins at the cap** (its reading T3). Then,
`max_backlog` sat at **64** with 1,029-1,076 dropped past it and traps up **20.1%**; now
`max_backlog` is **12**, drops are 27-64, and traps are exactly one per tick. Tasks 414, 415,
417 and 419 raised execution speed in between, so the backlog empties between gate calls.
**366's conclusion stays as the record of its moment, with a scope note at the head of both its
documents.**

## 5. Verification

The probe passes with `policy=true` and all ten assertions; a default pumpit1 smoke reports
`enabled=true` with `coalesced=0` and 99.67% delivery; the `=0` opt-out restores the old
behaviour at `coalesced=3,256`; and **pumpit3 shows no regression**, reaching `coalesced=0` and
99.26% delivery. Frames read 2,042 against the opt-out's 2,076, **−1.6%**, consistent with the
earlier −1.2% and **indistinguishable from run-to-run variation**: the supported claim is **not
"zero cost" but "not −16.4%"**.

## 6. Left unmeasured

A **paired frame measurement over the main track** — every A/B here was in the attract phase,
and the gate-heavy gameplay window is where the cost would show — and whether pumpit3 reaches
gameplay, since its smoke only covers attract.

## 7. Retrospective

**The fix was already in the tree.** I had worked out how to inject at the gate boundary and had
established that the direct-dispatch thunk never returns guest ESP through its frame — meaning
the hottest path in the emulator would have to be reworked — before running the backlog A/B.
**The thing that costs one environment variable should have been tried first.**

**My claim that "the backlog cannot fix this" was wrong.** I read the 120 opportunities per
second as an independent ceiling when it was a *consequence* of the boolean. Inverting cause and
effect deletes the solution.

**Doubting an old measurement's scope paid.** Taking 366's −16.4% at face value would have kept
this axis shut. It was recoverable only because that log recorded **the mechanism behind the
cost** and not just the verdict — had it written down the conclusion alone, there would have
been nothing to re-test. And it was 366 leaving the switch in place that made all of this
possible, which is why the `=0` path stays.
