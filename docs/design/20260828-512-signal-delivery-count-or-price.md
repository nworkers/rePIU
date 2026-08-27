# Task 512 — 시그널 전달: 횟수인가 단가인가

작업 지시: [20260828-512](../work-orders/20260828-512-signal-delivery-count-or-price.md) ·
작업 로그: [20260828-512](../work-logs/20260828-512-signal-delivery-count-or-price.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260828-511](20260828-511-live-execution-profile-report.md)

## 배경

Task 511이 축을 확정했습니다 — **폴트 핸들러가 프레임당 격차의 68.3%**, Windows 2.065M
cycle 대 Linux 88.072M cycle로 42.6배입니다. Linux에서 그 핸들러는 시그널 전달입니다.

42.6배는 곱셈 두 개 중 하나입니다.

$$\frac{\text{cycles}}{\text{frame}} = \frac{\text{deliveries}}{\text{frame}} \times \frac{\text{cycles}}{\text{delivery}}$$

**어느 쪽이 42.6배인지에 따라 고치는 방법이 완전히 다릅니다.**

| 만약 | 뜻 | 고치는 방향 |
|---|---|---|
| **횟수**가 크다 | Linux가 더 자주 폴트한다 | 폴트를 만드는 자리를 줄이는 일 — 경계 자체를 없애거나 묶는 것 |
| **단가**가 크다 | 전달 경로가 비싸다 | 경계를 시그널이 아닌 것으로 바꾸는 일 |

둘을 가르기 전에 어느 쪽으로도 손대면 안 됩니다.

## 확인됨 — Windows 쪽 값은 이미 있습니다

새로 잴 것이 절반뿐입니다. Windows는 종료 요약에 닿으므로 이미 전부 찍고 있습니다
(511의 대조 실행에서 그대로 읽었습니다).

| 항목 | Windows |
|---|---|
| `veh` 배달 총계 | 1,141,066 |
| 프레임 | 60,761 |
| **프레임당 배달** | **18.8회** |
| **배달당 cycle** | **약 104,000** |
| 커널 왕복 바닥 (`gap min`) | 21,756 cycle |
| single-step gap 평균 | 28,185 cycle |

`gap`은 Task 372가 넣은 것으로, **핸들러가 나간 뒤 다음 진입까지의 간격**입니다 — 곧 커널의
전달 경로 자체입니다. single-step 사이에는 게스트가 명령을 정확히 하나만 실행하므로 그 클래스가
가장 순수한 왕복 값이고, `min`은 그 어떤 표본도 그보다 쌀 수 없는 **바닥**입니다.

**두 값이 함께 있어야 답이 나옵니다.** `cycles/delivery`는 핸들러 본문까지 포함하지만 `gap`은
커널 경로만 봅니다. 단가가 크다면 그중 어디인지를 이 둘이 갈라 줍니다.

## 결정 — live 보고 줄에 실습니다. 그게 전부입니다

`Win32ExecutionTimeProfile`은 이미 버킷마다 `counts`를 `cycles`와 **같이** 쌓고 있고, gap
배열도 이미 채워집니다. **새로 세는 것이 없습니다.** 511이 만든 보고기가 그 값들을 찍지 않고
있을 뿐입니다.

한 줄에 다 넣지 않고 **두 번째 줄**로 냅니다. 기존 줄은 몫을, 새 줄은 전달의 해부를 담습니다 —
한 줄이 길어지면 사람도 스크립트도 읽기 나빠집니다.

```
[repiu-live-veh] #3 veh_count=… per_frame=… cycles_per_veh=… gap_min=… gap_ss_mean=… gap_ss_count=…
```

## 확인해야 하는 것 — Linux에서 이 필드들이 실제로 채워지는가

**이 세션에서 이미 두 번 걸린 함정입니다** — 채워지지 않는 필드를 보고 결론을 내리는 것.

코드를 읽어 두 자리 모두 플랫폼 공용 경로임을 확인했습니다.

* `ExecutionTimeScope(kVehTotal)`은 `DispatchGuestFault` 안, 주석이 *"the single choke point
  for every exception the guest thread takes"*라고 적은 자리입니다.
