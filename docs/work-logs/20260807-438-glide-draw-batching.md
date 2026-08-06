# Task 438 작업 로그 — Glide draw batching (opt-in 구현)

설계: [20260807-438](../design/20260807-438-glide-draw-batching.md) ·
작업 지시: [20260807-438](../work-orders/20260807-438-glide-draw-batching.md)

## 1. 한 줄 결과

삼각형마다 걸리던 host rendezvous를 **순서 경계까지 모았다가 한 번에** 넘기는 경로를
`REPIU_GLIDE_DRAW_BATCH` opt-in으로 넣었습니다. 스모크에서 왕복이 **2,694 → 1,347(2.00배)**
로 줄고, 총 삼각형 수와 구현 공백은 그대로입니다.

## 2. 순서 계약은 규칙 하나입니다

```
draw가 아닌 게이트를 처리하기 전에 무조건 flush
```

상태 변경·질의·swap·clear·LFB·다운로드를 **열거하지 않습니다.** 열거는 빠뜨리는 순간
순서 오류가 되고, 그 증상은 "가끔 그림이 이상하다"라서 추적이 어렵습니다.

**생략 short-circuit 뒤에 배치했습니다.** 생략된 setter는 이 지점에 닿기 전에 반환하며,
아무것도 바꾸지 않았으므로 flush를 유발하면 안 됩니다. Tasks 365/437이 batching의
**전제 조건**인 이유가 이것입니다 — 생략이 없으면 프레임당 290회의 setter가 전부 flush
지점이 됩니다.

## 3. 설계에서 바꾼 것 셋

**(a) 폴리곤은 배치에서 제외했습니다.** `grDrawPolygon` 계열은 `GL_TRIANGLE_FAN`인데,
팬 두 개를 한 `glBegin` 안에 넣으면 **첫 정점을 공유하는 하나의 팬으로 합쳐집니다.**
근사가 아니라 틀린 그림이므로 즉시 그리는 경로를 유지했습니다. PIU는 폴리곤 진입점을
호출하지 않으므로 잃는 것이 없습니다(371초 census: 호출 0).

**(b) teardown flush 사유를 없앴습니다.** `grBufferSwap`·`grSstWinClose`·
`grGlideShutdown`이 모두 draw가 아니므로 일반 규칙이 이미 덮습니다. 도달 불가능한
카운터를 남기는 대신, 종료 시 남은 정점은 `pending`으로 **그려진 척하지 않고**
보고합니다. 그것은 애초에 제출된 적 없는 프레임의 일부입니다.

**(c) 통계 단위를 정점 → 프리미티브로 고쳤습니다.** 첫 구현의 `mean-batch`는 정점
평균이라 삼각형에서 3배로 읽혔습니다. **왕복 감소 배수는 프리미티브/flush**이므로 그
값을 직접 싣습니다. 첫 스모크의 `mean-batch=6.00`은 실제로는 2.00배였습니다.

**(d) GL 상수 대신 자체 열거형을 씁니다.** `GL_POINTS`가 0이라 "비어 있음"과 구분되지
않았습니다. `Win32GlideBatchPrimitive`로 두고 backend가 GL로 옮깁니다. 덤으로 boundary가
GL 헤더에 의존하지 않습니다.

## 4. 변경

| 파일 | 내용 |
|---|---|
| `glide_draw_batch.h`/`.cpp`(신규) | 큐·용량·flush 사유·opt-in 정책·스냅샷 |
| `glide_opengl_backend.h`/`.cpp` | `DrawPrimitiveBatch` + 상태/정점 방출을 `PrepareDrawState`·`EmitDrawVertex`로 분리 |
| `linexe_glide_boundary.cpp` | flush 규칙, draw 4개 지점의 큐 적재 |
| `thread_context.h` · `execution_trampoline.h` · `live_telemetry_snapshot.cpp` | 큐 소유와 스냅샷 배선 |
| `main.cpp` | 요약 2줄(회계 + flush 사유) |
| `glide_draw_batch_probe.{h,cpp}`(신규) · `main.cpp` · `CMakeLists.txt` | 단정 9종 |
| 가이드 6단계 · README | A/B 절차와 새 변수 |

