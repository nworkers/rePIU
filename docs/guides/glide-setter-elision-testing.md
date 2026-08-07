# Glide setter 생략 기본값 검증 가이드 / Testing the Glide setter elision default

이 문서는 Task 365가 기본 ON으로 넣은 **동일 Glide 상태 생략**을 사용자가 직접 실제
플레이 장면에서 검증하고, 기본값을 유지할지 opt-in으로 되돌릴지 판단하기 위한 절차입니다.

* 설계: [20260730-365-glide-setter-state-elision.md](../design/20260730-365-glide-setter-state-elision.md)
* 측정 결과: [작업 로그](../work-logs/20260730-365-glide-setter-state-elision.md)

## 왜 사용자 검증이 필요한가

Task 365의 자동 60초 측정은 **부팅부터 시작하는 장면**이라 `grLfbLock` 구간을 포함하고,
그 장면에서는 결론이 이렇게 나왔습니다.

| 항목 | 결과 |
|---|---|
| 정확성 | **증명됨** — 관측된 중복만 생략, 렌더 시퀀스 동일 |
| Glide gate 비용 | **-5.13%p** (rendezvous 41,368회 제거) |
| 프레임 | **변화 없음** (1,215 → 1,206, 편차 내) |

즉 **비용은 확실히 줄었지만 이 장면에서는 프레임으로 환산되지 않았습니다.** 반면
Task 363이 기록한 "호출량이 늘면 FPS가 급락하는" 실제 플레이 장면에서는 상태 setter가
Glide gate의 85.33%, wall-clock의 20.59%였습니다. 그 장면에서는 결과가 다를 수 있으나
아직 측정되지 않았습니다. **그 장면을 잡을 수 있는 사람은 사용자뿐입니다.**

## 1단계 — 두 구성으로 같은 구간을 플레이

FPS가 실제로 떨어지는 구간을 두 번, 가능하면 같은 곡·같은 난이도·같은 길이로
플레이합니다.

```
:: 생략 ON (현재 기본값)
set REPIU_EXECUTION_BACKEND=dynamic
set REPIU_EXECUTION_TIMEOUT_MS=0
set REPIU_GLIDE_SWAP_INTERVAL=0
set REPIU_EXECUTION_TIME_PROFILE=1
set REPIU_GLIDE_ORDINAL_TIME_PROFILE=1
set REPIU_GLIDE_SETTER_CENSUS=1
set REPIU_GLIDE_SETTER_ELIDE=1
build\win32_x86_debug\Release\repiu.exe pumpit1 2> elide-on.log
```

```
:: 생략 OFF (기존 동작)
set REPIU_GLIDE_SETTER_ELIDE=0
build\win32_x86_debug\Release\repiu.exe pumpit1 2> elide-off.log
```

`REPIU_GLIDE_SWAP_INTERVAL=0`은 Task 371 이후 측정의 필수 조건입니다 — vsync 구성은
프레임을 30 fps로 양자화해서 CPU 개선이 프레임에 나타나지 않습니다. 게임 플레이
기본값은 vsync 그대로이고, 이 변수는 측정할 때만 씁니다.

나머지 환경 변수는 두 실행에서 **완전히 같아야** 합니다. 로그는 stderr로 나오므로 위처럼
`2>`로 받습니다.

**주의:** 실행 간 프레임 편차가 18%까지 관측됐고 각 구성의 첫 실행이 늘 가장 느립니다
(Task 335). 따라서 **각 구성 3회 이상** 하고 중앙값을 쓰십시오. 1회 비교로는 판단할 수
없습니다.

## 2단계 — 로그에서 볼 숫자

각 로그 끝에서 다음 네 줄을 찾습니다.

| 찾을 문자열 | 읽을 값 |
|---|---|
| `Win32 Glide call trace: ordinal=... _GRBUFFERSWAP@4 count=` | **프레임 수** |
| `Win32 execution time share veh/glide-gate/...` | **glide-gate 비중** |
| `Win32 Glide setter elision enabled/entries/elided/applied/...` | **생략 횟수** |
| `Win32 Glide setter census enabled/entries/calls/first/same/changed/...` | **호출 수와 동일 상태 수** |

PowerShell로 한 번에 뽑는 방법입니다.

