# 작업 기록 20260903-576 — 엔진에 long-mode 방출 연결

설계: [20260903-576](../design/20260903-576-engine-long-mode-emission.md) ·
작업 지시: [20260903-576](../work-orders/20260903-576-engine-long-mode-emission.md)

## 구현

`HostRequiresLongModeEmission()`을 runtime에 두고, 로더가 그것으로
`aot_build_options.enable_long_mode_emission`을 설정합니다. env toggle이 아닌
이유는 설계의 근거 그대로입니다 — x86-64 host에서 이 플래그가 꺼진 이미지는
다른 선택지가 아니라 **틀린 이미지**입니다.

판정은 `sizeof(void*)`가 아니라 `__x86_64__`/`_M_X64`입니다. 묻는 질문이
"CPU가 이 바이트를 long mode로 디코드하는가"이고, x32 ABI처럼 pointer가
4바이트이면서 CPU는 long mode인 구성에서 pointer 크기는 틀린 답을 냅니다.

배선은 다른 옵션들과 같은 경로를 따릅니다. `BuildAotCodeCacheImage`가 이미
`image->long_mode_emission_enabled`에 기록하고 있었으므로, placement가 그것을
물려받고 dynamic append가 되읽는 두 홉만 더했습니다. **append가 host를 다시 묻지
않는 것**이 요점입니다 — 정적 캐시와 나중에 붙는 block이 서로 다른 종류의
이미지가 되는 것을 막습니다.

## 결과 — 예측이 맞았고, 정지 지점이 두 단계 앞으로 갔습니다

Task 575와 **같은 ROM 세트(`pumpit2a`)**로 비교했습니다.

| | Task 575 | Task 576 |
|---|---|---|
| timer safe point site | (i386 이미지, site 존재) | **`enabled/sites: true/0`** |
| AOT code cache 배치 | **실패** — `timer safe-point request is unavailable` | **성공** |
| 도달한 지점 | AOT 배치 | **guest entry** |
| 정지 메시지 | `Failed to place requested AOT code cache` | `minimal original entry execution requires a 32-bit host` |
| 종료 코드 | 1 | **0** |

`sites: true/0`이 설계 예측 1·2를 그대로 보입니다 — timer safe point는 요청되어
있지만 long-mode 이미지가 하나도 내지 않으므로,
`ResolveAotTimerSafePoints`가 즉시 성공하고 Task 575를 막던 오류가 사라집니다.

**이제 x64 실행이 Task 544의 울타리에 닿습니다.** 그것이 3.20절 표의 항목 3이고,
지금까지는 그 앞에서 멈춰 한 번도 도달하지 못했습니다. 종료 코드도 1에서 0으로
바뀌었습니다 — 실패로 빠져나가는 것이 아니라 거절을 보고하고 런처로 정상 복귀
합니다.

## 정정 — Task 571이 validator에 대해 적은 것이 불완전했습니다

Task 571의 작업 로그는 `ValidateAotCodeCacheHleCoverage`를 위험으로 적으면서
"이 함수는 Win32 전용 `repiu_aot_probe`에서만 호출된다"고 했습니다.
**그렇지 않습니다** — 엔진의 dynamic append 경로도 호출합니다
(`src/engine/aot_code_cache.cpp`).

그 함수는 i386 바이트 배치를 그대로 검사하고 long-mode 인식이 없으므로,
long-mode 이미지에는 실패합니다. 이번 변경이 그것에 막히지 않은 이유는 하나뿐
입니다 — **정적 배치 경로는 그 함수를 호출하지 않습니다.**

따라서 이것은 **다음 blocker이지 이번 것이 아닙니다.** dynamic append는 게스트가
도는 중에만 실행되고 x64는 guest entry가 닫혀 있어 아직 도달하지 않습니다. 여기서
함께 고치지 않은 이유는 설계 결정 3에 있습니다 — 자기 probe를 갖는 별도 단위여야
하고, 섞으면 이번 단위가 무엇을 바꿨는지가 흐려집니다.

## 검증

| 항목 | 결과 |
|---|---|
| x64 `repiu` 실행 (`pumpit2a`) | 배치 성공, guest entry 울타리 도달, exit 0 |
| x64 `repiu` 실행 (`pumpipx3`) | 같음 |
| Linux i386 `repiu` 링크 | 성공 |
| Linux i386 `repiu_core_probe` | 19/19, failures 0 |
| Linux x64 `repiu_core_probe` | 20/20, failures 0 |
| Linux x64 census | **불변** — emittable 73,689... 아래 표 참조 |
| Win32 `repiu_aot_probe` | `_all=true` 41개, `_all=false` 0개 |

census가 바뀌지 않아야 하는 이유는 census가 플래그를 스스로 켜기 때문입니다.
실제로 그대로입니다.

```text
emittable 73748 (99.21%)  refused 585  blocks complete 15646 (90.09%)
reachable blocks 7723 (44.47%)  agrees=true
```

i386과 Win32는 `HostRequiresLongModeEmission()`이 false이므로 이미지 구성이
바뀌지 않습니다. 두 회귀가 그것을 확인합니다.

## 남은 것

3.20절 표가 이렇게 됩니다.

| # | 항목 | 상태 |
|---|---|---|
| 0 | 엔진이 long-mode 방출을 켜는 것 | **해결** |
| 1 | 심볼 두 개 | 해결 (575) |
| 2 | fault 경로 `Eip`/`Esp` 의미 | 미해결 |
| 3 | guest entry (`return 4`) | **이제 여기까지 도달** |
| 4 | dispatch thunk 5개 | 불필요 |
| 5 | `ValidateAotCodeCacheHleCoverage`의 i386 전제 | **새로 확인됨** |

