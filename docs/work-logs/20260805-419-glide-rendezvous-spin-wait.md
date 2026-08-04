# Task 419 작업 로그 — Glide rendezvous 스핀 대기 (**프레임 +27.7%, 채택**)

설계: [20260805-419](../design/20260805-419-glide-rendezvous-spin-wait.md) ·
작업 지시: [20260805-419](../work-orders/20260805-419-glide-rendezvous-spin-wait.md)

## 1. 한 줄 결과

**pumpit3 프레임이 2,399 → 3,063(중앙값 +27.7%)** 입니다. gate 시간에서 왕복 지연이
**65.5% → 5.3~12.9%**로 빠지고 그 자리를 진짜 GL 작업이 채웠습니다. pumpit1 회귀
없음(3,119 → 3,154). **기본값 20 µs로 채택합니다.**

## 2. 측정은 새로 하지 않아도 됐습니다

이 과제는 "계측을 켜는 것"으로 시작할 예정이었으나, **분해는 Task 418 로그에 이미
있었습니다.** `REPIU_EXECUTION_TIME_PROFILE=1`이 gate 집계 타이밍을 함께 켜기
때문이고, Task 418이 "꺼져 있다"고 읽은 `rendezvous/direct: 0/0`은 **ordinal별**
계측의 줄이었습니다. 그래서 이 과제는 계측이 아니라 **개선**으로 시작했습니다.

## 3. 변경

`glide_opengl_backend.cpp` / `.h`가 본체이고, 스냅샷 구조체와 로그 한 줄이 딸립니다.

| 파일 | 변경 |
|---|---|
| `glide_opengl_backend.h` | 두 플래그의 `std::atomic<bool>` 미러, 스핀 헬퍼, 카운터, 스냅샷 접근자 |
| `glide_opengl_backend.cpp` | 예산 해석(`REPIU_GLIDE_RENDEZVOUS_SPIN_US`, 기본 20), `_mm_pause` 스핀, **대기 세 곳**에 적용, 미러 갱신 |
| `glide_gate_timing.h` | `Win32GlideRendezvousSpinSnapshot` |
| `execution_trampoline.h` · `live_telemetry_snapshot.cpp` | attempt 레코드에 스냅샷 연결 |
| `main.cpp` | `glide gate spin budget-us/guest-hit/guest-miss/host-hit/host-miss` |

**정확성을 지킨 규칙 하나:** 원자 미러는 **힌트일 뿐**이고, 스핀이 성공해도 반드시
락을 잡아 **기존 조건변수 술어로 재확인**합니다. 술어를 원자 미러로 바꿨다면 lost
wakeup이 생겼을 것입니다. 이 규칙을 구현 주석에 고정했습니다.

## 4. A/B (같은 빌드·같은 세션·창 정상·EEPROM 실행별 격리·60초, 교대 실행)

| run | spin | 프레임 | traces | wake | work | complete | **wake+complete** | 호출당 cycle |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| off-2 | 0 | 2,399 | 8 | 34.57% | 34.15% | 30.82% | **65.4%** | 110,570 |
| off-3 | 0 | 2,229 | 8 | 34.36% | 33.84% | 31.29% | **65.7%** | 122,465 |
| off-4 | 0 | 2,442 | 8 | 34.93% | 33.27% | 31.35% | **66.3%** | 105,813 |
| on-1 | 20 | **3,036** | 10 | 4.00% | 93.41% | 1.95% | **6.0%** | 86,901 |
| on-2 | 20 | **3,052** | 10 | 9.35% | 85.96% | 3.59% | **12.9%** | 66,025 |
| on-3 | 20 | **3,084** | 10 | 6.16% | 90.33% | 2.68% | **8.8%** | 77,253 |
| on-4 | 20 | **3,075** | 10 | 3.55% | 94.08% | 1.78% | **5.3%** | 89,561 |

**중앙값 2,399 → 3,063.5 = +27.7%** (사전 등록 기준 +5%). 유효 7회 전부 격리 0.

