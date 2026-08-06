# Task 437 작업 로그 — 텍스처 상태 setter 생략을 A/B 가능 상태로

설계: [20260807-437](../design/20260807-437-glide-texture-setter-elision.md) ·
작업 지시: [20260807-437](../work-orders/20260807-437-glide-texture-setter-elision.md)

## 1. 출발점 — 사용자 로그 하나가 축을 정했습니다

사용자가 남긴 371.3초 gameplay 실행(v0.0.136, pumpit1, 20,212프레임, 54.4 fps)에서
Glide 게이트 크로싱 **5,586,761건** 중 draw는 **24.9%** 뿐이고 나머지 **74.3%가 상태
setter**였습니다. 그리고 Task 365의 batch 1은 이미 **99.999%** 를 생략하고 있었습니다
(`elided 2,048,762 / applied 22`).

남은 최대 무리는 텍스처 상태 블록이고, 네 ordinal의 호출 수가 **정확히 같습니다**.

| ordinal | 호출 | 프레임당 | 처리 |
|---|---:|---:|---|
| `grTexClampMode` | 395,764 | 19.6 | **batch 2 포함**(rendezvous 있음) |
| `grTexFilterMode` | 395,764 | 19.6 | **batch 2 포함**(rendezvous 있음) |
| `grTexMipMapMode` | 395,764 | 19.6 | 포함하되 ABI 전용이라 **절감 없음** |
| `grTexSource` | 395,764 | 19.6 | **제외** — 아래 §3 |

절감 대상은 **791,528 rendezvous, 프레임당 39.2회**입니다.

## 2. 게이트 예외가 사라졌다는 사실이 이 축을 다시 열었습니다

같은 로그의 direct dispatch가 `entry/success = 5,586,761/5,586,761`, target-miss 0
입니다. **크로싱마다 붙던 예외가 없으므로 남은 비용은 host rendezvous 하나**이고,
그것이 생략이 없애는 대상입니다. Task 365가 "rendezvous 41,368회를 없애도 프레임이
안 움직였다"고 적었을 때의 조건과 다릅니다 — 그래서 재측정이 필요하고, 그래서 이번
작업은 **승격이 아니라 A/B 가능 상태까지**입니다.

## 3. 판단 두 가지

**`grTexSource`는 제외했습니다. 근거는 반복률이 아니라 인자 형태입니다.** 네 번째
인자가 `GrTexInfo*` 포인터라, 같은 포인터 뒤의 구조체가 바뀌어도 키가 같아집니다.
`texture_generation`은 **다운로드만** 잡으므로 이 경로를 막지 못합니다. batch 1이 적어
둔 "반복률 32.24%"보다 강한 제외 사유이고, probe가 이 사실을 단정으로 고정합니다.

**clamp/filter 생략은 정확성 위험이 없습니다 — 이미 bind가 복원하기 때문입니다.**
Glide의 clamp/filter는 TMU 상태인데 OpenGL 대응물은 텍스처 객체별 파라미터라, 순진하게
옮기면 "다른 텍스처를 bind하면 낡는다"는 위험이 생깁니다. 그러나 `SetTextureSource`가
bind 직후 `tmu_*_`에서 네 파라미터를 다시 적용합니다
(`glide_opengl_backend.cpp:1405-1416`). 따라서 값이 같은 호출은 **같은 필드에 같은 값을
쓰고 이미 올바른 텍스처에 같은 `glTexParameteri`를 재발행하는 순수 무동작**이며 생략과
동치입니다. batch 1이 이 셋을 미룬 유일한 이유가 여기서 해소됩니다.

## 4. 변경

