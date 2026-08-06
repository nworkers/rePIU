# Task 439 작업 로그 — draw batching과 텍스처 setter 생략을 기본값으로

작업 지시: [20260807-439](../work-orders/20260807-439-glide-batching-default.md) ·
근거: [438 로그](20260807-438-glide-draw-batching.md) · [437 로그](20260807-437-glide-texture-setter-elision.md)

## 1. 한 줄 결과

사용자 짝 실행이 **glide-gate 비중 10.35% → 8.40%**, 배치 평균 **16.02**, 실패·시각
회귀 0을 기록해 두 스위치를 기본값으로 올렸습니다.

## 2. 승격 근거

| 지표 | `=0` | `=1` |
|---|---:|---:|
| mean-batch (프리미티브/flush) | — | **16.02** (최대 **332**) |
| flush | — | 167,133 — **전부** non-draw-gate |
| **glide-gate ÷ guest-run** | **10.35%** | **8.40%** |
| 게이트 크로싱당 | 7,334 cycle | **5,598** (−23.7%) |
| draw 1회당 총 게이트 | 10,883 cycle | **8,694** (−20.1%) |
| `failures`·`voided`·구현 공백 | 0 | 0 |
| `queued` = `drawn` + `pending` | — | 2,677,700 = 2,677,700 + 0 |
| 시각 | — | **차이 없음**(사용자) |

Task 437분은 텍스처 setter 셋이 **99.76% 중복**(385,197건 중 적용 933건), `voided` 0,
시각 차이 없음입니다.

**프레임은 근거로 쓰지 않았습니다.** 두 실행의 구간이 다르고(draw/frame 474 대 368),
같은 구성으로 돌린 두 실행이 13% 차이 난 전례가 있어 2~3% 효과를 분해할 수 없습니다.
판정은 구간 길이에 둔감한 **cycle 비중**으로 했습니다.

## 3. 변경

| 파일 | 내용 |
|---|---|
| `glide_draw_batch.cpp` · `glide_setter_state_cache.cpp` | 해석을 `ResolveOptInToggle` → **`ResolvePromotedToggle`** |
| 두 헤더 | 승격 사실과 근거 수치를 주석에 기록 |
| `glide_draw_batch_probe.cpp` | 정책 단정을 승격 의미로 갱신(미지정·빈 값 ON, `0|off|false` OFF, 미인식 fail-closed OFF) |
| README · 가이드 5·6단계 · analysis · frontier | 기본값 서술과 결과 반영 |

**끄는 경로는 둘 다 남겼습니다.** `REPIU_GLIDE_DRAW_BATCH=0`,
`REPIU_GLIDE_SETTER_ELIDE_TEXTURE=0`, 그리고 상위 kill switch
`REPIU_GLIDE_SETTER_ELIDE=0`. 이번 두 판정이 가능했던 이유가 그 대조군입니다.

## 4. 검증 (2026-08-07)

| 검증 | 결과 |
|---|---|
| Release 빌드 | **exit 0** |
| `repiu_aot_probe.exe MASTER\PIU_1ST\PIU\PIU.EXE` (Release) | **exit 0** — `glide_draw_batch_policy/all=true`, `glide_setter_state_cache_policy/all=true` |
| **환경 변수 전부 제거** 스모크 | `setter elision ... texture-state: **true**`, `draw batch enabled: **true**`, queued 3,662 = drawn 3,662 = `_GRDRAWTRIANGLE` 3,662, pending 0, failures 0, 구현 공백 0 |
| `REPIU_GLIDE_DRAW_BATCH=0` 스모크 | `draw batch enabled: **false**`, 같은 draw 3,662, 구현 공백 0 — 이전 경로 복원 |

두 스위치가 **독립적으로** 동작하는 것도 확인했습니다 — batch를 꺼도 텍스처 생략은
`texture-state: true`로 남습니다.