```powershell
foreach ($f in @("elide-off.log", "elide-on.log")) {
    "=== $f ==="
    Select-String -Path $f -Pattern `
        "_GRBUFFERSWAP@4 count=|execution time share|setter elision enabled|setter census enabled" |
        ForEach-Object { $_.Line }
}
```

## 3단계 — 판정

### 정확성 (이게 먼저입니다)

**census `same` 과 elision `elided` 가 같아야 합니다.** census는 동작을 바꾸지 않는
관측자이므로, 두 값이 같다는 것은 확인된 중복만 생략했다는 뜻입니다.

ON 로그에서 7종 setter의 `same` 합계와 `elided` 총합을 비교하십시오. 다르면
**즉시 `REPIU_GLIDE_SETTER_ELIDE=0`으로 되돌리고 로그를 첨부해 알려주십시오.** 자동
측정에서는 3회 모두 정확히 일치했지만, 다른 장면에서 어긋난다면 규칙에 빠진 경계가
있다는 뜻입니다.

`voided` 값도 확인하십시오. 자동 측정에서는 0이었습니다. 0이 아니면 backend 실패가
발생한 것이므로 함께 알려주십시오.

### 시각

같은 구간을 눈으로 비교하십시오. 특히 다음을 보십시오.

* 반투명(블렌딩) 요소 — 화살표 잔상, 페이드, 콤보 이펙트
* 화면 잘림 — `grClipWindow` 생략이 잘못되면 경계가 어긋납니다
* 색 채널 이상 — `grColorMask` 생략이 잘못되면 채널이 빠집니다
* 앞뒤 겹침 순서 — `grDepthBufferFunction` 생략이 잘못되면 뒤에 있어야 할 것이 앞에 옵니다

이상이 보이면 되돌리고 알려주십시오. 자동 측정에서는 렌더 시퀀스가 phase offset +1에서
72.9% 완전 일치했으므로(같은 프레임을 한 프레임 먼저 그림) 차이가 나올 가능성은 낮습니다.

### 성능

| 관측 | 의미 | 권고 |
|---|---|---|
| 프레임 중앙값 ON > OFF, 차이 5% 초과 | 이 장면은 setter 경로에 제한돼 있었음 | **기본 ON 유지.** batch 2 진행 가치 있음 |
| 프레임 차이 5% 이내 | 이 장면도 setter 경로에 제한되지 않음 | 기본값 결정은 취향. batch 2는 중단 |
| 프레임 중앙값 ON < OFF, 5% 초과 | 예상 밖 — 조사 필요 | 되돌리고 로그 첨부 |

glide-gate 비중은 두 경우 모두 내려갈 것입니다(비용은 확실히 줄어듦). 판단 기준은
**프레임**입니다.

## 4단계 — 함께 남기면 좋은 자료

`INT 8 chain HLE count` 줄도 함께 뽑아 주시면 Task 366(pacing 귀속)에 직접 쓸 수
있습니다. 실제 플레이 장면의 timer tick 전달률이 부팅 장면과 같은지가 현재 열린
질문입니다.

```powershell
Select-String -Path elide-on.log -Pattern "INT 8 chain HLE count" |
    ForEach-Object { $_.Line }