* `RecordVehExceptionGap`은 `RecordVehExceptionCensus` 안이고, 그 census는 `FaultEvent`를
  받습니다 — 3c 계층의 것이므로 양쪽 호스트가 같습니다.

**그래도 첫 실행에서 값이 0이 아닌지 눈으로 확인하고 시작합니다.** 읽어서 맞다는 것과 채워진다는
것은 다릅니다.

## 이 작업이 하지 않는 것

* **고치지 않습니다.** 512는 가르는 데까지입니다.
* 하위 버킷(`kVehPrologue`·`kAotReentry` 등)으로 내려가지 않습니다. 횟수/단가가 갈리면 그 다음이
  자연히 정해집니다.
* `dos`의 433.9배는 별도 단위입니다.

---

# Task 512 — signal delivery: a count problem or a price problem

Work order: [20260828-512](../work-orders/20260828-512-signal-delivery-count-or-price.md) ·
Work log: [20260828-512](../work-logs/20260828-512-signal-delivery-count-or-price.md) ·
Frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
Predecessor: [20260828-511](20260828-511-live-execution-profile-report.md)

## Background

Task 511 settled the axis: **the fault handler is 68.3% of the per-frame gap**, 88.072M cycles on
Linux against Windows' 2.065M, a factor of 42.6. On Linux that handler is signal delivery.

A 42.6x is one of two multiplications:

$$\frac{\text{cycles}}{\text{frame}} = \frac{\text{deliveries}}{\text{frame}} \times \frac{\text{cycles}}{\text{delivery}}$$

**Which one carries the 42.6x decides the fix completely.**

| If | Meaning | Direction of a fix |
|---|---|---|
| the **count** is high | Linux faults more often | remove or merge the places that create faults |
| the **price** is high | the delivery path costs | make the boundary something other than a signal |

Neither should be touched before the two are separated.

## Confirmed — the Windows half already exists

Only half of this needs measuring. Windows reaches its shutdown summary, so it already prints all of
it, and 511's control run was read straight off:

| Item | Windows |
|---|---|
| Total `veh` deliveries | 1,141,066 |
| Frames | 60,761 |
| **Deliveries per frame** | **18.8** |
| **Cycles per delivery** | **about 104,000** |
| Kernel round-trip floor (`gap min`) | 21,756 cycles |
| Single-step gap mean | 28,185 cycles |

The `gap` is Task 372's: **the interval from handler exit to the next entry**, which is the kernel's
delivery path itself. Between two single steps the guest executes exactly one instruction, so that
class reads as the purest round trip, and `min` is a **floor** no sample can go below.

**Both numbers are needed together.** `cycles/delivery` includes the handler body; `gap` sees only the
kernel path. If the price is what is high, these two say which part of it.

## Decision — put them on the live line. That is the whole change

`Win32ExecutionTimeProfile` already accumulates `counts` **alongside** `cycles` for every bucket, and
the gap arrays already fill. **Nothing new is counted.** 511's reporter simply does not print them.

They go on a **second line** rather than swelling the first: one line for the shares, one for the
anatomy of a delivery. A single long line reads worse for both people and scripts.

```
[repiu-live-veh] #3 veh_count=… per_frame=… cycles_per_veh=… gap_min=… gap_ss_mean=… gap_ss_count=…
```

## What has to be checked — that these fields actually fill on Linux

**This session has already been caught twice** by reading a conclusion off a field that was never
filled.

Reading the code, both sites are on the platform-neutral path:

* `ExecutionTimeScope(kVehTotal)` sits inside `DispatchGuestFault`, at the place whose comment calls
  it *"the single choke point for every exception the guest thread takes"*.
* `RecordVehExceptionGap` sits inside `RecordVehExceptionCensus`, and that census takes a
  `FaultEvent` -- 3c's type, identical on both hosts.

**Even so, the first run is checked for non-zero values before anything is concluded.** Reading that
it should fill and seeing that it does are different things.

## What this task does not do

* **It does not fix anything.** 512 goes as far as separating the two.
* It does not descend into the sub-buckets (`kVehPrologue`, `kAotReentry` and the rest). Once count
  and price are separated, what comes next follows on its own.
* `dos` at 433.9x is a separate unit.
