# Task 443 작업 지시 — batch 3 승격과 batch 4 추가

근거: [442 작업 로그](../work-logs/20260807-442-glide-setter-elision-batch-three.md)

## 1. batch 3 승격

사용자 gameplay 6회 A/B에서:

* **`census same` = `elided`가 6회 모두 오차 0**, `voided` 0, 구현 공백 0
* `grTexSource` 호출당 **−20.9%**(47,803 → 37,829), `grDepthMask` 왕복 **−86%**
* 시각 차이 **없음**(사용자 확인)

→ `ResolvePromotedToggle`로 바꿔 **기본값 켜짐**. `0`으로 끄는 대조군은 유지합니다.

## 2. batch 4 추가 — `grFogColorValue`·`grDitherMode`

같은 로그의 census가 **천장이 사실상 100%** 임을 보여 줍니다.

| gate | 프레임당 | 호출 | same | distinct | %guest-run |
|---|---:|---:|---:|---:|---:|
| `grFogColorValue` | 13.3 | 179,717 | 179,716 | **1** | **1.401%** |
| `grDitherMode` | 4.5 | 61,041 | 61,040 | **1** | **0.745%** |

**값이 평생 하나인데 프레임당 17.8회 host를 건드립니다.** 둘 다 이미 모델의 setter
집합에 속하므로 목록만 넓힙니다. opt-in으로 넣고 gameplay A/B 뒤 승격합니다.

**`grFogTable`은 제외합니다** — 인자가 64칸 테이블 포인터라 같은 포인터가 같은 내용을
증명하지 못합니다. 442가 `grTexSource`에 잘못 씌웠던 바로 그 위험이 여기서는 진짜입니다.

## 3. 검증

1. probe — batch 3·4 멤버십과 목록 분리, 모델 소속.
2. attract 스모크 — 회계(`same` = `elided`), 구현 공백 0.
3. (사용자) gameplay A/B — batch 4 승격 판단.

---

# Task 443 Work Order — promote batch three, add batch four

Six gameplay runs settled batch three: the census `same` total equalled the cache's `elided`
count **exactly in all six**, `voided` and the implementation gaps were zero, `grTexSource` cost
20.9% less per call, `grDepthMask` lost 86% of its round trips, and the user reports no visual
difference. It becomes the default through `ResolvePromotedToggle`, keeping the off path as the
control.

Batch four adds `grFogColorValue` and `grDitherMode`, whose ceiling the same logs put at
effectively 100%: each has **one distinct value** for the life of the process yet is called 13.3
and 4.5 times per frame, together 2.15% of guest-run. Both already belong to the shared setter
model, so only the list widens; opt-in until a gameplay A/B. **`grFogTable` stays out** — its
argument is a pointer to a 64-entry table, so an identical pointer proves nothing about the
contents, which is the hazard Task 442 disproved for `grTexSource` but which is real here.

Verification: the probe on membership and list disjointness, an attract smoke showing the
accounting closed with zero implementation gaps, and the user's gameplay A/B for promotion.
