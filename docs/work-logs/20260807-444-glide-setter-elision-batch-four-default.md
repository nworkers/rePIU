# Task 444 작업 로그 — batch 4 기본값 승격

근거: [443 로그](20260807-443-glide-setter-elision-batch-four.md) ·
사용자 gameplay A/B 6회(2026-08-07 21:54~22:00)

## 1. 정확성 — 6/6

| 검사 | 결과 |
|---|---|
| **`census same` = `elided`** | 6회 모두 **오차 0** |
| `voided` · 구현 공백 | 6회 모두 0 |
| 시각 | **차이 없음**(사용자) |

## 2. `grDitherMode` — 확실한 신호

| 지표 | OFF | ON |
|---|---:|---:|
| **호출당** | 100,197 / 101,404 / 105,952 | **5,058 / 5,136 / 5,146 (−95%)** |
| glide-gate ÷ guest-run | 7.81 / 7.66 / 7.40% | **7.32 / 6.86 / 6.78%** |

**분포가 겹치지 않습니다**(ON 최대 7.32% < OFF 최소 7.40%). 게다가 **ON 실행이 더 무거운
장면**이었습니다 — draw/frame 438~575 대 308~436, gate/frame 669~849 대 477~671. 무거운
장면은 gate 비중을 **올리는** 방향이므로 실제 효과는 측정치 이상입니다.

다만 이 구간에서 dither는 프레임당 1.3~2.5회뿐이라 **자체 절감은 약 0.33%p**이고,
측정된 −0.80%p의 나머지는 장면 편차입니다. **fps는 비교하지 않았습니다**(ON이 더 무겁고
길었습니다).

## 3. 공백 — fog는 한 번도 실행되지 않았습니다

6회 모두 `grFogColorValue` 호출 **0**입니다. 플레이하신 구간에 fog 화면이 없었습니다
(지난 b3 로그에서는 프레임당 13.3회). **batch 4의 더 큰 절반이 A/B로는 미검증입니다.**

그럼에도 승격한 근거는 census입니다 — 179,717회 호출에 **서로 다른 값이 1개**.
값이 하나뿐이면 "다시 적용"과 "생략"이 만들 수 있는 결과가 같습니다. 네 batch 중
**가장 강한 천장**이고, batch 1이 승격될 때의 근거(Task 364의 반복률)보다 강합니다.

## 4. 검증

Release 빌드·probe 통과(`batch_three_membership`·`batch_four_membership`), **환경 변수를
전부 지운 스모크**에서 `enabled/texture-state/batch-three/batch-four = true/true/true/true`,
구현 공백 0.

## 5. 남은 것

`grFogTable`(0.531%)은 인자가 64칸 테이블 **포인터**라 그대로 남습니다. 내용을 키에
넣으려면 해시가 필요하고, 그것은 별도 작업입니다.

---

# Task 444 Work Log — batch four becomes the default

Six gameplay runs kept the census `same` total equal to the cache's `elided` count **exactly**,
with zero voided entries, zero implementation gaps and no visual difference. `grDitherMode` fell
from 100,197-105,952 to **5,058-5,146 cycles per call**, and the Glide gate share went from
7.81/7.66/7.40% to 7.32/6.86/6.78% — **non-overlapping distributions**, and achieved while the
enabled runs carried the *heavier* scenes (438-575 draws per frame against 308-436), which pushes
the share up rather than down. Dither is only 1.3-2.5 calls per frame in these sections, so its
own contribution is about **0.33 points**; the rest of the measured 0.80 is scene variance. Frames
were not compared, the enabled runs being both heavier and longer.

**The gap: fog never ran.** All six runs called `grFogColorValue` zero times, so the larger half
of batch four is unmeasured in an A/B. It is promoted anyway on the census: **one distinct value
across 179,717 calls**, which makes re-applying and skipping indistinguishable by construction —
the strongest ceiling of the four batches, and stronger than the evidence that promoted batch one.

Verified by the Release build and probe, and by a smoke with **every switch removed** showing all
four batches enabled and zero implementation gaps. `grFogTable` stays out: its argument is a
pointer to a 64-entry table and needs its contents hashed into the key, which is separate work.
