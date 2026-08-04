# Task 415 작업 로그 — 세대 실패 벌칙을 주소로 좁힘 (**적용, 멈춤은 별개 원인**)

설계: [20260804-415](../design/20260804-415-generation-failure-address-scope.md)

## 1. 한 줄 결과

벌칙은 페이지에서 주소로 좁아졌고(격리 **1 → 0**, 프레임 회귀 없음), **남은 멈춤은
사라지지 않았습니다.** 멈춘 실행은 **격리가 0인데도 single-step이 2,494,772회**였고,
이는 격리와 무관한 **제3의 기전**을 가리킵니다.

## 2. 변경

`src/platform/win32/aot/aot_runtime_dispatch.cpp`와 새 헤더
`aot_generation_failure_policy.h`(ThreadContext 의존 없음 — 호스트가 counter를 읽기
위해)입니다.

* 세대 실패 시 **주소를 기억하고 페이지는 격리하지 않습니다.**
* retired target 번역 시도 **전에** 그 집합을 조회해 이미 실패한 주소는 **시도조차
  하지 않습니다**(격리가 제공하던 재시도 폭주 방지를 그대로 유지).
* 집합이 256을 넘거나 `REPIU_AOT_QUARANTINE_ON_GENERATION_FAILURE=1`이면 예전 동작.

`thread_context.h`·`execution_trampoline.h`를 건드리지 않아 증분 빌드로 끝났습니다.

## 3. A/B (같은 세션 · 60초 · EEPROM 실행별 격리)

| 조건 | frames | quarantine | single-step | 정책(주소/스킵/격리대체) |
|---|---:|---:|---:|---|
| page(예전) | 1,497 | 1 | 15,593 | 0/0/1 |
| address | 1,431 | 0 | 13,907 | 1/9/0 |
| page | 1,418 | 1 | 16,149 | 0/0/1 |
| address | 1,456 | 0 | 14,491 | 1/5/0 |
| page | 1,437 | 1 | 16,665 | 0/0/1 |
| address | 1,464 | 0 | 14,673 | 1/7/0 |
| address | **0** | 0 | **2,494,772** | 1/1/0 |
| address | 1,423 | 0 | 13,704 | 1/8/0 |

**기계는 의도대로 동작합니다** — 격리 0, 실패 주소 1개, 스킵 5~9회. 정상 실행의
single-step은 오히려 조금 낮습니다(13.7~14.7k 대 15.6~16.7k). 프레임 중앙값은
page 1,437 대 address 1,443으로 **회귀 없음**입니다.

## 4. 사전 등록 기준 대조 — 절반만 통과

* 통과: 격리 0, 재시도 없음, single-step 정상 수준, 프레임 회귀 없음.
* **불통과: 멈춤 해소.** address 5회 중 1회가 멈췄습니다(page는 3회 중 0회지만
  표본이 작아 우열을 주장하지 않습니다).

**따라서 Task 415는 유지하되, 남은 멈춤의 원인은 아니었습니다.** 페이지 격리가
멈춤을 만든다는 가설은 **반증**됐습니다.

## 5. 새 사실 — 남은 멈춤은 **single-step trace가 꺼지지 않는 것**입니다

멈춘 실행(run-07)의 지표입니다.

| 지표 | 값 | 정상 실행 |
|---|---:|---:|
| single-step 예외 | **2,494,772** | 13,704~16,665 |
| 재진입 funnel `not-pending` | **584,473** | (정상은 success가 지배) |
| 재진입 funnel `success` | 114,837 | — |
| `quarantined` 거부 | **0** | 0 |
| `last_eip` 분포 | `0x0301DFxx`·`0x030D5874`·`0x030D39xx`·`0x030CFxxx` **여러 곳** | — |

**한 페이지에 갇힌 것이 아닙니다.** 게스트가 여러 함수에 걸쳐 계속 single-step되고
있고, 재진입은 `aot_reentry_pending`이 서 있지 않아 584,473회 거부됐습니다. 즉
**trace 모드가 켜진 채 유지되고 캐시로 돌아갈 예약이 서지 않는 상태**입니다. 격리도
port I/O도 아닌 **제3의 기전**이며, frontier에 새 항목으로 올립니다.

## 6. 회고

* **가설 두 개 중 하나만 맞았습니다.** "페이지 격리가 과하다"는 맞았고(고쳤고),
  "그래서 멈춘다"는 틀렸습니다. A/B를 스위치로 한 바이너리에 넣어 둔 덕분에 그
  구분이 한 번의 측정으로 났습니다.
* **작은 헤더를 새로 만든 것이 40분을 아꼈습니다.** counter를 `aot_runtime_dispatch.h`에
  두면 `thread_context.h` 사슬을 타고 전체 재빌드가 됩니다. ThreadContext에 의존하지
  않는 헤더로 분리해 증분 빌드로 끝냈습니다.

---

# Task 415 Work Log — narrowing the generation-failure penalty (**kept; the stall is a different cause**)

## 1. Result in one line

The penalty narrowed from a page to an address (quarantines **1 → 0**, no frame
regression), and **the remaining stall did not go away**: the stalled run had **zero
quarantines yet 2,494,772 single steps**, which points at a **third mechanism**.

## 2. Change

`aot_runtime_dispatch.cpp` plus a new `aot_generation_failure_policy.h` that carries no
ThreadContext dependency so the host can read its counters. A generation failure now records
the **address** and does not quarantine the page; a retired target whose address is already
in that set **skips the translation attempt entirely**, preserving the no-retry-storm
property quarantine provided; and the old behaviour returns past 256 addresses or under
`REPIU_AOT_QUARANTINE_ON_GENERATION_FAILURE=1`. Neither `thread_context.h` nor
`execution_trampoline.h` was touched, so this built incrementally.

## 3. A/B (one session, 60 s, EEPROM isolated per run)

Page (old): 1,497 / 1,418 / 1,437 frames, one quarantine each, 15,593-16,665 single steps.
Address (new): 1,431 / 1,456 / 1,464 / **0** / 1,423 frames, zero quarantines, one failed
address with five to nine skips, and 13,704-14,673 single steps in the healthy runs.
**The mechanism does what it was designed to do**, healthy-run single steps are slightly
lower, and the frame medians (1,437 against 1,443) show no regression.

## 4. Against the pre-registered reading — half passed

Zero quarantines, no retries, healthy single-step levels, and no frame regression all hold.
**The stall did not go away**: one of five address runs stalled, against zero of three page
runs, on samples too small to rank the two. **Task 415 stays, but it was not the cause**, and
the hypothesis that page quarantine produces the stall is **refuted**.

## 5. New fact — the remaining stall is a single-step trace that never turns off

The stalled run shows **2,494,772** single-step exceptions against 13,704-16,665 in healthy
runs, a re-entry funnel of **584,473 `not-pending`** against 114,837 successes, **zero**
quarantine rejections, and `last_eip` scattered across `0x0301DFxx`, `0x030D5874`,
`0x030D39xx`, and `0x030CFxxx`. So the guest is **not confined to one page**: it is being
stepped across many functions while nothing schedules a return to the cache. That is neither
quarantine nor port I/O but a **third mechanism**, now filed on the frontier.

## 6. Retrospective

Two hypotheses, one right: "the page penalty is too broad" held and is fixed; "and that is
why it stalls" did not. Keeping the A/B behind a switch in one binary is what separated them
in a single measurement. And splitting the counters into a small ThreadContext-free header
saved forty minutes — putting them in `aot_runtime_dispatch.h` would have pulled the
`thread_context.h` chain and forced a full rebuild.
