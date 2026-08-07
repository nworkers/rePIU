# Task 442 작업 지시 — setter 생략 batch 3

설계: [20260807-442](../design/20260807-442-glide-setter-elision-batch-three.md)

## 1. 범위

`grTexSource`·`grConstantColorValue`·`grDepthMask`를 **opt-in 스위치 뒤에서** 생략
대상에 추가합니다. **기본값은 꺼짐**입니다.

**건드리지 않을 것:** 키 구성·`texture_generation`·무효화 규칙, batch 1·2 목록,
draw batch의 flush 규칙, 값 반환 게이트(LFB·질의).

## 2. 변경할 파일

| 파일 | 내용 |
|---|---|
| `glide_setter_state_cache.h`/`.cpp` | `GlideSetterBatchThreeElisionEnabled()`, `IsGlideSetterBatchThreeElisionGate()`, 스냅샷에 `batch_three` |
| `linexe_glide_boundary.cpp` | `elision_candidate_` 판정에 세 번째 목록 추가 |
| `main.cpp` | 생략 요약에 `batch-three` 표기 |
| `glide_setter_state_cache_probe.cpp` | 멤버십·분리·모델 소속 단정 |
| `README.md`·가이드 | 새 변수와 A/B 절차 |

## 3. 구현 규칙

* **목록만 넓힙니다. 규칙은 그대로입니다** — 그래야 census가 잰 천장이 계속 상한입니다.
* `grTexSource` 제외 사유가 바뀐 근거(우리가 포인터를 읽지 않는다는 사실)를 주석에
  남깁니다. 437의 반대 결론이 코드에 남아 있으면 다음 사람이 되돌립니다.
* 세 gate 모두 `IsGlideSetterStateGate`에 이미 속해야 하고, probe가 확인합니다.
* 스위치가 꺼져 있으면 지금과 완전히 같은 경로입니다.

## 4. 검증

1. Debug·Release 빌드 통과, probe 통과.
2. attract A/B(vsync OFF, census+time profile) — `elided` 증가분 = 세 gate의 `same` 합,
   `voided` 0, 구현 공백 0, `glide-gate ÷ guest-run` 감소, 배치 평균 증가.
3. 시각 회귀 없음.

## 5. 완료 기준

`=0`이 지금과 동일하고, `=1`에서 회계가 닫히며 gate 비중이 내려갑니다.

---

# Task 442 Work Order — setter elision batch three

Add `grTexSource`, `grConstantColorValue` and `grDepthMask` to the elision set behind
`REPIU_GLIDE_SETTER_ELIDE_BATCH3`, **off by default**. Keys, `texture_generation`, the
invalidation rules, batches one and two, the draw batch's flush rule and the value-returning gates
are untouched.

The cache header and source gain the policy, the third list and a `batch_three` snapshot field;
the boundary adds the list to its candidacy test; the loader prints the flag; the probe pins
membership, disjointness and model membership; and the README and guide document the variable.

**Widen the list, never the rules** — that is what keeps the census-measured ceiling an actual
bound. Record in the comment why `grTexSource` is now included: this backend never reads the
`GrTexInfo*`, so Task 437's opposite conclusion would otherwise be reverted by the next reader.

Verification: both builds and the probe; an attract A/B with vsync off and the census on, showing
the rise in `elided` equal to the three gates' `same` totals, zero `voided`, zero implementation
gaps, a lower gate share and a higher mean batch; and no visual regression.