```

## 5단계 — batch 2(텍스처 상태) A/B — Task 437 · **완료, 기본값 승격됨**

> **결과(2026-08-07).** 텍스처 setter 셋은 **99.76% 중복**이었고(385,197건 중 적용
> 933건), `voided` 0, 시각 차이 없음이었습니다. Task 439에서 **기본값 켜짐**으로
> 승격했습니다. 아래 절차는 되돌려 재현할 때(`=0`) 그대로 씁니다.

batch 1(7종)은 **호출의 99.999%가 이미 생략**되고 있습니다(371초 gameplay 실측:
`elided 2,048,762 / applied 22`). 남은 최대 무리는 bind마다 불리는 텍스처 상태
블록이고, Task 437이 그중 셋을 **opt-in**으로 생략 대상에 넣었습니다.

| gate | 371초 실측 호출 | 프레임당 | 이번 A/B |
|---|---:|---:|---|
| `grTexClampMode` | 395,764 | 19.6 | 대상 — rendezvous 있음 |
| `grTexFilterMode` | 395,764 | 19.6 | 대상 — rendezvous 있음 |
| `grTexMipMapMode` | 395,764 | 19.6 | 대상이지만 **ABI 전용이라 절감 없음** |
| `grTexSource` | 395,764 | 19.6 | **제외**(`GrTexInfo*` 포인터 인자) |

절감 대상은 **791,528 rendezvous, 프레임당 39.2회**입니다.

```
:: A — 지금 기본값 (batch 1만)
set REPIU_GLIDE_SETTER_ELIDE=1
set REPIU_GLIDE_SETTER_ELIDE_TEXTURE=0
build\win32_x86_debug\Release\repiu.exe pumpit1 2> tex-off.log
```

```
:: B — batch 2 포함
set REPIU_GLIDE_SETTER_ELIDE_TEXTURE=1
build\win32_x86_debug\Release\repiu.exe pumpit1 2> tex-on.log
```

나머지 변수는 1단계와 같게 두고(`REPIU_GLIDE_SWAP_INTERVAL=0`,
`REPIU_EXECUTION_TIME_PROFILE=1`, `REPIU_GLIDE_ORDINAL_TIME_PROFILE=1`,
`REPIU_GLIDE_SETTER_CENSUS=1`), **각 구성 3회 이상** 같은 구간을 플레이하십시오.

읽을 줄은 1단계와 같고, 생략 요약에 **`texture-state`** 항목이 추가됐습니다.

```
Win32 Glide setter elision enabled/texture-state/entries/elided/applied/...
```

**판정 순서는 정확성 → 시각 → 성능으로 같습니다.**

* 정확성: B의 `elided` 증가분이 텍스처 3종의 `same` 합계와 일치해야 합니다. `voided`는
  0이어야 합니다.
* 시각: 텍스처 경계(반복/클램프)와 확대 시 뭉개짐(선형/최근접)을 특히 보십시오 —
  clamp/filter가 잘못 생략되면 **테두리 늘어짐**이나 **필터 변화**로 나타납니다.
* 성능: 프레임 중앙값 차이가 5%를 넘으면 승격 근거가 됩니다. 5% 이내면 rendezvous
  단가가 이미 충분히 낮다는 뜻이므로, 다음 축은 생략이 아니라 **command batching**입니다.

`Win32 Glide ordinal timing: ordinal=` 줄에서 두 텍스처 setter의 work/call도 함께
남겨 주시면 batching의 단가 근거가 됩니다.

## 6단계 — draw batching A/B — Task 438 · **완료, 기본값 승격됨**

> **결과(2026-08-07).** 배치 평균 **16.02**(최대 332), flush 167,133건 전부
> non-draw-gate, **glide-gate 비중 10.35% → 8.40%**, 크로싱당 −23.7%,
> `failures`·`voided`·구현 공백 0, 시각 차이 없음. Task 439에서 **기본값 켜짐**으로
> 승격했습니다. 아래 절차는 `=0` 대조군으로 재현할 때 씁니다.

**이 축이 지금 가장 큽니다.** 실부하 구간에서 `grDrawTriangle`은 프레임당 652~686회이고
크로싱의 **69.9%** 이며, 삼각형마다 host 왕복이 한 번씩 듭니다. Task 438은 삼각형을
모았다가 **draw가 아닌 게이트를 만나면 한 번에** 넘깁니다.

**5단계 A/B에서 vsync 때문에 프레임 판정을 못 했으므로, 이번에는 세 변수를 반드시
켭니다.**

```
:: 공통 (양쪽 동일)
set REPIU_EXECUTION_BACKEND=dynamic
set REPIU_EXECUTION_TIMEOUT_MS=0
set REPIU_GLIDE_SWAP_INTERVAL=0
set REPIU_EXECUTION_TIME_PROFILE=1
set REPIU_GLIDE_ORDINAL_TIME_PROFILE=1
set REPIU_GLIDE_SETTER_ELIDE_TEXTURE=1

:: A — batching 없음
set REPIU_GLIDE_DRAW_BATCH=0
build\win32_x86_debug\Release\repiu.exe pumpit1 2> batch-off.log

