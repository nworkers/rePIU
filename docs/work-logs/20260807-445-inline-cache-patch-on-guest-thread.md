# Task 445 작업 로그 — inline cache 패치의 스레드 왕복 제거

설계: [20260807-445](../design/20260807-445-inline-cache-patch-on-guest-thread.md) ·
작업지시: [20260807-445](../work-orders/20260807-445-inline-cache-patch-on-guest-thread.md) ·
근거: 사용자 pumpit2 A/B 1쌍(2026-08-07 23:03~23:04, vsync OFF, Release)

## 1. 무엇을 바꿨나

간접 call/jump의 inline cache 미스는 코드 캐시에 **14바이트**를 쓰는 일인데, 그것을
워커 스레드에 부탁하고 `WaitForSingleObject(INFINITE)`로 기다리고 있었습니다. pumpit2
기준 **프레임당 약 350회**입니다.

Task 190이 워커를 둔 두 이유는 어느 쪽도 워커를 요구하지 않습니다.

* "게스트가 기다리므로 반쯤 패치된 슬롯을 실행하지 않는다" — 게스트가 **직접** 패치하면
  자동으로 성립합니다. 그때 게스트는 캐시를 실행하고 있지 않습니다.
* W^X — 두 스레드가 동시에 캐시를 만질 수 있을 때만 의미가 있는데, 그럴 수 없습니다.
  **세 개의 워커 요청 지점이 모두** `SetEvent` 직후 게스트를 `INFINITE`로 재웁니다.
  즉 **핸드셰이크 자체가 상호배제**였고, 패치를 게스트 스레드로 옮겨도 "한 번에 한
  스레드만 변경"은 그대로입니다.

호출하는 함수·placement·결과 구조체가 워커 쪽과 **완전히 동일**하므로
(`aot_runtime_dispatch.cpp:496` 대 `:757`), probe의 기존 `inline_cache_*` 레이아웃
검증이 새 경로를 그대로 덮습니다. `inline_cache_all=true`, probe exit 0.

## 2. 결과 — 처리량 +52%

| 지표 | OFF(워커) | ON(직접) | 변화 |
|---|---:|---:|---:|
| 실행 시간 | 69.9초 | 79.3초 | — |
| **swap** | 4,843 | 8,499 | — |
| **fps** | 69.3 | **107.2** | **+54.7%** |
| **swap ÷ Gcycle** | 18.698 | **28.937** | **+54.8%** |
| **primitive ÷ cycle** | — | — | **+51.1%** |
| `aot worker timing other` | **1,728,404** | **55** | 왕복 소멸 |
| patch direct/worker | 0 / 1,728,352 | 2,940,826 / 0 | 스위치 동작 |

**장면 편차가 아닙니다.** 두 실행의 프레임당 작업량이 거의 같습니다.

| 프레임당 | OFF | ON | 차이 |
|---|---:|---:|---:|
| primitive | 560.4 | 547.3 | −2.4% |
| **패치 횟수** | 356.9 | 346.0 | −3.1% |
| 평균 배치 | 32.42 | 32.12 | −0.9% |

패치 횟수가 프레임당 그대로라는 것이 핵심입니다 — **일의 양은 같고 단가만 내려갔습니다.**
이 조건에서는 fps 비교가 유효하며, 실제로 세 지표(fps·cycle당 swap·cycle당 primitive)가
+51~55%로 서로 일치합니다.

## 3. position census — 대기가 사라지고 실제 작업만 남았습니다

| 사이트 | OFF | ON |
|---|---:|---:|
| `RequestAotInlineCachePatch` (대기) | **30.4%** (1,253표본) | **0** |
| `PatchWin32AotIndirectInlineCache` (실작업) | 0 | **21.8%** (1,020표본) |
| `InvokeOnHostThread` | 50.0% | 48.0% |

표본을 **패치 1회당**으로 환산하면 7.25e-4 → 3.47e-4, 즉 **패치 단가 −52%**입니다.
남은 21.8%는 `VirtualProtect` 두 번과 `FlushInstructionCache`로, 왕복이 아니라 실제
작업입니다. 이것을 더 줄이려면 다른 축(패치 빈도 자체, frontier 5번)이 필요합니다.

`glide-gate ÷ guest-run`은 6.78% → 9.58%로 **올라갑니다.** Glide가 느려진 것이 아니라
경쟁하던 비-Glide 비용이 빠져서 분모 대비 비중이 커진 것입니다.

## 4. 정확성

| 검사 | OFF | ON |
|---|---|---|
| 종료 사유 | SDL exit | SDL exit |
| Glide 구현 공백 | 0/0/0/0/0/0 | 0/0/0/0/0/0 |
| setter `voided` | 0 | 0 |
| draw batch failures | 0 | 0 |
| 크래시 | 없음 | 없음 |

