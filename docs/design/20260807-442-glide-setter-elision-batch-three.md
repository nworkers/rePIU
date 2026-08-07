# Task 442 설계 — setter 생략 batch 3 (`grTexSource`·`grConstantColorValue`·`grDepthMask`)

선행: [437 batch 2](20260807-437-glide-texture-setter-elision.md) ·
[438 draw batching](20260807-438-glide-draw-batching.md) ·
[440 정정 — 측정은 vsync OFF](20260807-440-glide-async-present.md)

## 1. Task 437의 `grTexSource` 제외 사유는 **틀렸습니다**

437은 이렇게 적었습니다.

> `grTexSource`의 네 번째 인자가 `GrTexInfo*` 포인터라, 같은 포인터 뒤의 구조체가
> 바뀌어도 키는 같아집니다. 따라서 생략할 수 없습니다.

**우리 구현은 그 포인터를 읽지 않습니다.** 게이트는 `startAddress` 하나만 꺼내
`SourceTexture(start_address)`를 부릅니다(`linexe_glide_boundary.cpp:1769-1773`).
backend가 하는 일은 그 주소로 저장된 텍스처를 찾아 bind하고 TMU 샘플러 4개를 다시
적용하는 것뿐입니다. 포인터 뒤 내용은 **관측 가능한 상태에 들어오지 않습니다.**

그러면 같은 `startAddress`가 다른 그림을 가리키게 만드는 것은 **다운로드뿐**이고,
다운로드는 `texture_generation`을 올립니다 — 키가 이미 그것을 담고 있습니다.

**즉 기존 기구로 안전하게 생략됩니다.** 437의 제외는 실제 구현이 아니라 Glide 규격만
보고 내린 판단이었습니다.

## 2. 근거 — vsync OFF 실측 (attract 20초, Release, census)

| ordinal | 호출 | %guest-run | 호출당 | **반복률** | 생략 가능분 |
|---|---:|---:|---:|---:|---:|
| `grTexSource` | 24,552 | 2.57% | 77,650 | **31.3%** | **0.80%p** |
| `grConstantColorValue` | 24,820 | 0.50% | 15,061 | **94.5%** | 0.48%p |
| `grDepthMask` | 31,127 | 0.39% | 9,398 | **56.7%** | 0.22%p |

합계 약 **1.5%p**입니다. `grTexSource`의 호출당 비용 77,650 cycle 중 **78%가 `wake`** —
GL 작업이 아니라 왕복입니다.

**측정 조건은 vsync OFF입니다**(Task 440 정정). vsync ON 수치는 디스플레이 대기가 섞여
축 선택을 왜곡합니다.

## 3. 부수 효과 — 배치가 길어집니다

생략된 게이트는 **flush 지점이 되지 않습니다**(생략 short-circuit이 Task 438의 flush
규칙보다 앞에 있습니다). 그리고 그것이 옳습니다 — 상태가 그대로이므로 앞뒤 draw는 같은
상태에 속합니다.

`grTexSource`는 bind마다 불리는 **가장 잦은 flush 유발자**이므로, 그중 31.3%가 사라지면
배치 평균이 함께 올라갑니다. attract 실측 기준 flush 24,820 → 약 17,100, 평균 배치
2.00 → 약 2.9입니다.

## 4. 변경

`IsGlideSetterElisionGate`에 세 gate를 추가합니다. 규칙·키·세대·무효화는 그대로입니다.

| 설정 | 동작 |
|---|---|
| 미설정 | batch 1 + 2 (지금과 동일) |
| `REPIU_GLIDE_SETTER_ELIDE_BATCH3=1` | + `grTexSource`·`grConstantColorValue`·`grDepthMask` |
| `REPIU_GLIDE_SETTER_ELIDE=0` | 전부 끔(상위 kill switch) |

## 5. 정확성 논거

| gate | 왜 안전한가 |
|---|---|
| `grTexSource` | 인자 중 우리가 쓰는 것은 `startAddress`뿐이고, 그 주소의 내용을 바꾸는 것은 다운로드뿐이며 다운로드는 세대를 올립니다. 캐시는 **직전 적용 키 하나**만 들고 있으므로 다른 텍스처를 거쳐 돌아오면 키가 달라져 반드시 재적용됩니다 |
| `grConstantColorValue` | 인자 하나가 상태 전부입니다. distinct 값이 실측 1~8개 |
| `grDepthMask` | 인자 하나(불리언). distinct 실측 **2** |