| 파일 | 내용 |
|---|---|
| `glide_setter_state_cache.h/.cpp` | `GlideSetterTextureStateElisionEnabled()`(opt-in, `runtime::ResolveOptInToggle`), `IsGlideSetterTextureStateElisionGate()`, 스냅샷 `texture_state` |
| `linexe_glide_boundary.cpp` | `elision_candidate_`가 두 목록을 OR — **규칙이 아니라 목록만** 넓힘 |
| `main.cpp` | 생략 요약에 `texture-state` 표기(로그가 스스로 구성을 밝히도록) |
| `glide_setter_state_cache_probe.cpp` | 멤버십·분리·`grTexSource` 제외 단정 |
| 가이드·README | batch 2 A/B 절차와 새 변수 |

키·텍스처 세대·무효화 규칙은 **건드리지 않았습니다.** census가 천장을 잰 그 규칙
그대로여야 "census `same` == cache `elided`" 교차 검증이 성립합니다.

## 5. 검증 (2026-08-07, Win32 x86 Debug)

| 검증 | 결과 |
|---|---|
| 빌드 | **exit 0** |
| `repiu_aot_probe.exe MASTER\PIU_1ST\PIU\PIU.EXE` | **exit 0**, `glide_setter_state_cache_all=true` — 신규 `texture_membership=true` 포함 |
| 스모크 A (`=0`, 8초) | `enabled/texture-state/entries/elided/applied` = `true/false/7/955/16`, 구현 공백 0/0/0/0/0/0, exit 0 |
| 스모크 B (`=1`, 8초) | `true/true/**10**/1759/28`, 구현 공백 0/0/0/0/0/0, exit 0 |

**entries가 7 → 10으로 정확히 셋 늘었습니다.** 두 스모크는 8초 동안 진행도가 달라
(텍스처 setter 각 118회 대 160회) 절감량을 직접 빼서 비교할 수는 없습니다. 진행도로
정규화하면 batch 1분은 955 × 160/118 ≈ 1,295이고 B의 1,759에서 그것을 빼면 약 464로,
그 구간 텍스처 호출 480회의 대부분이 생략됐다는 **일관성 확인**까지가 이 스모크가
말할 수 있는 전부입니다.

**측정하지 않은 것:** 프레임 효과. 부팅·attract 구간이라 gameplay의 setter 밀도가
없습니다. 승격 판단은 사용자 A/B(가이드 5단계)를 기다립니다.

## 6. 함께 닫은 것

**Task 433(정점 깊이)이 완료됐습니다.** 사용자가 gameplay 3D 모델이 정상으로 보인다고
확인했고, 같은 실행이 구현 공백 0과 게이트 전건 처리를 함께 기록했습니다. 433 작업
로그의 "육안 확인 대기"를 확인 완료로 갱신하고, Glide analysis에 이번 전수 census를
날짜 절로 남겼습니다. **"PTX 465개 중 4개만 Glide 도달"이라는 옛 요약도 폐기했습니다** —
이 실행만으로 distinct 62입니다.

## 7. 사용자 A/B 결과 (2026-08-07 02:20~02:25, 각 2회)

`REPIU_GLIDE_SETTER_ELIDE_TEXTURE`만 바꿔 4회. **정확성은 통과, 성능은 판정 불가**입니다.

| 지표 | `=0` (2회) | `=1` (2회) |
|---|---|---|
| `voided` | 0 / 0 | **0 / 0** |
| 구현 공백 | 0/0/0/0/0/0 | 0/0/0/0/0/0 |
| `elided + applied` ÷ 대상 호출 | **100.00%** | **100.00%** |
| entries | 7 | **10** |
| 텍스처 census distinct | 34 / 34 | 34 / 35 |
| 경고·오류 줄 수 | 8 / 6 | 8 / 6 |
| fps | 49.35 / 47.42 | 49.14 / 45.83 |

**정확성.** census가 꺼져 있어 `same == elided` 교차검증은 못 했지만, **호출 수 회계가
정확히 닫혔습니다**(대상 호출 = elided + applied, 오차 0). 트리오 385,197건이 추가로
덮였는데 `applied`는 6,318 → 7,251, **+933뿐** — 텍스처 setter는 **99.76%가 중복**
이었습니다. 사용자도 두 구성의 화면 차이가 없다고 확인했습니다.

