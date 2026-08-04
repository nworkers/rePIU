# Task 418 작업 로그 — 비용 프로파일 재기준선 (**port I/O 축 소멸, Glide rendezvous 등장**)

설계: [20260804-418](../design/20260804-418-cost-profile-rebaseline.md) ·
작업 지시: [20260804-418](../work-orders/20260804-418-cost-profile-rebaseline.md)

## 1. 한 줄 결과

**port I/O 축은 사라졌습니다** — wall의 41.9~49.7%였던 것이 **0.5%**입니다. 새 지배
항목은 Glide gate이고, 그 안에서 게스트 스레드가 실제로 하는 일은 **host thread를
기다리는 것**입니다(host 표본의 **74.8~76.3%**가 `InvokeOnHostThread`의 조건변수 대기).

## 2. 실행

같은 Release 빌드(22:57 빌드, 세 스위치 문자열 확인)·같은 세션·EEPROM 실행별 격리·
60초. 그룹 A(인용 가능) pumpit3 5회 + pumpit1 2회, 그룹 B(시간 인용 금지) pumpit3 2회.

**창 상태 때문에 한 세트를 버렸습니다.** 첫 9회는 창을 최소화해 띄웠는데 **pumpit1이
12,119 / 11,888 프레임**으로 기준선(2,735~2,865)의 **4.2배**였습니다. 설계 §4가 대조로
둔 pumpit1이 바로 이 목적이었고, 여기서 걸렸습니다. 창을 정상으로 바꿔 전량 재측정한
두 번째 세트만 아래에 씁니다.

**부수 확인 — pumpit1은 표시 제한이고 pumpit3는 아닙니다.** 같은 창 상태 변경에서
pumpit3는 2,252~2,974 → 2,477~2,515로 사실상 그대로입니다. Task 402의 결론과 같은
방향입니다.

## 3. 검산 — 유효 표본 7회, 제외 2회

| run | 프레임 | traces | 격리 | 세대실패/스킵/격리대체/걸친활성화 | exit 총수==합 |
|---|---:|---:|---:|---|---|
| a3-2 | 2,477 | 8 | 0 | 0/0/0/0 | 38,890 == 38,890 |
| a3-3 | 2,508 | 8 | 0 | 0/0/0/1 | 통과 |
| a3-4 | 2,502 | 8 | 0 | 0/0/0/0 | 통과 |
| a3-5 | 2,515 | 8 | 0 | 0/0/0/1 | 통과 |
| a1-2 (pumpit1) | 3,124 | 16 | 0 | 0/0/0/0 | 통과 |
| b3-1 | 2,504 | 8 | 0 | 0/0/0/0 | 통과 |
| b3-2 | 2,349 | 8 | 0 | 0/0/0/0 | 통과 |
| **a3-1** | **0** | 4 | — | — | **제외 — 부팅 크래시** |
| **a1-1** | **0** | 0 | — | — | **제외 — 재배치 베이스 실패** |

그룹 B 검산도 통과 — `3,295 = 3,169 + 126 + 0`, overflow 0, parts-match true.

**정상 모드 확인:** 유효 7회 전부 격리 0·세대 실패 0입니다. 즉 Tasks 415·417 이후의
기본 경로만 측정했습니다.

## 4. 표 1 — port I/O 축의 소멸 (pumpit3)

| 지표 | Task 414 이전 | 이번(중앙값) | 변화 |
|---|---:|---:|---|
| port I/O 예외 비중 | 90.4~92.9% | **19.4%** | 약 1/5 |
| port I/O 총 횟수 | 1,772,285 | **74,438** | **24배 감소** |
| `0x…1DB22` 단일 비중 | 85.9~97.2% | **32.3%** | — |
| port I/O **cycles / guest-run** | wall 41.9~49.7% | **0.5%** (1.1~1.3G / 222.1G) | 축 소멸 |

`cache`는 여전히 모든 항목에서 0입니다 — 그 코드는 지금도 arena에서 돕니다. 다만
**그 사실이 이제 비용을 설명하지 않습니다.**

## 5. 표 2 — 새 지배 항목 (pumpit3, 유효 4회)

