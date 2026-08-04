# Task 417 작업 로그 — 걸친 요청 항목 활성화 (**마지막 멈춤 해소**)

설계: [20260804-417](../design/20260804-417-spanning-entry-activation.md) ·
작업 지시: [20260804-417](../work-orders/20260804-417-spanning-entry-activation.md)

## 1. 한 줄 결과

**멈춤이 없어졌습니다.** 같은 세션 A/B에서 strict(예전 규칙)는 **5회 중 2회 멈춤**,
relaxed(신규)는 **8회 중 8회 정상**이며, relaxed 실행은 세대 실패가 **아예 발생하지
않습니다**(`generation failure addresses` = 0).

## 2. 변경

`aot_code_cache_win32.cpp` 한 곳입니다. append 루프에서 **요청 항목**
(`entry.guest_address == guest_entry`)이 기존 규칙에 거부되면, **quarantined 페이지를
걸치지 않는 한** 활성으로 둡니다. 나머지 항목은 규칙 그대로입니다.

정확성 근거는 설계 §2 그대로입니다 — 이미지는 방금 현재 바이트로 번역됐고,
`RegisterAddressMapPages`가 **걸친 모든 페이지에 등록**하므로 이후 어느 페이지에 써도
이 항목이 retire됩니다.

## 3. A/B (같은 세션 · 60초 · EEPROM 실행별 격리)

| 조건 | frames | single-step | 실패주소/스킵/격리/걸친활성화 |
|---|---:|---:|---|
| strict | 1,388 | 12,903 | 1/18/0/0 |
| relaxed | 1,223 | 8,932 | **0**/0/0/**2** |
| strict | 1,201 | 8,397 | 1/12/0/0 |
| relaxed | 1,209 | 8,577 | **0**/0/0/**2** |
| strict | **0 (멈춤)** | **1,687,244** | 1/1/0/0 |
| relaxed | 1,216 | 8,746 | **0**/0/0/**3** |
| strict | **0 (멈춤)** | **1,646,170** | 1/1/0/0 |
| relaxed | 1,221 | 8,857 | **0**/0/0/**3** |
| strict | 1,215 | 8,748 | 1/14/0/0 |

보강 4회(전부 relaxed): **1,416 / 1,018 / 1,356 / 1,390 프레임**, 실패 주소 0,
걸친 활성화 1~3회. **relaxed 누계 8회 8회 정상.**

**회귀 없음 — pumpit1:** 2,848 프레임(오늘 기준선 2,865 / 2,735), 걸친 활성화 **0회**
— 그 타이틀에는 이 상황이 없습니다.

## 4. 사전 등록 기준 대조 — 전부 통과

* `generation failure addresses` **0** → 근인 제거 확인.
* strict에서만 멈춤 재현(2/5), relaxed 0/8 → 인과 확인.
* single-step 8,577~13,529로 정상 수준 유지(멈춤은 1.6~1.7M).
* pumpit1 회귀 없음.

## 5. 오늘 세션 전체 궤적 (pumpit3, 60초)

| 시점 | 정상 실행 | 프레임 |
|---|---|---|
| 세션 시작(Task 414 이전) | **11회 중 0회** | 0~1 |
| Task 414(지연 루프 batching) 이후 | 15회 중 13회 | 1,378~1,497 |
| Task 417 이후(relaxed) | **8회 중 8회** | 1,018~1,416 |

## 6. 회고

* **두 원인이 겹쳐 있었습니다.** 포화(414)와 번역 실패로 인한 arena 낙하(417)는 서로
  독립이며, 전자를 고치기 전에는 후자가 보이지 않았습니다.
* **415는 우회였고 417이 근인이었습니다.** 415(벌칙을 주소로 축소)는 옳은 변경이지만
  멈춤을 없애지 못했고, 그 실패가 "왜 그 항목이 애초에 활성화되지 못하는가"라는 질문을
  남겨 417로 이어졌습니다. **A/B 스위치를 남겨 둔 덕분에 그 구분이 매번 한 번의
  측정으로 났습니다.**
* **전수 census가 방향을 바꿨습니다.** `last_eip` 15개로 "전역 trace 모드"라고 본 것을
  hotspot 전수 census가 반증하고 두 페이지로 좁혔습니다. 표본이 적은 지표로 방향을
  정하면 하루를 잃습니다.

---

# Task 417 Work Log — activating the straddling requested entry (**the last stall is gone**)

## 1. Result in one line

**The stall is gone.** In one session, strict (the old rule) stalled **two of five** runs
while relaxed (the new one) was **healthy in eight of eight**, and relaxed runs produce **no
generation failure at all** (`generation failure addresses` = 0).

## 2. Change

One place in `aot_code_cache_win32.cpp`: in the append loop a **requested entry** refused by
the original rule stays active unless it spans a **quarantined** page; every other entry
keeps the old rule. The correctness argument is the design's: the image was just translated
from current bytes, and `RegisterAddressMapPages` registers the entry under **every page it
spans**, so a later write to either page still retires it.

## 3. A/B (one session, 60 s, EEPROM isolated per run)

Strict: 1,388 / 1,201 / **stall** / **stall** / 1,215 frames, with single steps of 8,397 to
12,903 in the healthy runs against **1,646,170 and 1,687,244** in the stalls. Relaxed:
1,223 / 1,209 / 1,216 / 1,221, plus four more at 1,416 / 1,018 / 1,356 / 1,390 — **eight of
eight healthy**, zero failed addresses, and one to three spanning activations each.

**No pumpit1 regression:** 2,848 frames against today's 2,865 and 2,735, with **zero**
spanning activations, since the situation does not arise there.

## 4. Against the pre-registered reading — all four pass

Failed addresses fall to zero; stalls reproduce only under strict (two of five) and never
under relaxed (zero of eight); single steps stay at healthy levels; pumpit1 does not
regress.

## 5. The session's trajectory for pumpit3 at 60 seconds

Zero healthy runs in eleven at the start, thirteen of fifteen after Task 414's delay-loop
batching, and **eight of eight** after this change, at 1,018 to 1,416 frames.

## 6. Retrospective

**Two causes overlapped**: saturation (Task 414) and the arena fall-through from a failed
translation (this task) are independent, and the second was invisible until the first was
fixed. **Task 415 was a detour and Task 417 the root**: narrowing the penalty to an address
was right but did not stop the stall, and that failure is what raised the question of why the
entry could not activate in the first place — the A/B switches are what made each of those
verdicts a single measurement. And the **full census changed the direction**: reading
fifteen `last_eip` samples as "global trace mode" was refuted by the hotspot census, which
narrowed it to two pages. Choosing a direction from a thin sample costs a day.
