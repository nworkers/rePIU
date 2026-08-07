# Task 442 작업 로그 — setter 생략 batch 3

설계: [20260807-442](../design/20260807-442-glide-setter-elision-batch-three.md) ·
작업 지시: [20260807-442](../work-orders/20260807-442-glide-setter-elision-batch-three.md)

## 1. 한 줄 결과

`grTexSource` 호출당 비용 **−32.8%**, draw 배치 평균 **2.00 → 2.87**. 다만 Glide gate
전체 비중은 **18.77% → 18.59%(−0.18%p)** 로 작습니다.

## 2. Task 437의 제외 사유를 정정했습니다

437은 `grTexSource`의 `GrTexInfo*` 포인터 때문에 생략할 수 없다고 적었는데, **우리
게이트는 그 포인터를 읽지 않습니다**(`startAddress` 하나만 씁니다). 그 주소의 내용을
바꾸는 것은 다운로드뿐이고 다운로드는 `texture_generation`을 올리므로, 기존 키가 이미
그 변화를 담고 있습니다. **규격이 아니라 구현을 봤어야 했습니다.**

## 3. 실측 (Release, attract 20초, vsync OFF, census ON)

| 지표 | `=0` | `=1` |
|---|---:|---:|
| **`census same` = `elided`** | 257,326 = 257,326 | 332,172 = 332,172 |
| **`elided + applied` = 대상 호출** | 257,378 = 257,378 | 366,499 = 366,499 |
| `voided` · 구현 공백 | 0 · 0 | 0 · 0 |
| entries | 10 | 13 |
| **draw 배치 평균 / flush** | 2.00 / 24,876 | **2.87** / 18,839 |
| `grTexSource` 호출당 | 78,466 cycle | **52,754** (−32.8%) |
| `grTexSource` rendezvous | 42,022 | 29,079 (−30.8%) |
| **glide-gate ÷ guest-run** | 18.77% | **18.59%** |

**정확성은 가장 강한 형태로 확인됐습니다** — census가 잰 `same`과 캐시의 `elided`가
**오차 0으로 일치**합니다. 관측된 중복만 생략했다는 뜻입니다.

**배치가 길어진 것은 예측대로입니다.** 생략된 게이트는 flush 지점이 되지 않고,
`grTexSource`는 가장 잦은 flush 유발자였습니다. flush가 24% 줄었습니다.

## 4. 왜 gate 비중은 조금만 내려갔나

`grTexSource`가 애초에 guest-run의 **2.57%** 이고 그중 31.3%만 중복이므로 상한이
0.80%p입니다. 게다가 절감분 일부는 **더 커진 flush로 옮겨갑니다** — 같은 GL 작업이
더 적은 횟수에 몰릴 뿐입니다. Task 438에서 배운 "비용은 옮겨간다"가 여기서도 성립합니다.

## 5. 남은 것

1. **승격 판단은 gameplay A/B 뒤로.** 텍스처 바인딩이 잘못 생략되면 **다른 그림이
   그려지므로** 육안 확인이 결정적입니다. attract 단독 실행으로는 판정하지 않습니다.
2. **다음 축은 LFB입니다.** 같은 실행에서 `grLfbLock` **8.54%** + `grLfbUnlock`
   **2.25%** = **10.8%** 로, vsync OFF 기준 현재 최대 Glide 항목입니다. 값을 돌려주므로
   생략할 수 없고, `grLfbLock` 1회가 20.8M cycle(640×480 리드백)입니다. attract 전용
   비용이라 gameplay 영향은 별도 확인이 필요합니다.

---

# Task 442 Work Log — setter elision batch three

## 1. Result

`grTexSource` costs **32.8% less per call** and the draw batch rises from **2.00 to 2.87**, but
the Glide gate's share of guest-run moves only **18.77% to 18.59%**.

## 2. Task 437's exclusion was corrected

Task 437 ruled `grTexSource` out because of its `GrTexInfo*`, but **this gate never reads that
pointer** — it uses `startAddress` alone, and only a download changes what lives there, which
bumps the `texture_generation` the key already carries. The exclusion was reasoned from the
specification instead of from the implementation.

## 3. Measurement (Release, twenty seconds of attract, vsync off, census on)

Correctness holds in its strongest form: the census's `same` total equals the cache's `elided`
**exactly** in both configurations (257,326 and 332,172), `elided + applied` equals the covered
call count with no remainder, and `voided` and the implementation gaps are zero. The batch grows
as predicted — an elided gate is not a flush point and `grTexSource` was the most frequent
trigger — taking flushes down 24% and the mean batch from 2.00 to 2.87, while `grTexSource` falls
from 78,466 to 52,754 cycles per call and its rendezvous from 42,022 to 29,079.

## 4. Why the gate share barely moved

`grTexSource` is only **2.57% of guest-run** and just 31.3% of its calls repeat, capping this at
0.80 points; part of that saving then **relocates into the larger flushes**, since the same GL work
is simply issued in fewer, bigger pieces. It is the Task 438 lesson again.

## 5. Left open

Promotion waits on a **gameplay A/B with a visual check** — a wrongly elided texture binding draws
a different picture, so eyes decide this one, and attract alone cannot. **The next axis is the
LFB pair**: `grLfbLock` at 8.54% plus `grLfbUnlock` at 2.25% is **10.8% of guest-run** in the same
run, the largest Glide item under the correct measurement standard. It returns data and cannot be
elided, and one lock costs 20.8M cycles for a 640x480 readback. That cost is attract-heavy, so its
gameplay weight needs its own measurement.