**성능은 이 측정으로 판정할 수 없습니다.** `swap interval override requested=false`,
즉 **vsync가 켜진 상태**였고(가이드가 못박은 `REPIU_GLIDE_SWAP_INTERVAL=0` 누락),
time profile과 census도 꺼져 있었으며, 구간도 서로 달랐습니다(draws/frame 652·686·671·
**514**, 길이 27.8~81.5초). fps 차 −1.9%는 편차와 구분되지 않습니다.

**따라서 승격하지 않고 opt-in을 유지합니다.** 정확성 근거는 갖췄으므로, 승격 판단은
아래 draw batching 측정과 함께 하는 편이 낫습니다.

## 8. 이 A/B가 바꾼 것 — 다음 축은 setter가 아니라 draw입니다

이번 4회는 **draw가 프레임당 652~686개**로, Task 437의 근거였던 371초 실행(68.7)의
**10배**입니다. 그 실행은 메뉴가 섞여 draw를 24.9%로 보이게 했지만, **실제 부하
구간에서 draw는 크로싱의 69.9%** 입니다.

그리고 `DrawTriangle`은 **삼각형 1개당 `InvokeOnHostThread` 1회**입니다
(`glide_opengl_backend.cpp:1152-1159`).

| 프레임당 host rendezvous (`=1` 실행) | 회 |
|---|---:|
| **draw** | **670.8** |
| 미적용 setter(`grTexSource` 32.1 · `grDepthMask` 18.6 등) | 65.5 |
| 적용된 setter | 1.8 |
| 생략된 setter(rendezvous 없음) | (221.3) |

Task 437이 없앤 96회는 **draw 670회의 7분의 1**입니다. vsync를 걷어내더라도 이 축의
지배항은 draw이며, 다음 작업은 **Task 438 draw batching**입니다.

## 9. 남은 것

1. **batch 2 승격 판단** — 정확성은 확정. 프레임 근거는 Task 438 측정과 함께 확보합니다.
2. `grTexSource`는 포인터 내용을 키에 넣기 전에는 생략 불가. bind당 rendezvous 1회 잔존.
3. `grDepthMask`(326,884) · `grConstantColorValue`(72,710) · combine 2종 · `grDitherMode`는
   각자 근거가 필요한 batch 3 후보입니다.

---

# Task 437 Work Log — making the texture-state elision A/B-able

## 1. One user log chose the axis

In the user's 371.3-second gameplay run (v0.0.136, pumpit1, 20,212 frames, 54.4 fps), draws are
only **24.9%** of the 5,586,761 Glide gate crossings and **74.3% are state setters** — and Task
365's batch one was already eliding **99.999%** of its share (2,048,762 elided against 22
applied). The largest remaining group is the texture-state block, whose four ordinals are called
**exactly 395,764 times each**: one four-call block per bind. Two of them cost a host rendezvous,
making the prize **791,528 rendezvous, 39.2 per frame**.

## 2. The vanished gate exception is what reopened this axis

Direct dispatch handled all 5,586,761 crossings with no target miss, so **the per-call exception
is gone and the remaining cost is the rendezvous** — exactly what elision removes. That is a
different condition from the one under which Task 365 reported "41,368 rendezvous removed, frames
unchanged", which is why this task stops at **A/B-able rather than promoted**.

## 3. Two judgements

**`grTexSource` stays out, on argument shape rather than repeat rate.** Its fourth argument is a
`GrTexInfo*`, and the struct behind an unchanged pointer can change without a download, which
`texture_generation` does not catch — a stronger exclusion than batch one's "repeats only 32.24%
of the time", and the probe now pins it.