**스핀은 거의 다 잡습니다.** 게스트측 hit 251~253만 대 miss 4,777~12,341(**99.5%+**),
호스트측 hit 119~120만 대 miss 71,008~82,753(**약 94%**). 20 µs 예산이 충분합니다.

## 5. 무엇이 좋아진 것인가 — **gate가 짧아진 게 아니라 일을 하게 됐습니다**

| 지표 | off | on |
|---|---:|---:|
| rendezvous 횟수 | 1,030,851 / 1,059,787 | **1,266,930 / 1,266,524** (+20%) |
| gate 총 cycles | 113.98G / 112.14G | 83.65G / 113.43G |
| 프레임당 호출 수 | 430 / 434 | **415 / 412** |

프레임당 호출 수가 사실상 같으므로 **프레임당 일의 양은 그대로**입니다. 같은 60초에
호출을 20% 더 처리했고 프레임은 27.7% 늘었습니다. 즉 **gate 시간이 줄어든 것이 아니라
대기가 작업으로 바뀐 것**입니다.

## 6. 사전 등록 판정 — 네 항목 전부 통과

| 종점 | 기준 | 결과 |
|---|---|---|
| **1차 프레임** | 중앙값 ≥ 2,620 | **3,063.5** — 통과 |
| 2차 구간 | `wake+complete` < 30% | **5.3~12.9%** — 통과 |
| 정확성 | ordinal 수·GL 에러·decline·`frame-errors` | **ordinal 39종 동일**, GL 에러 0, decline 0, `frame-errors=0` — 통과 |
| 회귀 | pumpit1이 대조 범위 | **3,119(off) → 3,154(on), +1.1%** — 통과 |

**pumpit1이 왜 거의 변하지 않는지도 예측대로입니다.** 그 타이틀은 gate 시간의
92.33%가 이미 work이고(`wake+complete` 7.6%), 스핀 후 1.15%가 되지만 **원래 작던
몫**이라 프레임에 영향이 없습니다. Task 418이 "이건 pumpit3 고유 문제"라고 한 근거가
그대로 확인됩니다.

## 7. 한계와 주의

* **장면이 같지 않습니다.** `on` 실행은 DOS path trace가 8 → 10으로, 같은 60초에 **더
  진행**합니다. 프레임 종점은 영향받지 않지만, 위 share 수치는 서로 **조금 다른
  장면**을 재고 있습니다. 인과 방향은 "빨라져서 더 갔다"입니다.
* **CPU를 더 씁니다.** 스핀은 지연을 cycle로 갚는 거래이므로, 코어가 부족한 환경에서는
  `REPIU_GLIDE_RENDEZVOUS_SPIN_US=0`이 정답일 수 있습니다. 그래서 스위치를 남겼습니다.
* **부팅 크래시가 11회 중 2회**(off-1, p1-on) 났습니다. frontier 항목 3이며 이 변경과
  무관합니다(재배치 후보 전부 occupied). 두 표본을 버리고 보충했습니다.
* 20 µs가 최적값이라는 근거는 없습니다. hit율이 이미 94~99.5%이므로 **예산을 더 늘릴
  이유는 적고**, 줄이는 쪽(예: 10 µs)은 미측정입니다.

## 8. 회고

* **Task 365·368과 결과가 갈린 이유가 분명합니다.** 그때 줄인 것은 **작업**이었고
  이번에 없앤 것은 **막힘**입니다. 게스트 스레드가 실제로 대기 중(CPU share 50~54%)
  이었으므로 임계 경로가 직접 짧아졌습니다. "비용 감소가 프레임이 되지 않는다"는
  이 저장소의 경험칙은 **비용의 종류에 달려 있었습니다.**
* **측정을 새로 하기 전에 기존 로그를 다시 읽는 것이 하루를 아꼈습니다.** 켜야 한다고
  적어 둔 계측이 이미 켜져 있었습니다.
* **A/B 스위치를 남기는 관행이 또 값을 했습니다.** 같은 바이너리에서 교대 실행으로
  세션 편차와 효과를 분리했습니다.

---

# Task 419 Work Log — spin-then-wait for the Glide rendezvous (**+27.7% frames, adopted**)

