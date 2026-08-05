# Task 430 작업 로그 — 틱 손실을 시간축에 올렸고, 누적 평균이 왜 무의미한지가 즉시 드러났습니다

설계: [20260805-430](../design/20260805-430-timer-tick-time-series.md) ·
작업 지시: [20260805-430](../work-orders/20260805-430-timer-tick-time-series.md) ·
선행: [Tasks 421~423](20260805-421-423-cd-audio-and-stall-root-cause.md)

## 1. 한 줄 결과

**후보 1‴ 확정입니다.** 사용자가 **점프 증상을 실제로 본** 실행에서, 음악은 정확히
실시간으로 흐르는 동안 게스트 시계는 **실시간의 51% 속도**로 갔고, 28.3초 구간에서
**13.9초** 뒤처졌습니다. 판정은 [§9](#9-판정--후보-1-확정)에 있습니다.

## 2. 재실행 없이 기존 산출물에서 확인한 것

Task 421 census(295초)와 Task 422 trace를 다시 집계했습니다.

**확인 — 음악 위치는 정확합니다(421 결론 독립 재확인).** 같은 generation 연속 표본
1,734개에서 평균 `delta_lba` **8.206**(기대 8.21), 분포는 7·8·9뿐, 역행 0·정지 0,
worker 반복 31~44회/표본, underrun 9회는 **generation 전환 9회와 정확히 일치**하여
재생 중 발생분이 0입니다.

**새 사실 — 게스트 메인 루프는 실시간 60 Hz입니다.** trace의 IOCTL 12 폴링을 초 단위로
집계하면 gameplay 137초 중 이상치가 **양끝 잘린 버킷 2건뿐**이고 나머지는 전부
59~61 polls/s입니다. 정밀 구간은 다음과 같습니다.

```
7,200 polls / 119.968 s = 60.0160 Hz   (오차 0.03%)
```

**그래서 후보 1‴이 데이터와 충돌합니다.** 루프가 4틱(240 Hz ÷ 4) 대기라면 4% 손실 시
57.6 Hz여야 하는데 실측은 60.016 Hz입니다. 그런데 우리가 가진 것은 누적 한 줄
(`due/injected/coalesced/dropped: 41531/39830/1677/24`)뿐이라 **그 4%가 언제 생겼는지
말하지 못합니다.** Task 421 설계 §3의 공백이 그대로 반복된 것이고, 이것이 이 Task의
동기입니다.

## 3. 만든 것

| 파일 | 내용 |
|---|---|
| `cd_audio_position_census.h` | 표본에 `timer_ticks_due`·`timer_ticks_injected` |
| `live_telemetry_snapshot.cpp` | 표본 시점에 틱 카운터 차분 — `worker_iterations`와 **같은 규칙** |
| `cd_audio_position_census.cpp` | 열 2개 + 파생 열 `tick_lag_ms` |
| `docs/guides/cd-audio-position-census.md` | 새 열과 판정 규칙 |

**동작 불변** — 이미 유지되는 카운터를 원자 읽기 2회로 읽을 뿐이고, 주입 정책·safe
point·`REPIU_TIMER_TICK_BACKLOG` 기본값은 건드리지 않았습니다.

## 4. 검증

**probe** — `repiu_aot_probe` 전체 통과(exit 0), timer tick delivery 항목 9개 전부
`true`. 카운터 의미가 바뀌지 않았음을 확인합니다.

**스모크(pumpit1, 44.5초)** — 새 열 3개가 채워지고, census 합계가 종료 요약과
일치합니다(due **10,163 = 10,163**, injected 4,381 대 4,382 — 차이 1은 마지막 표본
이후에 온 틱이므로 정상). 항등식도 성립합니다: `10,163 = 4,382 + 5,744 + 37 + 0`.

## 5. 스모크가 곧바로 보여 준 것 — 누적 평균은 어느 구간도 설명하지 못합니다

| 구간 | due | injected | 전달률 |
|---|---:|---:|---:|
| 0~30초 | 6,671 | 1,670 | **25.0%** |
| 30~35초 | 1,179 | 989 | 83.9% |
| 35~40초 | 1,208 | 1,191 | **98.6%** |
| 40~45초 | 1,105 | 531 | 48.1% |
| 전체 | 10,163 | 4,382 | 43.1% |

**전달률이 25%에서 98.6%까지 요동칩니다.** 누적 평균 43.1%는 그 어느 구간의 값도
아닙니다. 종료 요약 한 줄로 이 축을 판단하던 것이 왜 위험했는지가 그대로 보입니다.

**주의 — 이 표는 pumpit1 attract 구간이며 사용자가 보고한 증상의 맥락이 아닙니다.**
계측이 손실을 실제로 분해한다는 검증일 뿐, 이것으로 후보 1‴을 판정하지 않습니다.

## 6. 설계 판단 하나가 측정으로 옳았음이 확인됐습니다

`tick_lag_ms`에 240 Hz를 상수로 박지 않은 것(설계 §5)이 맞았습니다. 스모크 첫 1초의
`ticks_due`는 표본당 **1~2**로 약 **18.2 Hz**이고, 이는 게스트가 아직 divisor를 바꾸기
전의 **DOS 기본 BIOS 틱 주파수**입니다. 이후 표본당 24~28(**240 Hz**)로 전이합니다.
240을 상수로 썼다면 부팅 구간의 지연이 13배 과대 계산됐을 것입니다.

## 9. 판정 — 후보 1‴ 확정

사용자 측정(2026-08-06, pumpit1, 47초, census 427표본). **사용자가 이 실행에서 노트·BGA
점프 증상을 실제로 확인했습니다.**

```
timer tick delivery due/injected/coalesced/dropped/deferred: 10942/5661/5256/25/0
```

항등식 성립(`10,942 = 5,661 + 5,256 + 25 + 0`), 전체 전달률 **51.7%**.

**본곡 구간(generation 7, wall 18,609~46,953 ms, `start_lba 20545`):**

| 지표 | 값 | 설계 §6 기준 |
|---|---|---|
| 구간 길이 | 28.34초 | — |
| 음악 진행 | LBA 20,553 → 22,678 = **74.97 LBA/s** | 정확(오차 0.04%) |
| `injected/due` | **약 51%** | ≤ 96% → **확정** |
| `tick_lag_ms` | 8,810 → **22,666** (**+13,856 ms**) | 단조 증가·> 1,000 ms → **확정** |

**28.3초 동안 음악은 28.3초 흘렀는데 게스트 시계는 14.5초만 흘렀습니다.** 사전 등록
기준 두 갈래를 모두, 근소하게가 아니라 압도적으로 넘습니다.

### 9.1 시계열이 아니었으면 보이지 않았을 구조

같은 실행 안에서 구간마다 정반대입니다.

| 구간 | 전달률 | `tick_lag_ms` |
|---|---|---|
| 프리뷰(gen 3·5·6, 14.3~18.5초) | **약 100%**(26/26, 27/27) | 9,006 → 8,793 (**감소**) |
| 본곡(gen 7, 18.6초~) | **약 51%** | 8,810 → 22,666 (**+13,856**) |

프리뷰에서는 완벽하고 본곡에서만 반토막입니다. 종료 요약의 누적 51.7% 한 줄로는 이
구조가 보이지 않으며, 이것이 Task 430의 존재 이유였습니다.

### 9.2 기전 — `deferred = 0`이 범위를 좁힙니다

`deferred`(IF=0 또는 비게스트 EIP로 주입을 미룬 횟수)가 **0**입니다. 주입이 조건에
막힌 것이 아니라 **`InjectPendingInterrupts`에 도달하는 빈도 자체가 240 Hz보다
낮습니다.** 주입 5,661회 / 47초 = **초당 약 120회**이고 due는 240 Hz이므로, 밀린 틱이
평균 8.3 ms 기다리다 절반이 coalesce로 버려집니다.

**따라서 `REPIU_TIMER_TICK_BACKLOG=1`로는 고쳐지지 않습니다.** 기회가 초당 120회인데
240회가 밀리면 backlog는 상한 64에 붙고 초과분이 `dropped`가 될 뿐이며, 이는 Task 366이
관측한 판정 T3과 정확히 같은 그림입니다. **다음 축은 주입 정책이 아니라 주입 기회
빈도**입니다.

## 10. 정정 — 제 예상이 틀렸습니다

§7에 "게스트 루프가 60.016 Hz로 안정적이므로 1‴이 **기각될 쪽**으로 본다"고 적었으나
**틀렸습니다.** 그 60 Hz는 08-05 pumpit3 실행(전달률 95.9%)의 값이고 이번 측정은
pumpit1(51%)입니다. **서로 다른 실행의 수치를 한 결론에 묶은 것**이 오류이며,
frontier에 이미 적혀 있는 "세션 간 절대 비교는 성립하지 않는다"를 제가 어겼습니다.

**판정표를 측정 전에 고정해 둔 덕분에 예상과 무관하게 데이터가 판정했습니다.** 예상을
먼저 세웠더라면 51%를 보고도 "루프는 60 Hz니까 괜찮다"로 읽었을 수 있습니다.

## 7. 다음 — 사용자 측정

가이드대로 **증상이 보이는 gameplay까지** 플레이한 뒤 `build/cd_audio_position_census.txt`와
stderr 로그를 주시면, 설계 §6 표를 그대로 적용해 후보 1‴을 확정 또는 기각합니다.

```
set REPIU_CD_AUDIO_POSITION_CENSUS=1
set REPIU_EXECUTION_TIMEOUT_MS=0
```

**예상은 적어 두되 판정표는 손대지 않았습니다.** 게스트 루프가 60.016 Hz로 안정적인
점 때문에 저는 1‴이 **기각될 쪽**을 봅니다만, 그것은 예상이지 판정이 아닙니다.
Task 420이 "그럴듯한 서사"를 한 번의 측정에 뒤집힌 선례가 있습니다.

> **[측정 후] 이 예상은 틀렸습니다 — §9 판정과 §10 정정을 보십시오.**

## 8. 회고

* **재실행보다 기존 산출물 재집계가 먼저였습니다.** 60.016 Hz는 이미 디스크에 있던
  Task 422 trace에서 나왔습니다. Task 419가 켜려던 계측이 이미 Task 418 로그 안에
  있었던 것과 같은 교훈입니다.
* **스모크가 검증을 넘어 근거를 하나 줬습니다.** 계측이 도는지만 보려던 실행이
  "누적 평균은 무의미하다"를 직접 보여 줬습니다.
* **판정표를 미리 고정한 것을 이번에도 지켰습니다.** 예상(1‴ 기각)이 있는 상태에서
  기준을 쓰면 기준이 예상을 따라가므로, 설계 §6을 먼저 쓰고 그 뒤에 예상을 적었습니다.
* **주의 하나:** Task 421~423 로그의 "후보 A~E 전부 기각"은 과했습니다. 그 근거는
  진행률·역행·정지 관측인데, 후보 A·E는 **일정한 오프셋**이라 그 측정으로는 보이지
  않습니다. 증상이 "점프"라 우선순위는 낮지만, 기각됐다고 적힌 것은 정정해 둡니다.

---

# Task 430 Work Log — candidate 1‴ confirmed: the guest clock runs at half speed

## 1. Result in one line

**Candidate 1‴ is confirmed.** In a run where the user **saw the jumping symptom**, the music
advanced in exact real time while the guest clock ran at **51% of real time**, falling **13.9
seconds behind over a 28.3-second stretch**. The verdict is in section 9.

## 2. Found in existing artefacts, with no new runs

Re-aggregating Task 421's 295-second census **independently confirms** its conclusion: 1,734
consecutive same-generation samples at a mean `delta_lba` of **8.206** against 8.21,
distributed over 7, 8 and 9 only, with zero regressions and zero freezes, worker iterations of
31-44 per sample, and nine underruns matching the nine generation changes exactly — none
during playback.

**New fact — the guest's main loop runs at real-time 60 Hz.** Bucketing Task 422's IOCTL 12
polls by second over 137 seconds of gameplay leaves **only two outliers, both truncated edge
buckets**; every other second holds 59-61 polls. Measured over the steady window, **7,200
polls in 119.968 s is 60.0160 Hz — 0.03% error.**

**That collides with candidate 1‴:** a loop waiting four ticks would run at 57.6 Hz under 4%
loss, not 60.016 Hz. But the only figure available is one cumulative line, which **cannot say
when** the 4% occurred — Task 421 §3's gap, repeated, and this task's motive.

## 3. Built

`timer_ticks_due` and `timer_ticks_injected` on the census sample, differenced at sample time
by **the same rule** `worker_iterations` already uses, plus the derived `tick_lag_ms` column
and the guide's new readings. **Behaviour is unchanged** — two atomic reads of counters
already maintained, with injection policy, safe points and the `REPIU_TIMER_TICK_BACKLOG`
default all untouched.

## 4. Verification

`repiu_aot_probe` passes end to end with all nine timer-tick assertions `true`, so counter
semantics are unchanged. A 44.5-second pumpit1 smoke fills the three new columns, and the
census sums match the exit summary (due **10,163 = 10,163**; injected 4,381 against 4,382, the
difference being one tick owed after the final sample), with the partition identity holding at
`10,163 = 4,382 + 5,744 + 37 + 0`.

## 5. What the smoke showed immediately

Delivery over that run: **25.0%** across the first 30 seconds, 83.9% from 30-35 s, **98.6%**
from 35-40 s, and 48.1% from 40-45 s, against a cumulative **43.1%**. **Delivery swings
between 25% and 98.6%, and the run-long average describes none of it** — which is exactly why
judging this axis from one exit line was unsafe. **This is pumpit1's attract phase and not the
context of the reported symptom**; it verifies that the instrument decomposes real loss, and
it is not used to judge candidate 1‴.

## 6. A design choice confirmed by measurement

Not hard-coding 240 Hz into `tick_lag_ms` (design section 5) was right. The smoke's first
second shows `ticks_due` of **one to two per sample — about 18.2 Hz**, the **DOS default BIOS
tick rate** before the guest reprograms the divisor, transitioning afterwards to 24-28 per
sample (**240 Hz**). A hard-coded 240 would have overstated the boot-phase lag thirteenfold.

## 7. Next — the user's measurement

Play to where the symptom appears and send `build/cd_audio_position_census.txt` with the
stderr log; design section 6's table then names candidate 1‴ confirmed or rejected. **The
expectation is recorded without touching the readings:** the loop's steady 60.016 Hz leads me
to expect 1‴ **rejected**, but that is an expectation and not a verdict — Task 420's plausible
story was overturned by a single measurement.

## 9. Verdict — candidate 1‴ confirmed

User measurement, 2026-08-06, pumpit1, 47 seconds, 427 samples, and **the user confirmed
seeing the note and BGA jumping in this run**. The exit line reads
`due/injected/coalesced/dropped/deferred: 10942/5661/5256/25/0`, the partition identity holds
(`10,942 = 5,661 + 5,256 + 25 + 0`), and overall delivery is **51.7%**.

Over the main track (generation 7, wall 18,609-46,953 ms, `start_lba 20545`) the music advances
**74.97 LBA/s — exact to 0.04%** — while `injected/due` sits at about **51%** and `tick_lag_ms`
climbs monotonically from 8,810 to **22,666, a growth of 13,856 ms**. Both pre-registered
thresholds (at or below 96%, and monotonic growth past 1,000 ms) are cleared by a wide margin:
**the music ran 28.3 seconds while the guest clock ran 14.5.**

**The structure only a time series shows:** in the same run, delivery is about **100%** through
the preview tracks (generations 3, 5 and 6, where `tick_lag_ms` actually *falls* from 9,006 to
8,793) and about **51%** through the main track. The cumulative 51.7% describes neither, which
is exactly why this task existed.

**`deferred = 0` narrows the mechanism.** No injection was ever blocked by IF=0 or a non-guest
instruction pointer, so ticks are not being refused — `InjectPendingInterrupts` is simply
reached less often than 240 Hz. At 5,661 injections over 47 seconds the opportunity rate is
about **120 per second**, so an owed tick waits ~8.3 ms and every other one coalesces away.
**`REPIU_TIMER_TICK_BACKLOG=1` therefore cannot fix this**: with opportunities at 120/s against
240/s owed, the backlog pins at its cap of 64 and the excess becomes `dropped` — precisely Task
366's T3 reading. **The next axis is the opportunity rate, not the delivery policy.**

## 10. Correction — my expectation was wrong

Section 7 recorded an expectation that 1‴ would be **rejected**, reasoning from the guest loop's
steady 60.016 Hz. That was wrong: the 60 Hz figure comes from the 08-05 pumpit3 run, whose
delivery was 95.9%, while this measurement is pumpit1 at 51%. **Binding figures from two
different runs into one conclusion** is the error, and it breaks the rule already written in the
frontier that cross-session absolute comparison does not hold. Fixing the readings in the design
*before* measuring is what kept the expectation from deciding the verdict — had the bar been set
afterwards, 51% could have been read as "fine, the loop is still 60 Hz".

## 8. Retrospective

Re-aggregating what was already on disk came before re-running: the 60.016 Hz figure came out
of Task 422's existing trace, the same lesson as Task 419's instrumentation already sitting in
Task 418's logs. The smoke run gave evidence beyond its purpose. And the readings were fixed
in the design *before* the expectation was written down, so the bar could not drift toward the
guess. One correction: Tasks 421-423's "candidates A through E all rejected" was too strong —
that evidence was rate, regression and freeze, none of which can see the **constant offset**
that A and E produce. The symptom is a jump, so it stays low priority, but the rejection is
corrected here.