**단일/배치 경로가 갈라지지 않도록** 상태 적용과 정점 방출을 함수 둘로 쪼개 양쪽이
같은 코드를 씁니다.

## 5. 검증 (2026-08-07, Win32 x86 Debug)

| 검증 | 결과 |
|---|---|
| 빌드 | **exit 0** |
| `repiu_aot_probe.exe MASTER\PIU_1ST\PIU\PIU.EXE` | **exit 0**, `glide_draw_batch_all=true`(정책·멤버십·순서·빈 flush·primitive 전환·용량·실패 회계·불활성) |
| 스모크 `=0` (12초) | 배치 비활성, draw 2,568, 구현 공백 0/0/0/0/0/0, exit 0 |
| 스모크 `=1` (12초) | **primitives-queued 2,694 = primitives-drawn 2,694 = `_GRDRAWTRIANGLE` 2,694**, pending 0, failures 0, flushes 1,347, **mean-batch 2.00**, 구현 공백 0/0/0/0/0/0, exit 0 |

**회계가 닫힙니다** — 큐에 넣은 프리미티브 수, 그린 프리미티브 수, 게이트 호출 수가
셋 다 같고 남은 것이 없습니다. flush 사유는 전부 `non-draw-gate`이며 용량·primitive
전환은 0이었습니다.

**이 스모크는 attract 구간이라 배치가 짧습니다.** 프레임당 draw가 8.6개뿐이어서
flush 지점 사이에 2개가 모입니다. 실부하 gameplay는 프레임당 draw 670개에 flush 지점
약 100개이므로 설계가 예측한 배치 길이는 **약 6.7**입니다. 그 값과 프레임 효과는
사용자 A/B에서 확인해야 합니다.

## 6. 사용자 A/B 1차 (03:00) — **무효**, 그러나 단가를 얻었습니다

사용자가 가이드대로 `batch-off.log`/`batch-on.log`를 남겼는데, **두 실행 모두 배치
코드가 없는 바이너리**였습니다. 가이드가 `Release` 경로를 적어 두었는데 제가 Debug만
빌드해 둔 것이 원인입니다 — **지시서에 Release 재빌드를 넣었어야 했습니다.**

| 근거 | 값 |
|---|---|
| Release 바이너리 시각 | 02:20 = Task 437까지 |
| `Glide draw batch` 요약 줄 | 두 로그 모두 **없음** |
| ordinal 73 `rendezvous` | 2,533,050 / 2,341,566 = **draw 호출 수와 동일** |

**대신 측정 조건은 처음으로 정확했습니다**(`swap interval ... effective: 0`,
time profile ON). 그래서 설계가 미지수로 남긴 **왕복 1회의 단가**가 나왔습니다.
`_GRDRAWTRIANGLE@12` 2,533,050회, 호출당 **7,373 cycle**:

| 구간 | cycle/호출 | 비중 |
|---|---:|---:|
| queue | 518 | 7.0% |
| **wake** | **1,957** | **26.5%** |
| **work(실제 GL)** | **948** | **12.9%** |
| complete | 1,295 | 17.6% |
| backend 밖(게이트·디코드) | 2,654 | 36.0% |

**실제 GL 작업은 12.9%뿐이고 순수 왕복이 3,770 cycle(51%)입니다.** 배치가 없애는 것이
정확히 이 3,770이며, draw ordinal은 guest-run의 **6.55%** 이므로 상한은 그 절반 남짓,
**guest-run의 2~3%** 입니다. "왕복 6.7배"는 횟수의 배수일 뿐 프레임의 배수가 아닙니다.

## 7. 제 Release 스모크 — attract에서는 **이득이 없습니다**

Release를 다시 빌드해(03:03) 15초씩 A/B를 돌렸습니다. 기구는 정확히 동작합니다
(`primitives-queued = drawn = _GRDRAWTRIANGLE count`, pending 0, failures 0, 공백 0).

