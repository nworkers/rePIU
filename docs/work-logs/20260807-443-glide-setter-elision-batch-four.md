# Task 443 작업 로그 — batch 3 승격, batch 4 추가

작업 지시: [20260807-443](../work-orders/20260807-443-glide-setter-elision-batch-four.md) ·
근거: [442 로그](20260807-442-glide-setter-elision-batch-three.md)

## 1. batch 3 승격 — 사용자 gameplay 6회

| 검사 | 결과 |
|---|---|
| **`census same` = `elided`** | **6/6 오차 0** |
| `voided` · 구현 공백 | 6/6 모두 0 |
| `grTexSource` 호출당 | 47,803 → **37,829 (−20.9%)**, 분포 안 겹침 |
| `grDepthMask` 왕복 | 148,329 → 17,345~27,111 (**−86%**), 호출당 −60% |
| glide-gate ÷ guest-run | 9.55% → 9.09% (**편차 안**, 판정 근거 아님) |
| 시각 | **차이 없음**(사용자) |

**gameplay는 attract와 반복률이 달랐습니다.** `grTexSource`는 13~16%(attract 31.3%)로
절반이고 — 실제로 매번 다른 텍스처를 bind합니다 — 대신 `grDepthMask`가 **86~90%**
(attract 56.7%)로 뒤집혀 이번 batch의 실주력이 됐습니다. Task 437이 "변동이 잦다"며
뺐던 게이트입니다.

## 2. `grDepthMask`는 더 팔 것이 없습니다

사용자가 다음 축으로 지목했으나, 이번 batch가 이미 해결했습니다: `distinct=2`(on/off),
`max_frame_changes=6`. 프레임당 14.8~21회 부르는데 **실제로 바뀌는 건 2회**이고 나머지
86~90%를 생략이 걷어냈습니다. 잔량은 진짜 토글이므로 게스트 동작입니다.

## 3. batch 4 — 값이 평생 하나인 setter 둘

같은 로그의 census가 천장을 **사실상 100%** 로 보여 줍니다.

| gate | 프레임당 | 호출 | same | distinct | %guest-run |
|---|---:|---:|---:|---:|---:|
| `grFogColorValue` | 13.3 | 179,717 | 179,716 | **1** | **1.401%** |
| `grDitherMode` | 4.5 | 61,041 | 61,040 | **1** | **0.745%** |

**`grFogTable`은 제외했습니다** — 인자가 64칸 테이블 포인터라 같은 포인터가 같은 내용을
증명하지 못합니다. 442가 `grTexSource`에 잘못 씌웠던 위험이 여기서는 진짜입니다.

## 4. 검증

| 검증 | 결과 |
|---|---|
| Release 빌드 · probe | **통과**(`batch_three_membership`·`batch_four_membership` 포함) |
| attract 스모크 `=1` | `grDitherMode` 왕복 **826 → 1**, 호출당 67,536 → 10,058, `same 598 = elided 598`, 구현 공백 0 |

**attract는 fog를 쓰지 않으므로 batch 4의 실효는 이 스모크로 판정할 수 없습니다.**
근거는 §3의 gameplay census이고, 승격은 사용자 A/B 뒤로 미룹니다.

## 5. 다음 축

같은 gameplay 로그의 잔여 순위(batch 3 적용 후, guest-run 대비):

| ordinal | %run | 비고 |
|---|---:|---|
| `grTexSource` | 2.822 | 반복 13~16%뿐 — 남은 68%는 진짜 bind |
| `grBufferSwap` | 1.423 | vsync OFF 기준. Task 440에서 닫힘 |
| **`grFogColorValue`** | **1.401** | batch 4 |
| `grDrawTriangle` | 0.886 | 배치 적용됨 |
| **`grDitherMode`** | **0.745** | batch 4 |
| **`grFogTable`** | **0.531** | 포인터 인자 — 내용 해시가 필요 |

**LFB는 gameplay에 없습니다**(attract 전용). 442에서 "다음 축은 LFB"라고 적었던 것을
정정합니다 — 그 10.8%는 attract 한정입니다.

---

# Task 443 Work Log — promote batch three, add batch four

## 1. Batch three, promoted on six gameplay runs

The census `same` total equalled the cache's `elided` count **exactly in all six runs**, with
`voided` and the implementation gaps at zero, `grTexSource` down **20.9% per call** on
non-overlapping distributions, `grDepthMask` losing **86%** of its round trips, and no visual
difference. The gate share moved 9.55% to 9.09%, inside run variance, so it is not the basis.

**Gameplay repeats differently from attract**: `grTexSource` repeats only 13-16% against attract's
31.3% — it really does bind a different texture most times — while `grDepthMask` inverts to
**86-90%** from 56.7% and carries this batch. It is the gate Task 437 set aside as "changing often
enough to deserve its own measurement".

## 2. `grDepthMask` has nothing left

Named as the next axis, but this batch already settled it: two distinct values, at most six real
changes per frame, 14.8-21 calls per frame of which the elision now removes 86-90%. What remains
is genuine toggling, which is guest behaviour.

## 3. Batch four: two setters whose value never changes

The same logs put the ceiling at effectively 100% — `grFogColorValue` at 179,716 repeats of
179,717 calls with **one** distinct value, 13.3 per frame and 1.401% of guest-run, and
`grDitherMode` at 61,040 of 61,041 with one distinct value, 4.5 per frame and 0.745%.
**`grFogTable` stays out**: a pointer to a 64-entry table proves nothing about its contents, which
is the hazard Task 442 disproved for `grTexSource` but which is real here.

## 4. Verification

Release build and probe pass with both membership assertions. An attract smoke with batch four on
takes `grDitherMode` from 826 round trips to **one**, 67,536 to 10,058 cycles per call, with
`same` equal to `elided` and zero implementation gaps. **Attract does not use fog**, so it cannot
judge batch four's real effect; the evidence is the gameplay census above and promotion waits on
the user's A/B.

## 5. Next

After batch three, the remaining gameplay ranking against guest-run is `grTexSource` 2.822%
(only 13-16% repeatable), `grBufferSwap` 1.423% (closed in Task 440), `grFogColorValue` 1.401%
and `grDitherMode` 0.745% (batch four), `grDrawTriangle` 0.886% (already batched), and
`grFogTable` 0.531%, which needs its contents in the key. **The LFB pair does not appear in
gameplay at all** — correcting Task 442's note, its 10.8% is attract-only.