항목 3이 다음 순서로 보이지만, 그것을 열면 항목 2가 즉시 필요해집니다 — 게스트가
돌기 시작하면 첫 boundary에서 fault 경로가 틀린 `Eip`를 보게 됩니다.

## 아직 확인하지 않음

- 항목 0을 켠 뒤 **dynamic append**가 어떻게 되는지 재지 않았습니다. 게스트가
  돌지 않으므로 도달하지 않지만, 항목 5가 그 자리에서 기다리고 있습니다.
- long-mode 이미지의 배치 이후 resolver들(jump table, segment override, inline
  cache)이 빈 site 목록을 어떻게 다루는지 개별로 재지 않았습니다. 배치가 성공했
  으므로 막지는 않습니다.

---

# Work log 20260903-576 — Wiring long-mode emission into the engine

Design: [20260903-576](../design/20260903-576-engine-long-mode-emission.md) ·
work order: [20260903-576](../work-orders/20260903-576-engine-long-mode-emission.md)

## Implementation

`HostRequiresLongModeEmission()` lives in the runtime, and the loader sets
`aot_build_options.enable_long_mode_emission` from it. Not an environment toggle,
for the design's reason: on an x86-64 host an image built with the flag off is a
**wrong image**, not an alternative one.

The test is `__x86_64__`/`_M_X64` rather than `sizeof(void*)`, because the
question is "will the CPU decode these bytes in long mode", and under the x32
ABI — four-byte pointers with the CPU still in long mode — pointer size answers
it wrongly.

The wiring follows the path every other option takes. `BuildAotCodeCacheImage`
already recorded `image->long_mode_emission_enabled`, so only two hops were
added: the placement inherits it and the dynamic-append path reads it back. The
point is that **append does not ask the host again**, which is what would let the
static cache and the blocks appended later become different kinds of image.

## Result — the prediction held, and the stopping point moved two stages on

Compared against Task 575 on the **same ROM set (`pumpit2a`)**:

| | Task 575 | Task 576 |
|---|---|---|
| Timer safe-point sites | (i386 image, sites present) | **`enabled/sites: true/0`** |
| AOT code cache placement | **failed** — `timer safe-point request is unavailable` | **succeeds** |
| Stage reached | AOT placement | **guest entry** |
| Stopping message | `Failed to place requested AOT code cache` | `minimal original entry execution requires a 32-bit host` |
| Exit code | 1 | **0** |

`sites: true/0` is design predictions 1 and 2 exactly: timer safe points are
requested, a long-mode image emits none, so `ResolveAotTimerSafePoints` succeeds
at once and the error that blocked Task 575 is gone.

**The x64 run now reaches Task 544's fence.** That is item 3 of section 3.20's
table, and until now execution stopped short of ever reaching it. The exit code
also moved from 1 to 0 — it reports the refusal and returns to the launcher
rather than bailing out as a failure.

## Correction — what Task 571 wrote about the validator was incomplete

Task 571's work log recorded `ValidateAotCodeCacheHleCoverage` as a hazard while
stating that "this function is called only from the Win32-only
`repiu_aot_probe`". **It is not**: the engine's dynamic-append path calls it too
(`src/engine/aot_code_cache.cpp`).

That function checks i386 byte placement literally and has no long-mode
awareness, so it fails on a long-mode image. There is exactly one reason this
change was not blocked by it: **the static placement path does not call it.**

So it is **the next blocker, not this one**. Dynamic append runs only while a
guest runs, and x64's guest entry is closed, so it is not reached yet. Design
decision 3 records why it was not fixed alongside: it needs its own probe, and
mixing it in would blur what this unit changed.

## Verification

| Item | Result |
|---|---|
| x64 `repiu` run (`pumpit2a`) | placement succeeds, reaches the guest-entry fence, exit 0 |
| x64 `repiu` run (`pumpipx3`) | same |
| Linux i386 `repiu` link | succeeds |
| Linux i386 `repiu_core_probe` | 19/19, 0 failures |
| Linux x64 `repiu_core_probe` | 20/20, 0 failures |
| Linux x64 census | **unchanged** — see below |
| Win32 `repiu_aot_probe` | 41 `_all=true`, 0 `_all=false` |

The census must not move because it sets the flag itself, and it does not:

```text
emittable 73748 (99.21%)  refused 585  blocks complete 15646 (90.09%)
reachable blocks 7723 (44.47%)  agrees=true
```

On i386 and Win32 `HostRequiresLongModeEmission()` is false, so image
composition does not change; the two regressions confirm it.

## What is left

Section 3.20's table now reads:

| # | Item | Status |
|---|---|---|
| 0 | The engine enabling long-mode emission | **done** |
| 1 | Two symbols | done (575) |
| 2 | The fault path's `Eip`/`Esp` meaning | open |
| 3 | Guest entry (`return 4`) | **now reached** |
| 4 | The five dispatch thunks | not needed |
| 5 | `ValidateAotCodeCacheHleCoverage`'s i386 assumption | **newly confirmed** |

Item 3 looks like the next step, but opening it makes item 2 immediately
necessary: once a guest starts running, the first boundary puts the fault path in
front of a wrong `Eip`.

## Not yet verified

- What **dynamic append** does now that item 0 is on. It is not reached because
  no guest runs, but item 5 is waiting there.
- Whether the post-placement resolvers (jump tables, segment overrides, inline
  caches) handle empty site lists individually was not measured separately.
  Placement succeeded, so none of them blocks.