| 지표 | `=0` | `=1` |
|---|---:|---:|
| draw ordinal 호출당 | 10,143 cycle | **3,817 cycle** |
| ordinal 73 `rendezvous` | 62,258 | **0** |
| mean-batch / max-batch | — | **2.00 / 2** |
| **glide-gate 총 cycle** | 10.89e9 (19.62%) | **11.10e9 (20.00%)** |

**draw ordinal의 호출당 비용은 2.66배 줄었지만 총 게이트 비용은 줄지 않았습니다.**
비용이 사라진 게 아니라 flush를 유발한 ordinal로 **옮겨간 것**이고(`grBufferSwap`의
`rendezvous`가 8,492 호출에 16,801), 배치 길이가 2라 왕복 절반만 없어져 그 이득이
큐 적재·flush 비용과 상쇄됩니다. 두 실행의 프레임이 3.9% 다르므로 +1.9%는 편차와
구분되지 않습니다 — **"이득 없음"이 정확한 표현이고 "손해"라고 말할 근거는 없습니다.**

**attract는 이 축을 판정할 수 없는 장면입니다.** 프레임당 draw가 7개뿐이고 게임이
사각형(삼각형 2개) 단위로 그려 배치가 항상 2입니다. gameplay 로그로 상한을 계산하면
draw 2,533,050 / flush 지점 465,569 = **5.44**이고, 실제 값은 다음 실행의 `mean-batch`가
말해 줍니다. **그 값이 2 근처면 이 축은 닫힙니다.**

## 8. 남은 것

1. **사용자 A/B**(가이드 6단계). Task 437 A/B가 vsync 때문에 판정 불가로 끝났으므로,
   이번에는 `REPIU_GLIDE_SWAP_INTERVAL=0`·`REPIU_EXECUTION_TIME_PROFILE=1`·
   `REPIU_GLIDE_ORDINAL_TIME_PROFILE=1`을 **필수**로 적었습니다.
2. 승격은 Task 437과 **함께** 판단합니다. 두 스위치는 같은 축이고, batching이 켜지면
   생략의 값어치가 커집니다.
3. `grTexSource`(프레임당 32.1)는 여전히 bind마다 왕복 1회입니다. 포인터 내용을 키에
   넣는 작업이 남아 있습니다.

---

# Task 438 Work Log — Glide draw batching, implemented as an opt-in

## 1. Result in one line

The per-triangle host rendezvous is replaced by a queue handed over **once per ordering
boundary**, behind `REPIU_GLIDE_DRAW_BATCH`. A smoke shows round trips falling **2,694 to 1,347 —
2.00x** with the total triangle count and the implementation-gap counters unchanged.

## 2. The ordering contract is one rule

*Flush before handling any non-draw gate.* State changes, queries, swap, clear, LFB and downloads
are **not enumerated**, because an enumeration is wrong the moment it misses a case and the
symptom — "the picture is occasionally odd" — is hard to trace. The rule sits **after** the
elision short-circuit: an elided setter returns before reaching it and changed nothing, so it must
not force a flush. That is precisely why Tasks 365 and 437 are a precondition here; without them
all 290 setter calls per frame would be flush points.

## 3. Four departures from the design

**Polygons are excluded.** `grDrawPolygon` maps to `GL_TRIANGLE_FAN`, and two fans inside one
`glBegin` **merge into a single fan around the first vertex** — not an approximation but a wrong
picture. They keep drawing immediately, and PIU calls no polygon entry point, so nothing is lost.

**The teardown flush reason is gone.** `grBufferSwap`, `grSstWinClose` and `grGlideShutdown` are
all non-draw gates, so the general rule already covers them. Rather than keep an unreachable
counter, what remains at exit is reported as `pending` instead of being counted as drawn — it is
part of a frame that was never presented.

**Statistics count primitives, not vertices.** The first implementation's `mean-batch` was a
vertex mean, which reads three times too high for triangles; the reduction factor is
primitives per flush, so that is what the line carries now. The first smoke's `6.00` was really
2.00.

**The queue uses its own primitive enum.** `GL_POINTS` is zero and would have been
indistinguishable from an empty batch; `Win32GlideBatchPrimitive` fixes that and keeps the
boundary free of GL headers.

## 4. The change