LFB blit는 자기 GL 상태를 격리하고 이전 바인딩을 복원하므로 텍스처 키를 무효화하지
않습니다.

## 6. 한계

| 항목 | 판단 |
|---|---|
| 천장이 31.3%뿐 | `grTexSource`는 대부분 **실제로 다른 텍스처**를 bind합니다. 남는 68.7%는 이 축으로 줄일 수 없습니다 |
| attract 기준 수치 | gameplay에서 `grTexSource`는 guest-run의 2.81%이므로 비슷한 비율이 기대되지만 장면마다 다릅니다 |
| **LFB가 더 큽니다** | 같은 실행에서 `grLfbLock` 8.54% + `grLfbUnlock` 2.25% = **10.8%**. 값을 돌려주므로 생략 불가이며 별도 축입니다(frontier에 기록) |

## 7. 검증

1. probe — 세 gate가 스위치에 따라만 포함되고, batch 1·2 목록은 불변, 모델의 setter
   집합에 속함.
2. attract A/B — `same` 합계와 `elided` 증가분 일치, `voided` 0, 구현 공백 0,
   `glide-gate ÷ guest-run` 감소, 배치 평균 증가.
3. 시각 — 텍스처가 뒤바뀌면 즉시 드러납니다(다른 그림이 그려짐).

---

# Task 442 Design — setter elision batch three

## 1. Task 437's exclusion of `grTexSource` was wrong

Task 437 excluded it because its fourth argument is a `GrTexInfo*` whose contents could change
behind an unchanged pointer. **Our implementation never reads that pointer**: the gate takes
`startAddress` alone and calls `SourceTexture(start_address)`, and the backend binds the texture
stored at that address and re-applies the four TMU sampler parameters. The struct behind the
pointer never enters observable state. The only thing that can change what lives at that address
is a **download**, which bumps `texture_generation` — already part of the key. The exclusion was
reasoned from the Glide specification rather than from what this backend does.

## 2. Evidence, measured with vsync off

Over twenty seconds of attract on Release with the census enabled: `grTexSource` runs 24,552 times
at **2.57% of guest-run** and 77,650 cycles per call — **78% of it `wake`**, a round trip rather
than GL work — and repeats identically **31.3%** of the time. `grConstantColorValue` repeats
**94.5%** of 24,820 calls, and `grDepthMask` **56.7%** of 31,127. Together the elidable share is
about **1.5 points of guest-run**. The condition is vsync off, per Task 440's correction.

## 3. A second effect: longer batches

An elided gate never reaches the flush rule, and that is correct — the state did not change, so
the draws on either side belong together. `grTexSource` is the most frequent flush trigger, once
per bind, so removing 31.3% of them lifts the mean batch as well: on the measured run, flushes
24,820 to about 17,100 and the mean batch 2.00 to about 2.9.

## 4. The change

Three gates join `IsGlideSetterElisionGate` behind `REPIU_GLIDE_SETTER_ELIDE_BATCH3`, off by
default. Keys, generation and invalidation rules are untouched, and `REPIU_GLIDE_SETTER_ELIDE=0`
still overrides everything.

## 5. Why each is safe

`grTexSource` is safe because only `startAddress` is used and only a download changes what is
there, which bumps the generation; the cache holds a single last-applied key per ordinal, so a
return trip through another texture always re-applies. `grConstantColorValue` and `grDepthMask`
are single-argument setters whose argument is the entire state, measured at one to eight and two
distinct values respectively. The LFB blit isolates its GL state and restores the previous
binding, so it does not invalidate a texture key.

## 6. Limits

The ceiling is only 31.3% for `grTexSource`: most binds really are different textures, and the
other 68.7% cannot be removed on this axis. The numbers come from attract; gameplay puts
`grTexSource` at 2.81% of guest-run, so a similar proportion is expected but scenes differ. And
**the LFB pair is larger** — `grLfbLock` at 8.54% plus `grLfbUnlock` at 2.25% is 10.8% of
guest-run in the same run. It returns data and cannot be elided, so it is a separate axis, now
recorded on the frontier.

## 7. Verification

The probe pins that the three gates are included only with the switch on, that batches one and two
are unchanged, and that all three belong to the shared setter-state set. An attract A/B must show
the `same` totals matching the rise in `elided`, zero `voided`, zero implementation gaps, a lower
`glide-gate` share and a higher mean batch. Visually a wrong texture binding is immediate — a
different picture is drawn.