## 5. 남긴 것

* **기본값 켜짐으로 승격했습니다.** A/B는 각 1회지만 서로 독립적인 세 지표가 +51~55%로
  일치하고, 프레임당 작업량이 3% 안에서 같으며, 정확성 신호가 전부 0입니다. 반복 측정을
  더 쌓자는 제안에 대해 **사용자가 "1번 테스트에도 결과가 명확했다"고 판단**했습니다.
  `REPIU_AOT_INLINE_CACHE_PATCH_INLINE=0`이 워커 왕복 대조군으로 남습니다.
* 워커가 없는 구성에서는 기존과 동일하게 `false`를 반환합니다. 분기를 null 체크 **뒤에**
  두었습니다.
* A/B 도중 **무진행 감시견 결함**을 발견했습니다. `REPIU_EXECUTION_TIMEOUT_MS`를 걸면
  1초 감시견이 무장되는데, 그 "진행" 판정에 Glide 게이트 직접 디스패치가 빠져 있어
  건강한 실행을 6.27초에 죽였습니다. Task 445와 무관한 축이라 [TODO](../TODO.md)에
  기록만 하고 별도 태스크로 미뤘습니다.

---

# Task 445 Work Log — removing the inline cache patch round trip

## 1. The change

An indirect call/jump inline-cache miss writes **fourteen bytes** into the code cache, and
the guest was asking a worker thread to do it and then blocking on
`WaitForSingleObject(INFINITE)` — about **350 times per frame** on pumpit2.

Neither of Task 190's two reasons for the worker requires one. Its "the guest waits, so it
never executes a half-patched slot" holds automatically when the guest is the one patching,
because it is not executing the cache then. Its W^X rule matters only if two threads can
touch the cache at once, and none can: **all three** worker request sites signal and then
park the guest on an infinite wait, so the handshake *was* the mutual exclusion, and it
survives the move — still exactly one thread mutating at a time.

The guest-thread path calls the same function with the same placement and the same result
struct as the worker (`aot_runtime_dispatch.cpp:496` versus `:757`), so the probe's existing
`inline_cache_*` layout assertions already cover it: `inline_cache_all=true`, probe exit 0.

## 2. Result: throughput up 52%

One user A/B pair on pumpit2 with vsync off, Release, both runs ended by closing the window.
Frames rose from 69.3 to **107.2 per second**, **+54.7%**; swaps per billion guest cycles
rose **+54.8%** and primitives per cycle **+51.1%**. The worker's `other` operation count
fell from **1,728,404 to 55**, and the new summary line reports `0/1,728,352` against
`2,940,826/0`.

This is not a scene difference. Per frame the two runs did the same work: 560.4 versus 547.3
primitives, 356.9 versus 346.0 patches, a mean batch of 32.42 versus 32.12 — all within 3%.
That the patch count per frame is unchanged is the point: **the amount of work is the same
and only its unit price fell**, which is why all three independent throughput measures agree
at +51 to +55%.

## 3. The census: waiting gone, work remaining

`RequestAotInlineCachePatch` held **30.4%** of sited guest samples with the worker and
**disappears** without it; `PatchWin32AotIndirectInlineCache` appears in its place at
**21.8%**. Per patch that is 7.25e-4 samples against 3.47e-4 — **the unit cost halved**. What
remains is two `VirtualProtect` calls and a `FlushInstructionCache`: real work, not a round
trip, and reducing it further needs the separate axis of patching less often (frontier item
five).

`glide-gate ÷ guest-run` *rises* from 6.78% to 9.58%. Glide did not get slower; a competing
non-Glide cost left the denominator's company.

## 4. Correctness

Both runs ended on an SDL exit request with zero Glide implementation gaps, zero voided
setter entries, zero draw batch failures and no crash.

## 5. What is left

The default is **on**. This is one A/B pair, and the proposal was to defer promotion to a
task with repeated runs as Task 443 led to Task 444; the user judged one test conclusive, and
the evidence supports that -- three independent measures agree at +51 to +55%, per-frame work
matches within 3%, and every correctness signal is zero. `REPIU_AOT_INLINE_CACHE_PATCH_INLINE=0`
remains as the worker round-trip control. Configurations without a worker still return `false`,
because the branch sits after the null checks.

The A/B also surfaced an unrelated defect: setting `REPIU_EXECUTION_TIMEOUT_MS` arms a
one-second stall watchdog whose notion of progress omits the Glide gate direct dispatch path,
which killed a healthy run at 6.27 seconds. It is recorded in [TODO](../TODO.md) and deferred
to its own task.