New `glide_draw_batch` header and source hold the queue, capacity, flush reasons, opt-in policy
and snapshot; the backend gains `DrawPrimitiveBatch` with the state application and vertex
emission split into `PrepareDrawState` and `EmitDrawVertex` **so the single and batched paths
cannot drift apart**; the boundary carries the flush rule and enqueues at the four draw sites; the
context, attempt struct and snapshot are wired; the loader prints two summary lines; a new probe
with nine assertions is registered; and the guide gains step six.

## 5. Verification (2026-08-07, Win32 x86 Debug)

The build exits 0 and the probe reports `glide_draw_batch_all=true`, covering the opt-in policy,
gate membership including the polygon exclusion, vertex order through a flush, the harmless empty
flush, the primitive-kind change, the capacity bound that never splits a primitive, failure
accounting, and null-safety. A 12-second `=1` smoke reports **primitives queued 2,694, drawn
2,694, and `_GRDRAWTRIANGLE@12 count` 2,694** with zero pending, zero failures, 1,347 flushes and
a mean batch of 2.00, alongside zero implementation gaps in both configurations.

**The accounting closes**: queued, drawn and gate-called primitives are the same number with
nothing left over, and every flush was the non-draw-gate rule. Batches are short here because the
attract section draws only 8.6 triangles per frame; a real load section draws 670 against roughly
100 flush points, where the design predicts about 6.7 — a number the user's A/B must confirm
along with the frame effect.

## 6. The user's first A/B — void, but it yielded the unit cost

The `batch-off.log` and `batch-on.log` captures were both taken with **a binary that has no
batching code**: the guide names the `Release` path and only Debug had been rebuilt, which is my
omission — the work order should have called for a Release rebuild. The logs prove it themselves:
no `Glide draw batch` line at all, and ordinal 73 reporting `rendezvous` exactly equal to the draw
count in both.

**The measurement conditions were finally right, though** (swap interval effective 0, time profile
on), so they answer what the design left open: **what one rendezvous costs**. Over 2,533,050
draws at **7,373 cycles per call**, the split is queue 518, wake 1,957, **work 948**, complete
1,295, and 2,654 outside the backend. **The actual GL work is 12.9%; the round trip is 3,770
cycles, 51%.** That is what batching removes, and since the draw ordinal is **6.55% of guest-run**,
the ceiling is **2-3% of guest-run** — the 6.7x is a count ratio, never a frame ratio.

## 7. My Release smoke — no gain in the attract phase

With Release rebuilt, a 15-second A/B shows the mechanism working exactly (queued equals drawn
equals the gate call count, zero pending, zero failures, zero implementation gaps) and the draw
ordinal's per-call cost falling **10,143 to 3,817 cycles, 2.66x**, with ordinal 73's `rendezvous`
at **zero**. But **total glide-gate cycles did not fall**: 10.89e9 (19.62%) against 11.10e9
(20.00%). The cost did not vanish, it **moved to whichever ordinal triggered the flush** —
`grBufferSwap` reports 16,801 rendezvous for 8,492 calls — and at a batch length of **2.00** only
half the round trips go away, which the queueing and flush costs offset. With frames differing
3.9% between the runs, +1.9% is indistinguishable from variance: **"no gain" is the honest
reading, and there is no basis for saying "worse"**.

**The attract phase cannot judge this axis.** It draws seven triangles per frame in quads, so the
batch is always two. The gameplay log bounds the batch at **2,533,050 draws over 465,569 flush
points = 5.44**, and the next run's `mean-batch` gives the real figure. **If that comes back near
two, this axis is closed.**

## 8. Left open

The user's A/B (guide step six) **must** enable `REPIU_GLIDE_SWAP_INTERVAL=0`,
`REPIU_EXECUTION_TIME_PROFILE=1` and `REPIU_GLIDE_ORDINAL_TIME_PROFILE=1`, since Task 437's A/B
was void for want of the first. Promotion is decided together with Task 437, both being the same
axis. `grTexSource` still costs one rendezvous per bind, 32.1 per frame, until the pointed-to
`GrTexInfo` contents enter the elision key.
