# Task 439 작업 지시 — draw batching과 텍스처 setter 생략을 기본값으로

근거: [438 작업 로그](../work-logs/20260807-438-glide-draw-batching.md) ·
[437 작업 로그](../work-logs/20260807-437-glide-texture-setter-elision.md)

## 1. 승격 근거 (사용자 gameplay A/B, Release, vsync off, time profile on)

| 지표 | `=0` | `=1` |
|---|---:|---:|
| **mean-batch** | — | **16.02** (최대 332) |
| flush | — | 167,133 — **전부 non-draw-gate** |
| **glide-gate ÷ guest-run** | **10.35%** | **8.40%** |
| 게이트 크로싱당 비용 | 7,334 cycle | **5,598** (−23.7%) |
| draw 1회당 총 게이트 비용 | 10,883 | **8,694** (−20.1%) |
| `failures` · `voided` · 구현 공백 | 0 | 0 |
| `queued` = `drawn` + `pending` | — | 2,677,700 = 2,677,700 + 0 |
| 시각 회귀 | — | **없음**(사용자 확인) |

Task 437 A/B에서는 텍스처 setter 셋이 **99.76% 중복**(385,197건 중 적용 933건),
`voided` 0, 시각 차이 없음이 확인됐습니다.

## 2. 범위

두 스위치의 **미지정 기본값만** 뒤집습니다. 해석 규칙·순서 계약·키·세대 규칙은
그대로입니다.

| 환경 변수 | 이전 | 이후 |
|---|---|---|
| `REPIU_GLIDE_SETTER_ELIDE_TEXTURE` | opt-in(꺼짐) | **켜짐**, `0`으로 opt-out |
| `REPIU_GLIDE_DRAW_BATCH` | opt-in(꺼짐) | **켜짐**, `0`으로 opt-out |

`REPIU_GLIDE_SETTER_ELIDE=0`은 여전히 상위 kill switch입니다.

## 3. 구현 규칙

* 해석은 `runtime::ResolvePromotedToggle`로 바꿉니다 — 미지정·빈 값은 ON,
  명시적 `0|off|false`는 OFF, 알 수 없는 값은 fail-closed OFF.
* **끄는 경로를 남깁니다.** 두 스위치 모두 회귀 대조군이며, Task 437·438 판정이
  가능했던 이유가 그 대조군입니다.
* probe의 정책 단정을 승격된 의미로 갱신합니다.

## 4. 검증

1. Release·Debug 빌드 통과.
2. `repiu_aot_probe`가 `glide_draw_batch_all=true`,
   `glide_setter_state_cache_all=true`로 통과.
3. **환경 변수를 지운** 스모크에서 배치 요약이 `enabled=true`이고 회계가 닫힘.
4. `REPIU_GLIDE_DRAW_BATCH=0` 스모크에서 이전 경로(삼각형당 왕복)가 복원됨.

## 5. 완료 기준

아무 설정 없이 로더를 켜면 두 최적화가 모두 적용되고, `0`으로 각각 되돌릴 수 있으며,
README·가이드·analysis의 기본값 서술이 코드와 일치합니다.

---

# Task 439 Work Order — make draw batching and texture-setter elision the defaults

## 1. Evidence

A paired gameplay A/B on Release with vsync off and the time profile on measured batches averaging
**16.02 primitives** (peak 332) with all 167,133 flushes coming from the non-draw-gate rule; the
Glide gate fell from **10.35% to 8.40% of guest-run**, per-crossing cost by **23.7%** and per-draw
gate cost by **20.1%**; failures, voided setters and implementation gaps were zero; `queued`
equalled `drawn` with nothing pending; and the user reports **no visual difference**. Task 437's
A/B had already shown the three texture setters to be **99.76% redundant** with zero voided
entries and no visual difference.

## 2. Scope

Flip **only the unset default** of the two switches. Parsing rules, the ordering contract, and the
key and generation rules are untouched. `REPIU_GLIDE_SETTER_ELIDE=0` remains the overriding kill
switch.

## 3. Implementation rules

Resolve both through `runtime::ResolvePromotedToggle`: unset and empty mean ON, an explicit
`0|off|false` opts out, and an unrecognised value stays fail-closed OFF. **Keep the off paths** —
they are the regression controls that made both verdicts possible. Update the probes' policy
assertions to the promoted meaning.

## 4. Verification

Release and Debug builds pass; `repiu_aot_probe` reports `glide_draw_batch_all=true` and
`glide_setter_state_cache_all=true`; a smoke **with the environment cleared** shows the batch
summary enabled with its accounting closed; and `REPIU_GLIDE_DRAW_BATCH=0` restores the
per-triangle path.

## 5. Done when

Launching with nothing set applies both optimisations, `0` restores either one, and the documented
defaults match the code.