## 5. 이번 축에서 배운 것

* **예측은 5.44, 실측은 16.02였습니다.** 제 추정은 "모든 flush 지점에 그릴 것이 있다"는
  가정이었고, 실제로는 draw가 최대 332개까지 연속으로 뭉쳐 flush 지점 대부분이 빈 큐를
  만납니다. 상한 계산이 하한처럼 쓰인 셈입니다.
* **장면이 축을 결정합니다.** 같은 코드가 attract에서는 배치 2로 이득 0, gameplay에서는
  배치 16으로 −1.95%p입니다. attract 스모크만 봤다면 이 축을 잘못 닫았을 것입니다.
* **프레임으로 측정하지 마십시오.** 같은 구성 두 실행이 13% 차이 났습니다. 2~3% 효과는
  `glide-gate ÷ guest-run`처럼 정규화된 지표로만 보입니다.
* **비용은 사라지기도 하고 옮겨가기도 합니다.** draw ordinal에서 빠진 4,730 cycle 중
  2,541은 flush ordinal로 이동했습니다. ordinal 하나만 보면 2.66배 개선으로 과대평가됩니다.

---

# Task 439 Work Log — promoting draw batching and texture-setter elision

## 1. Result in one line

The user's paired run measured the **Glide gate falling from 10.35% to 8.40% of guest-run** with
batches averaging **16.02** primitives and zero failures or visual differences, so both switches
are now defaults.

## 2. The evidence

Batches averaged 16.02 primitives with a peak of 332; all 167,133 flushes came from the
non-draw-gate rule; per-crossing gate cost fell **23.7%** and per-draw gate cost **20.1%**;
`failures`, `voided` and implementation gaps were zero; `queued` equalled `drawn` with nothing
pending; and the user reports no visual difference. Task 437's share of this is the three texture
setters measured **99.76% redundant** — 933 applications out of 385,197 newly covered calls.

**Frames were not used as evidence.** The two runs covered different sections (474 against 368
draws per frame) and two runs of one configuration have differed by 13% before, which cannot
resolve a 2-3% effect; the verdict rests on the section-insensitive cycle share.

## 3. The change

Both switches resolve through `ResolvePromotedToggle` instead of `ResolveOptInToggle`, so unset
means on and an explicit `0|off|false` opts out, with unrecognised values staying fail-closed off.
The headers record the promotion and its numbers, the batch probe's policy assertion is updated,
and the README, the guide's steps five and six, the Glide analysis topic and the frontier all
carry the result. **Both off paths remain**, together with the overriding
`REPIU_GLIDE_SETTER_ELIDE=0` kill switch — they are why these verdicts were possible.

## 4. Verification

The Release build exits 0 and its probe reports both policy and full assertions true. **With every
switch removed from the environment**, the smoke shows `texture-state: true` and
`draw batch enabled: true` with queued, drawn and `_GRDRAWTRIANGLE` all at 3,662, nothing pending,
zero failures and zero implementation gaps; `REPIU_GLIDE_DRAW_BATCH=0` restores the per-triangle
path with the same draw count. The two switches are also confirmed independent — turning batching
off leaves the texture elision on.

## 5. What this axis taught

**The prediction was 5.44 and the measurement was 16.02**, because the estimate assumed every
flush point had work pending while draws actually cluster up to 332 deep, leaving most flush
points with an empty queue — an upper bound used as if it were a lower one. **The scene decides
the axis:** the same code gives a batch of two and no gain in attract, and a batch of sixteen with
−1.95 points in gameplay; judging from the attract smoke alone would have closed this axis
wrongly. **Do not measure in frames** — two runs of one configuration differed by 13%, so a 2-3%
effect is only visible in a normalised ratio. And **cost both vanishes and moves**: of the 4,730
cycles per draw that left the draw ordinal, 2,541 reappeared at the flush sites, so reading that
one ordinal alone overstates the win as 2.66x.