| 층 | 지표 | 값 |
|---|---|---|
| 예외 | single-step / **breakpoint** / AV / other | 8.5~8.7% / **68.4~69.4%** / 2.8~2.9% / 19.3~20.0% |
| 예외 | breakpoint provenance = **HLE 경계** | 257,004~267,660 (**약 83%**) |
| 시간 | VEH gap이 wall에서 | **95.7~96.0%**, 그 중 breakpoint가 **97.9%** |
| 시간 | glide-gate cycles / guest-run | **54~55%** (120.7~122.8G / 222.1G) |
| 호스트 | 최대 호출 지점 | **`GlideOpenGlBackend::InvokeOnHostThread+0x1F7` 74.8~76.3%** |
| 호스트 | 2위 이하 | 전부 **1.6% 이하** |
| 호스트 | 게스트 스레드 CPU share | **50.4~54.0%** |

**그 지점은 대기입니다.** `glide_opengl_backend.cpp:295`의
`host_command_cv_.wait(lock, [] { return host_command_complete_; })`이며, 게스트 스레드는
Glide 호출마다 host thread와 왕복합니다(이 실행에서 gate 호출 **2,459,898회**). CPU
share가 50~54%인 것이 같은 사실의 다른 표현입니다 — **스레드는 절반을 기다립니다.**

## 6. 표 3 — Task 412의 순위는 이월되지 않았습니다

설계 §1이 예고한 대로입니다. 왼쪽은 멈춘 실행(프레임 0~1), 오른쪽은 정상 실행입니다.

| 심볼 | Task 412(멈춤) | 이번(정상) |
|---|---:|---:|
| `InvokeOnHostThread` | 표에 없음 | **74.8~76.3%** |
| 세그먼트 override 재해석 | 15.1% | 상위 10위 밖 (≤1%) |
| `WriteGuestBytes` | 13.6% | 상위 10위 밖 |
| `FindAotCacheAddress` | 12.7% | **0.73%** |
| JAMMA 스냅샷 | 10.4% | 상위 10위 밖 |

**frontier 항목 4의 근거는 이것으로 소진됐습니다.** 그 순위는 멈춤의 것이었고,
정상 실행에서는 어느 것도 1%를 넘지 않습니다.

**정정 — 항목 5(return IC thrash)까지 함께 소진으로 적었던 것은 틀렸습니다.** host
표본은 **시간** 축인데 0c의 주장은 **횟수** 축이므로, 위 표로는 그 항목을 닫을 수
없습니다. 같은 로그를 횟수 축으로 다시 읽은 결과가 아래이며, 결론은 **실재하나 비용
미격리**입니다.

| 관측 | a3-2 | a3-5 | 뜻 |
|---|---:|---:|---|
| return entry/attempt/success | 194,225 | 198,297 | fallback은 양쪽 다 **0** |
| inline-cache patch attempt/success | **194,341** | 198,413 | return + inline breakpoint(116)과 **정확히 일치** |
| breakpoint provenance `inline` | 116 | 116 | 이 19.4만 회는 **예외가 아닙니다** |
| transfer `return` cycles / guest-run | **9.4%** | 9.3% | 자체 share가 **482.54%** → 버킷 중첩, **상한** |
| host 표본 해당 경로 합 | 약 2~3% | — | `ResolveAotTransferTarget`·`RequestAotInlineCachePatch`·`FindAotCacheAddress` |

**return 한 번마다 IC를 한 번 재패치합니다** — 4-entry IC가 전혀 안정되지 않는다는
0c의 관찰 그대로입니다. 다만 그 경로는 **예외 없는** miss tail 직접 호출이므로 "예외
횟수를 줄인다"는 원래 동기는 성립하지 않고, 배타 비용은 **2~3%(하한)~9.4%(상한)**
사이에서 미확정입니다.

## 7. 판정 — 사전 등록한 결정 트리 그대로

port I/O가 예외의 19.4%로 **50% 미달** → host 최대 인구로 분기 → **Glide gate** →
**gate 본체 분해**가 다음 축입니다. 76%는 "단일 축 없음" 문턱(30%)을 크게 넘으므로 그
분기가 아닙니다.