:: B — batching
set REPIU_GLIDE_DRAW_BATCH=1
build\win32_x86_debug\Release\repiu.exe pumpit1 2> batch-on.log
```

> `REPIU_GLIDE_SWAP_INTERVAL=0`이 빠지면 60 Hz 양자화 때문에 CPU 개선이 프레임에
> 나타나지 않습니다. **5단계 A/B가 그래서 판정 불가로 끝났습니다.**

**같은 구간을 각 3회 이상**, 가능하면 같은 곡·난이도·길이로 플레이하십시오.

읽을 줄은 다음 두 개가 추가됩니다.

```
Win32 Glide draw batch enabled/queued/drawn/flushes/failures/max-batch/pending/mean-batch
Win32 Glide draw batch flush reason non-draw-gate/primitive-change/capacity
```

> **바이너리를 먼저 확인하십시오.** 두 실행 모두 `Win32 Glide draw batch ...` 줄이
> 있어야 합니다. 없으면 배치 코드가 없는 빌드입니다 — `scripts\build_win32_x86.ps1
> -Configuration Release`로 다시 빌드하십시오. **1차 A/B가 이것 때문에 무효였습니다.**

**판정 순서.**

**0. `mean-batch`를 먼저 보십시오. 이 값이 이 축의 운명입니다.**
`=1` 로그의 `mean-batch`가 **2 근처면 이 축은 닫힙니다** — 왕복이 절반만 줄고 그 이득이
큐 비용과 상쇄되는 것을 attract 구간에서 이미 확인했습니다(총 게이트 비용 변화 없음).
gameplay 로그로 계산한 상한은 **5.44**이며, 실제 값이 4를 넘어야 의미가 있습니다.

**1. 정확성:** `failures` **0**, `primitives-queued == primitives-drawn + pending`,
`_GRDRAWTRIANGLE@12 count`가 두 구성에서 비슷할 것(같은 구간이라면).

**2. 시각:** 겹침 순서와 반투명 — 순서가 깨지면 **뒤에 있어야 할 것이 앞에** 오거나
블렌딩 결과가 달라집니다.

**3. 성능 — 프레임이 아니라 cycle로 보십시오.** 기대 효과는 **guest-run의 2~3%**
(draw ordinal이 guest-run의 6.55%, 그중 왕복이 51%)인데, 같은 구성으로 돌린 두 실행이
**13% 차이**난 적이 있어 프레임으로는 이 크기를 분해할 수 없습니다. 대신:

```
Win32 execution time cycles guest-run/veh/glide-gate/port-io/dos:
Win32 Glide ordinal timing: ordinal=73 ...
```

* **`glide-gate` cycle을 `guest-run` cycle로 나눈 비중**이 내려가야 합니다(11.0% → 8%대 기대).
  이것이 유일하게 구간 길이에 둔감한 지표입니다.
* ordinal 73의 `rendezvous`가 draw 수보다 **훨씬 작아야** 합니다.
* `work`는 거의 그대로여야 합니다 — 같은 GL 작업을 하는 것이므로.

**두 구성 다 3회 이상** 돌리고 중앙값을 쓰십시오. 승격은 gate 비중이 내려가고 시각
회귀가 없을 때이며, 그때 Task 437도 함께 승격합니다.

---

## English

This guide lets you validate the exact-state Glide setter elision that Task 365
enabled by default, in a real gameplay scene, and decide whether the default
should stay on.

**Why your test matters.** The automated 60-second measurement starts from boot
and includes the LFB section. There, correctness was proven and the Glide gate
cost fell 5.13 points (41,368 rendezvous removed), but frames did not move
(1,215 to 1,206, inside run variance). In the gameplay scene behind the original
"FPS collapses when call volume rises" report, Task 363 measured state setters at
85.33% of the Glide gate and 20.59% of wall time, so the outcome could differ —
and only you can capture that scene.

**Procedure.** Play the same section twice with identical settings, once with
`REPIU_GLIDE_SETTER_ELIDE=1` and once with `=0`, keeping
`REPIU_EXECUTION_BACKEND=dynamic`, `REPIU_EXECUTION_TIME_PROFILE=1`,
`REPIU_GLIDE_ORDINAL_TIME_PROFILE=1`, and `REPIU_GLIDE_SETTER_CENSUS=1` the same
in both, capturing stderr to a file. Run each configuration at least three times
and use medians: run-to-run frame variance reaches 18% and the first run of a
configuration is always the slowest.

**Read four lines** from each log: the `_GRBUFFERSWAP@4 count=` frame count, the
`execution time share` glide-gate percentage, the `setter elision` summary, and
the `setter census` summary.

**Judge correctness first.** The census is a behaviour-neutral observer, so its
`same` count must equal the elision's `elided` count — that is what proves only
confirmed duplicates were skipped. If they differ, or if `voided` is nonzero, set
`REPIU_GLIDE_SETTER_ELIDE=0` and send the log: it means the rules are missing a
boundary that this scene exercises. Then compare the two runs visually, watching
translucent effects, screen clipping, color channels, and front-to-back ordering,
since those are the states being cached.

**Then judge performance.** If median frames improve by more than 5%, the scene
was limited by this path: keep the default and batch two is worth doing. Within
5%, this scene is not limited by it either, batch two stops, and the default
becomes a preference. A regression beyond 5% is unexpected and worth reporting.
The glide-gate share will fall either way, since the cost reduction is real; the
deciding number is frames.

**Also useful:** capture the `INT 8 chain HLE count` line. Whether a real gameplay
scene delivers timer ticks at the same rate as the boot scene is an open question
that Task 366 is working on, and your log would feed straight into it.

## Step five — the batch-two (texture-state) A/B, Task 437

Batch one's seven gates are **already elided 99.999% of the time** — 2,048,762 elided against 22
applied over a measured 371-second gameplay run. The largest remaining group is the texture-state
block the game issues once per bind: `grTexClampMode`, `grTexFilterMode` and `grTexMipMapMode` at
395,764 calls each, 19.6 per frame. Task 437 adds those three as an **opt-in**, keeping
`grTexSource` out because its `GrTexInfo*` argument means an identical key does not prove
identical state. Two of the three cost a host rendezvous, so the prize is **791,528 rendezvous,
39.2 per frame**; `grTexMipMapMode` is an ABI-only no-op and saves nothing.

Run the same section at least three times per configuration with everything from step one
unchanged, switching only `REPIU_GLIDE_SETTER_ELIDE_TEXTURE` between `0` and `1`. The elision
summary now carries a **`texture-state`** field, so each log states which configuration produced
it.

Judge in the same order. **Correctness:** the increase in `elided` must match the `same` total of
the three texture gates, and `voided` must stay zero. **Visually:** watch texture edges (repeat
versus clamp) and magnified sprites (linear versus nearest) — a wrongly elided clamp or filter
shows up as smeared borders or a changed filter, not as a missing object. **Performance:** a
median frame difference beyond 5% is the case for promoting the default; within 5% the rendezvous
unit cost is already low enough that the next axis is **command batching** rather than more
elision. Capturing the `Win32 Glide ordinal timing: ordinal=` lines for the two texture setters
gives that batching decision its unit cost.

## Step six — the draw batching A/B, Task 438

> **Settled on 2026-08-07 and promoted to the default in Task 439:** batches averaged **16.02**
> primitives (peak 332), all 167,133 flushes came from the non-draw-gate rule, the Glide gate
> fell from **10.35% to 8.40% of guest-run** with 23.7% off the per-crossing cost, and
> failures, voided setters, implementation gaps and visual differences were all zero. The
> procedure below is how to reproduce it against the `=0` control.

**This is now the largest axis.** Under real load `grDrawTriangle` runs 652-686 times per frame,
**69.9% of all gate crossings**, and each triangle costs its own host round trip. Task 438 queues
them and hands the queue over **once, whenever a non-draw gate arrives**.

Because step five's A/B could not judge frames with vsync on, **three variables are mandatory
here**: `REPIU_GLIDE_SWAP_INTERVAL=0`, `REPIU_EXECUTION_TIME_PROFILE=1` and
`REPIU_GLIDE_ORDINAL_TIME_PROFILE=1`, identical in both runs, with
`REPIU_GLIDE_SETTER_ELIDE_TEXTURE=1` in both so the only difference is
`REPIU_GLIDE_DRAW_BATCH`. Play the same section at least three times per configuration.

Two new lines appear:

```
Win32 Glide draw batch enabled/queued/drawn/flushes/failures/max-batch/pending/mean-batch
Win32 Glide draw batch flush reason non-draw-gate/primitive-change/capacity
```

**Check the binary first:** both logs must contain a `Win32 Glide draw batch ...` line. Its
absence means the build has no batching code — rebuild with
`scriptsuild_win32_x86.ps1 -Configuration Release`. **The first A/B was void for exactly this
reason.**

**Read `mean-batch` before anything else, because it decides the axis.** Near two and the axis is
closed: only half the round trips disappear and the queueing cost offsets them, which the attract
section already demonstrated with no change in total gate cycles. The gameplay logs bound it at
**5.44**, and it needs to come back above four to matter.

Then correctness — `failures` zero, `primitives-queued` equal to `primitives-drawn` plus
`pending`, comparable `_GRDRAWTRIANGLE@12 count` — and then the picture, watching overlap order
and translucency, since a broken order puts what belongs behind in front.

**Judge performance in cycles, not frames.** The expected effect is **2-3% of guest-run** (the
draw ordinal is 6.55% of it and the round trip is 51% of that), while two runs of the *same*
configuration have differed by **13%**, so frames cannot resolve it. Use the ratio of
`glide-gate` to `guest-run` cycles from the `execution time cycles` line — the one figure that is
insensitive to how long the section ran — expecting 11.0% to fall into the eights; ordinal 73's
`rendezvous` far below the draw count; and `work` essentially unchanged, since it is the same GL
work either way. Three runs per configuration, medians, and Task 437 is promoted alongside if the
gate share falls with no visual regression.