**Eliding clamp and filter carries no correctness risk, because the bind already restores them.**
Glide's clamp and filter are TMU state while OpenGL's are per-texture-object parameters, which
would make elision wrong if a later bind left them stale — but `SetTextureSource` re-applies all
four from `tmu_*_` immediately after binding (`glide_opengl_backend.cpp:1405-1416`). A
same-valued call is therefore a pure no-op, and this retires the one reason batch one gave for
deferring these three gates.

## 4. The change

The cache gains an opt-in policy (`runtime::ResolveOptInToggle`), the three-gate list and a
`texture_state` snapshot field; the boundary ORs the two lists in its candidacy test, **widening
the list and not the rules**; the loader prints the flag so a log states its own configuration;
and the probe asserts membership, disjointness from batch one, and the `grTexSource` exclusion.
The key, texture-generation and invalidation rules are untouched, which is what keeps the census
`same` versus cache `elided` cross-check meaningful.

## 5. Verification (2026-08-07, Win32 x86 Debug)

The build exits 0 and the probe passes with the new `texture_membership` assertion. Two
eight-second smokes report `elision enabled/texture-state/entries/elided/applied` of
`true/false/7/955/16` and `true/true/10/1759/28`, both with zero Glide implementation gaps and
exit 0: **entries rise by exactly three**. The two smokes did not reach the same progress (118
versus 160 texture setter calls), so the elision delta cannot be subtracted directly; normalising
batch one by progress leaves about 464 of that run's 480 texture calls elided, which is a
**consistency check and not a measurement**. The frame effect is deliberately unmeasured here —
a boot and attract section has none of gameplay's setter density — and the promotion decision
waits on the user's A/B in step five of the guide.

## 6. Closed alongside

**Task 433 (vertex depth) is complete**: the user confirms the gameplay 3D models look correct,
and the same run recorded zero implementation gaps with every gate crossing handled. Its work log
now reads confirmed, the Glide analysis carries a dated section with this full call census, and
the stale summary that "only four of 465 PTX assets reach Glide" is retired — this run alone
shows 62 distinct textures.

## 7. The user's A/B (2026-08-07, two runs per configuration)

**Correctness passes; performance is not decidable from these runs.** `voided` is zero and the
Glide implementation gaps are zero in all four, `entries` goes 7 to 10, and the call accounting
**closes exactly** — covered calls equal elided plus applied with no remainder, which stands in
for the census cross-check the disabled census could not provide. The three texture gates added
385,197 covered calls and only **933 more applications** (6,318 to 7,251): they are **99.76%
redundant**, and the user reports no visual difference between the configurations.

The frame comparison is void: `swap interval override requested=false` means **vsync was on**,
which the guide names as a precondition failure, the time profile and census were off, and the
sections differ (652, 686, 671 and **514** draws per frame over runs of 27.8 to 81.5 seconds).
The −1.9% fps difference is indistinguishable from variance. **The switch therefore stays
opt-in**, with the promotion decision folded into the next measurement.

## 8. What the A/B changed — the next axis is draws, not setters

These runs carry **652-686 draws per frame, ten times the 68.7 of the 371-second run** that
motivated this task; that run's menu-heavy mix made draws look like 24.9% of crossings, while a
real load section puts them at **69.9%**. And `DrawTriangle` takes **one `InvokeOnHostThread` per
triangle** (`glide_opengl_backend.cpp:1152-1159`), so per frame the rendezvous split is 670.8 for
draws, 65.5 for uncovered setters (`grTexSource` at 32.1, `grDepthMask` at 18.6), and 1.8
applied, against 221.3 elided that cost nothing. **What Task 437 removed is one seventh of the
draw traffic**, so the dominant term is draw batching — Task 438.

## 9. Left open `grTexSource` cannot be elided until the pointed-to contents enter the key, leaving one
rendezvous per bind. `grDepthMask` (326,884), `grConstantColorValue` (72,710), the two combine
setters and `grDitherMode` remain batch-three candidates, each needing its own evidence.