**다음 작업의 첫 측정은 이미 정해져 있습니다.** rendezvous를 publish → host 시작 →
host 종료 → resume 네 구간으로 나누는 계측은 코드에 **이미 있으나**
(`RecordGlideGatePublish` / `RecordGlideGateResume` / `RecordGlideOrdinalRendezvous`)
이번 실행에서 `Glide ordinal backend rendezvous/direct: 0/0`으로 **꺼져 있었습니다.**
그것을 켜면 76%가 **대기(host가 일하는 동안)** 인지 **왕복 비용(깨우고 잠드는 것)** 인지
한 번의 측정으로 갈립니다. 이 구분에 따라 대응이 정반대입니다 — 전자는 host 쪽 작업을
줄여야 하고, 후자는 왕복 자체를 없애야 합니다(Task 365의 생략이 그 방향이었습니다).

## 8. 부수 확인 — 항목 8이 9회 중 2회

| run | 증상 |
|---|---|
| a3-1 | `VirtualAlloc MEM_RESERVE failed with error 487` → arena base `0x07000000` → `INT 21h AH=4Ah` 위치에서 사망(`B4 4A CD 21`) |
| a1-1 | `0x07000000`/`0x08000000`/`0x09000000` 전부 occupied → `Failed to reserve an available relocated image base` |

이전 기록은 8회 중 1회였습니다. **재현율이 낮지 않습니다.**

## 9. 회고

* **대조가 세트 하나를 살렸습니다.** pumpit1을 같이 돌리지 않았다면 최소화 창에서 나온
  분포를 그대로 썼을 것입니다. 4.2배는 코드로 설명되지 않는 크기였고, 그래서 걸렸습니다.
* **"멈춤의 비용 ≠ 정상의 비용"이 수치로 확인됐습니다.** 설계 단계에서 의심만 했던 것을
  표 3이 확정합니다. 상위 4개가 전부 자리를 잃었습니다.
* **결정 트리를 미리 고정한 것이 값을 했습니다.** 76%라는 큰 수를 본 뒤에 기준을
  정했다면 "당연히 이것"이라고 적었을 텐데, 30% 문턱과 분기를 미리 써 둔 덕분에 판정이
  기계적이었습니다.
* **계측이 이미 있는데 꺼져 있었습니다.** rendezvous 4구간 분해가 그렇습니다. 다음
  작업은 구현이 아니라 **스위치를 켜는 것**에서 시작합니다.

---

# Task 418 Work Log — re-baselining the cost profile (**the port I/O axis is gone**)

## 1. Result in one line

**The port I/O axis has disappeared** — from 41.9-49.7% of wall to **0.5%** — and the new
dominant item is the Glide gate, inside which what the guest thread actually does is **wait
for the host thread**: **74.8-76.3%** of host samples sit in `InvokeOnHostThread`'s condition
variable wait.

## 2. Runs

One Release build (built at 22:57, all three switch strings verified), one session, EEPROM
isolated per run, 60 seconds each: group A five pumpit3 plus two pumpit1, group B two
pumpit3. **One whole set was discarded over window state**: the first nine ran minimized and
**pumpit1 produced 12,119 and 11,888 frames against a 2,735-2,865 baseline — 4.2x**. The
pumpit1 control existed for exactly this, and it caught it; only the re-run with a normal
window is used below. Incidentally this confirms **pumpit1 is display-limited and pumpit3 is
not**: the same change moved pumpit3 only from 2,252-2,974 to 2,477-2,515.

## 3. Cross-checks — seven valid samples, two excluded

Frames 2,477-2,515 for pumpit3, 3,124 for pumpit1, and 2,349-2,504 for group B, all with
eight or more DOS path traces, **zero quarantines and zero generation failures**, and the
`arena single-step exit` sum equal to its total. Group B also satisfies
`3,295 = 3,169 + 126 + 0` with zero overflow. Two runs are excluded as boot crashes (below).

## 4. The port I/O axis collapsed