## 1. Result in one line

**pumpit3 frames go from 2,399 to 3,063 at the median, +27.7%**, as round-trip latency falls
from **65.5% of gate time to 5.3-12.9%** and real GL work takes its place. No pumpit1
regression (3,119 to 3,154). **Adopted at the 20 µs default.**

## 2. No new measurement was needed

This task was planned to begin by switching instrumentation on, but **the decomposition was
already in Task 418's logs**: `REPIU_EXECUTION_TIME_PROFILE=1` enables the aggregate gate
timing too, and the `rendezvous/direct: 0/0` line Task 418 read as "off" belongs to the
per-ordinal profile. So the task began as an improvement rather than a measurement.

## 3. Change

`glide_opengl_backend.cpp` and its header carry the work — atomic mirrors of the two flags, a
`_mm_pause` spin helper with a budget from `REPIU_GLIDE_RENDEZVOUS_SPIN_US` (default 20, zero
restoring the old behaviour), applied at the three waits — with a snapshot struct in
`glide_gate_timing.h`, a field on the attempt record, and one log line in `main.cpp`. **The
rule that keeps it correct**: the atomic mirrors are **hints only**, and every successful spin
still takes the mutex and re-tests the original condition-variable predicate. Replacing the
predicates with the mirrors is what would have produced lost wakeups, and the implementation
comment pins that.

## 4. A/B, alternating, one build and one session, normal window, EEPROM isolated per run

Spin off: 2,229 / 2,399 / 2,442 frames with `wake + complete` at 65.4-66.3%. Spin on: 3,036 /
3,052 / 3,075 / 3,084 frames with `wake + complete` at **5.3-12.9%**. **Medians 2,399 against
3,063.5, +27.7%** against a pre-registered +5%, with zero quarantines in all seven valid runs.
The spin catches nearly everything: guest side 2.51-2.53 M hits against 4,777-12,341 misses
(**over 99.5%**), host side 1.19-1.20 M against 71,008-82,753 (**about 94%**).

## 5. What improved — the gate did not shrink, it started working

Rendezvous counts rise from 1.03-1.06 M to **1.267 M (+20%)** while calls per frame stay
essentially constant (430-434 against 412-415), so the work per frame is unchanged: the same
sixty seconds carried 20% more calls and produced 27.7% more frames. **Waiting turned into
working** rather than gate time falling.

## 6. Pre-registered readings — all four pass

Frames reach 3,063.5 against a 2,620 threshold; `wake + complete` lands at 5.3-12.9% against a
30% threshold; correctness holds with **39 Glide ordinals in both conditions**, zero GL errors,
zero gate declines and `frame-errors=0`; and pumpit1 goes 3,119 to 3,154 (+1.1%). **pumpit1
barely moves for the predicted reason**: 92.33% of its gate time was already work, so its
`wake + complete` of 7.6% falling to 1.15% has little to give — exactly the title-specific
split Task 418 identified.

## 7. Limits

**The scenes are not identical**: `on` runs reach ten DOS path traces against eight, so they
progress further in the same sixty seconds, and the share figures above describe slightly
different scenes — the causal direction being that speed produced the progress. **It spends
CPU**, trading latency for cycles, so a core-poor machine may want
`REPIU_GLIDE_RENDEZVOUS_SPIN_US=0`, which is why the switch stays. **Two of eleven runs hit the
boot crash** (frontier item 3), unrelated to this change and replaced by extra samples. And
**20 µs is not shown to be optimal** — with hit rates already at 94-99.5% there is little
reason to raise it, while lowering it is unmeasured.

## 8. Retrospective

The split from Tasks 365 and 368 is now explicable: those reduced **work**, this removed
**blocking**. The guest thread was genuinely waiting at 50-54% CPU share, so the critical path
shortened directly — the repository's rule of thumb that cost reduction does not become frames
turns out to depend on **which kind of cost**. Re-reading existing logs before measuring again
saved a day, since the instrumentation this task planned to enable was already on. And keeping
an A/B switch paid once more, separating the effect from session drift inside one binary.