Port I/O is **19.4%** of exceptions against 90.4-92.9% before, **74,438** operations against
1,772,285 (**24x**), the single hottest address is **32.3%** against 85.9-97.2%, and in cycles
it is **0.5%** of the run (1.1-1.3 G of 222.1 G). Its `cache` count is still zero everywhere —
that code still runs in the arena — but **that fact no longer explains any cost**.

## 5. What dominates now

Exceptions are **68.4-69.4% breakpoint** (against 8.5-8.7% single-step, 2.8-2.9% access
violation, 19.3-20.0% other), and **about 83%** of those breakpoints come from the HLE
boundary. The VEH gap is **95.7-96.0%** of wall and **97.9%** of it is breakpoint. Glide gate
cycles are **54-55%** of the run. In the host layer, one call site holds
**74.8-76.3%** — `GlideOpenGlBackend::InvokeOnHostThread+0x1F7` — with nothing else above
1.6%, while the guest thread's CPU share is only **50.4-54.0%**. That site is the
condition-variable wait at `glide_opengl_backend.cpp:295`: every Glide call is a round trip to
the host thread, 2,459,898 of them in this run, and the CPU share is the same fact stated
differently — **the thread spends half its time waiting**.

## 6. Task 412's ranking did not carry over

As the design predicted. `InvokeOnHostThread` was absent from the stalled run's table and is
now 74.8-76.3%; segment-override re-resolution fell from 15.1% to outside the top ten,
`WriteGuestBytes` from 13.6% to outside it, `FindAotCacheAddress` from 12.7% to 0.73%, and the
JAMMA snapshot from 10.4% to outside it. **The evidence under frontier item 4 is spent**: that
ranking belonged to the stall, and in a healthy run none of it reaches one percent.

**Correction — item 5, the return inline-cache thrash, was wrongly folded into that.** Host
samples measure time while 0c's claim is about counts, so this table cannot close it. Read on
the count axis, the same logs show 194,225 return dispatches against **194,341** inline-cache
patches — returns plus the 116 inline breakpoints, exactly — so **every return re-patches a
four-entry cache**, with zero fallbacks. Those 194 thousand events are **not exceptions**: the
`inline` breakpoint provenance is 116, and the path enters through the miss tail directly. Its
cost stays unisolated between a **2-3%** floor in host samples and a **9.4%** ceiling from the
transfer handler's `return` bucket, whose own share reads 482.54% and therefore nests. **The
thrash is confirmed and its cost is not**, and being off the exception axis, its original
motive — cutting exception count — does not apply.

## 7. Verdict — the pre-registered tree, unchanged

Port I/O at 19.4% is below the 50% branch, so the verdict follows the largest host population
to the **Glide gate**, and the next axis is **decomposing the gate body**. At 76% it is far
above the 30% "no single axis" threshold. **The next task's first measurement is already
determined**: the instrumentation that splits the rendezvous into publish, host start, host
finish and resume already exists (`RecordGlideGatePublish`, `RecordGlideGateResume`,
`RecordGlideOrdinalRendezvous`) but was **off** in these runs
(`Glide ordinal backend rendezvous/direct: 0/0`). Turning it on separates, in one measurement,
whether the 76% is **waiting while the host works** or **the cost of the round trip itself** —
and the two demand opposite remedies, the first reducing host-side work and the second removing
the round trip, which is the direction Task 365's elision took.

## 8. Boot crashes: two runs in nine

`a3-1` hit `VirtualAlloc MEM_RESERVE failed with error 487`, landed the arena at `0x07000000`,
and died at the `INT 21h AH=4Ah` site (`B4 4A CD 21`); `a1-1` found `0x07000000`, `0x08000000`
and `0x09000000` all occupied and failed to reserve a relocated image base at all. The earlier
record was one run in eight, so **the reproduction rate is not low**.

## 9. Retrospective

The control saved a set: without running pumpit1 alongside, the minimized-window distribution
would have been written up as the baseline, and 4.2x is a size no code change explains. "A
stall's cost is not a healthy run's" is now measured rather than suspected — all four leaders
lost their place. Fixing the decision tree in advance paid: after seeing a 76% population it
would have been easy to call it obvious, but the threshold and branches were already written.
And the instrumentation for the next step exists and was simply switched off, so the next task
starts by enabling it rather than by building anything.
