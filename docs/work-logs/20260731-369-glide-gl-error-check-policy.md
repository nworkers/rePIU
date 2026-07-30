# 작업 로그: Glide setter GL 에러 체크 정책 / Work log: Glide setter GL error-check policy

Task 369. 설계 [20260731-369](../design/20260731-369-glide-gl-error-check-policy.md),
작업 지시 [20260731-369](../work-orders/20260731-369-glide-gl-error-check-policy.md)

## 한국어

### 발단

Tasks 364~368이 전부 자동 부팅 장면에서 측정돼 세 번 연속 "이 장면에서는 이득이
작다"를 냈습니다. 사용자가 실제 gameplay 장면을 3회 캡처했고, 그중 3차에
`REPIU_GLIDE_SETTER_PHASE=1`을 넣어 `grDepthMask` 본체를 분리했습니다.

1차·2차 캡처에서 제가 세운 "`glGetError`가 범인" 가설을 3차가 확정했습니다.

### 원인

| 구간 | 총 cycles | 호출당 | 비율 |
|---|---:|---:|---:|
| `glDepthMask()` | 22,576,956 | 911 | 0.19% |
| `glGetError()` | 12,172,660,110 | **491,356** | **99.81%** |

24,774회 호출, `errors=0`, `max-error=53,722,433` cycle(14.55 ms). 독립 계측인
ordinal `work`(12,222,557,126)와 99.78% 일치. wall의 6.24%, Glide gate의 38.1%.

`glGetError`는 OpenGL 비동기 명령 스트림의 **동기화 지점**이라 비용이 앞에 쌓인
명령량에 비례합니다. 게다가 컨텍스트 단위 플래그이므로 값이 0이 아니어도 어느
호출이 실패했는지 지목하지 못합니다.

### 구현

* 신규 `glide_gl_error_policy.{h,cpp}` — `REPIU_GLIDE_GL_ERROR_CHECK`, 기본 OFF.
  resolver는 `1`/`on`/`true`만 수용.
* backend에 resolve-once 멤버와 `CheckGlErrorIfEnabled()` 추가. OFF면 `glGetError`를
  **호출하지 않습니다**(분기만 건너뛰는 것으로는 의미가 없음).
* setter 13개소 게이트 적용. `SetAlphaBlend`의 선행 drain 루프도 같은 정책으로
  묶었습니다(phase 계측이 호출당 62,653 cycle로 잰 구간).
* `StoreTexture`, `PresentLfbSurface`, `ReadbackFramebuffer` 3개소는 제외.
* `BufferSwapOnHostThread`에서 present **직후** 프레임당 1회 drain·기록. swap이
  이미 동기화 지점이라 추가 flush가 없고, GL 에러 플래그는 조회될 때까지 남으므로
  해당 프레임 에러를 놓치지 않습니다. 무한 루프 방지 상한 32회.
* Task 364 phase 타임스탬프는 그대로 유지 — OFF에서 error 구간이 0으로 수렴하는 것이
  적용 증거가 됩니다.
* 요약 한 줄 추가(무조건 출력):
  `Win32 Glide GL error policy per-call-check/frame-checks/frame-errors/first-code/drain-iterations`

### 검증

* VS2022 Win32 x86 **Debug/Release 양쪽 빌드 성공**(신규 C4819 없음, 기존 코드페이지
  경고만).
* `repiu_aot_probe.exe MASTER\PIU_1ST\PIU\PIU.EXE`: Debug/Release 모두 **exit 0**,
  신규 probe 6개 항목 전부 true. resolver는 `1 `(후행 공백)과 `TRUE`를 거부하는
  것까지 고정했습니다 — 실제 측정에서 사용자가 `set VAR=1 && ...`의 공백 때문에
  phase 계측을 한 번 놓쳤기 때문입니다.
* **동일 장면 A/B(자동 부팅, 70초 고정, 동일 빌드, wall 259.18e9 cycle 동일):**

| 지표 | OFF (기본) | ON (기존 동작) |
|---|---:|---:|
| 프레임 | 2,429 | 2,223 |
| depth-mask apply/call | 950.8 cy | 911.2 cy |
| **depth-mask error/call** | **71.6 cy** | **4,600 cy** |
| `grDepthMask` gate/call | 49,916 cy | 58,157 cy |
| `grDepthMask` work/call | 2,026 cy | 6,574 cy |
| frame-checks / frame-errors | 2,429 / **0** | 2,223 / **0** |
| VEH / Glide gate | 38.47% / 29.32% | 34.20% / 25.33% |

`glGetError` 호출이 실제로 사라졌고(error 구간 −98.4%), 프레임 검사 2,429회 전부
에러 0이므로 **아무 에러도 가려지지 않았습니다.**

### 정직한 한계

**이 A/B는 이득을 판정하지 못합니다.** 자동 장면에서 `glGetError`는 호출당 4,600
cycle(총 wall의 0.06%)뿐이라 gameplay 장면의 491,356 cycle(6.24%)과 **107배**
차이입니다. 프레임 +9.3%는 이 장면의 알려진 편차 범위이므로 이득으로 주장하지
않습니다.

즉 이 A/B가 증명한 것은 **메커니즘과 안전성**이고, **효과 크기는 gameplay 장면
재캡처로만 판정됩니다.**

`grBufferSwap` work가 호출당 17.3M → 21.4M cycle로 늘었습니다. 다만 그 증가분
(약 13.4e9 cycle)은 제거한 `glGetError` 총량(0.16e9)의 **84배**이므로 "비용이
present로 이동했다"로 설명되지 않고, 프레임 수 증가에 따른 장면 동역학으로 봅니다.
gameplay 장면에서는 별도로 확인해야 합니다.

### gameplay 장면 검증 (2026-07-31, 사용자 캡처) — **판정 완료**

작업 로그 작성 시점에 "남은 것 1"로 미뤘던 gameplay 재캡처를 사용자가 수행했습니다.
64.5초 / 1,788프레임, 정책 기본 OFF.

**제거는 예측대로입니다.**

| 지표 | 이전(3차, 체크 ON) | 이번(OFF) |
|---|---:|---:|
| depth-mask `error`/call | 491,356 cy | **68.2 cy** |
| depth-mask `max-error` | 53,722,433 cy | **888 cy** |
| `grDepthMask` gate/call | 542,835 cy | **53,204 cy** |
| `grDepthMask` wall 비중 | 6.89% | **0.76%** |

**6.13%p 회수**로 설계 예상치 6.24%와 일치합니다. frame-checks 1,788회 전부
`frame-errors=0`, 생략 정확성도 유지(census `same` 220,888 = `elided` 220,888,
`voided=0`)입니다.

**이동도 설계가 경고한 그대로 일어났습니다.**

| 지표 | 이전 | 이번 |
|---|---:|---:|
| `grBufferSwap` work/call | 212,582 cy | **6,220,464 cy** |
| `grBufferSwap` work 총량 | 0.28e9 | **11.12e9** |
| 전체 host GL work | 16.79e9 | 16.62e9 (거의 불변) |

`grDepthMask`에서 사라진 12.16e9 중 **약 10.8e9(89%)가 `grBufferSwap`으로
이동**했습니다. 드라이버 작업은 실재하며 사라지지 않고 다음 동기화 지점으로
옮겨갑니다. `grBufferSwap`이 이제 단일 최대 Glide ordinal(wall의 4.72%)입니다.

**그럼에도 순이득이 있습니다.** 장면 구성이 2% 이내로 일치하므로(삼각형/프레임
85.8 → 84.0, rendezvous/프레임 224.5 → 220.8) 프레임당 정규화가 성립합니다.

| 프레임당 | 이전 | 이번 | 변화 |
|---|---:|---:|---:|
| wall | 39.85 ms | 36.05 ms | **−9.6%** |
| Glide gate | 24,066,213 cy | 20,801,917 cy | −13.6% |
| host GL work | 12,658,735 cy | 9,293,465 cy | **−26.6%** |

같은 양을 렌더하면서 host GL work가 프레임당 26.6% 줄었습니다. 이동이 아니라 실제
감소이며, 프레임당 19회 강제 배수가 1회로 줄어 드라이버가 배치를 모을 수 있게 된
결과로 읽힙니다.

**fps 주장은 제한합니다.** 25.09 → 27.73 fps(+10.5%)이지만 사전 기준선 3회가
26.8 / 27.2 / 25.1(편차 8.4%)이므로 기준선 최고치 대비로는 +2.0%입니다. 단일
실행으로 +10.5%를 주장할 수 없으며, 방어 가능한 진술은 "기준선 범위 상단 이상"과
"프레임당 wall −9.6%"입니다. 후자가 장면 일치 덕분에 신뢰도가 높습니다.

**다음 측정이 이미 정해집니다.** `grBufferSwap`의 6,220,464 cycle이 실제 GPU 작업
배수인지 Task 369가 추가한 프레임당 검사인지 갈라야 합니다. 새 계측은 필요
없습니다 — 기존 swap 계측의 `present_end → accounting_end` 구간이 정확히 그
검사를 포함하므로 `REPIU_GLIDE_SWAP_TIME_PROFILE=1` 캡처 한 번으로 분리됩니다.
크면 검사를 present 이전으로 옮기거나 N프레임 주기로 낮추고, 작으면 남은 것은
진짜 GPU 작업이므로 Glide 축을 닫습니다.

### 문서

* 신규 [docs/analysis/glide-gate-cost-attribution.md](../analysis/glide-gate-cost-attribution.md)
  — 3회 캡처, gate 분해, `glGetError` 귀속, 생략 상한 순위, 미계측 영역.
* [docs/analysis/README.md](../analysis/README.md) 색인 갱신.
* [docs/analysis/glide2x-ovl-and-opengl-hle.md](../analysis/glide2x-ovl-and-opengl-hle.md)
  — Task 364의 "`glGetError` 전역 제거 기각" 결론을 **철회**하는 note 삽입(한/영).
  측정은 옳았으나 자동 장면에만 해당했습니다.
* [docs/analysis/current-execution-frontier.md](../analysis/current-execution-frontier.md)
  — Task 369 절 추가.

### 남은 것

1. ~~gameplay 장면 재캡처~~ — **완료**(위 검증 절). 후속은
   `REPIU_GLIDE_SWAP_TIME_PROFILE=1` 캡처로 `grBufferSwap` 6.22M cycle을 present
   본체와 프레임 검사로 분리하는 것입니다.
2. Task 365 batch 2(`grDepthMask` 생략)는 재캡처 후 판단. 본 변경으로 상한이
   wall 6.89% → 약 0.55%로 붕괴했을 것으로 예상됩니다.
3. rendezvous 왕복 11.8 µs × 약 30만 회(`direct` 경로 미사용).
4. **커널 예외 전달 비용 계측** — VEH 버킷이 핸들러 진입~퇴출만 재므로 프레임당
   861~972개 예외의 커널 왕복이 `unaccounted`에 숨어 있습니다. Task 368의 예외 축
   종결 판정 재검토 포함.
5. fog / combine setter의 GLSL uniform 경로 31~36k cycle(wall 약 0.9%).

## English

### Cause

With `REPIU_GLIDE_SETTER_PHASE=1` on a user gameplay capture, `grDepthMask` split
into 911 cycles of `glDepthMask` and 491,356 cycles of the trailing `glGetError` —
99.81% of the cost across 24,774 calls that never reported an error, worst case
14.55 ms, agreeing with the independent per-ordinal `work` counter to 99.78%. That
is 6.24% of wall time and 38.1% of the Glide gate. `glGetError` is a
synchronisation point in an asynchronous command stream, so its cost tracks what is
queued ahead of it, and its context-wide flag cannot name the failing call anyway.

### Implementation

A new `glide_gl_error_policy` module gates the per-call check behind
`REPIU_GLIDE_GL_ERROR_CHECK` (default off, resolver accepting only `1`, `on`,
`true`). The backend gained a resolve-once member and a `CheckGlErrorIfEnabled()`
helper that does not call `glGetError` at all when disabled. Thirteen setter sites
are gated, including the leading drain loop in `SetAlphaBlend`; `StoreTexture`,
`PresentLfbSurface`, and `ReadbackFramebuffer` are deliberately untouched. A single
drain runs immediately after `SDL_GL_SwapWindow`, where the swap has already
synchronised and the error flag still holds anything the frame raised, bounded at
32 iterations. Task 364's phase timestamps stay in place so the collapsing error
interval proves the change took effect, and one unconditional summary line reports
the policy and the frame-check results.

### Verification

Debug and Release both build. `repiu_aot_probe` exits 0 in both configurations with
all six new assertions true, including the pinned rejection of `1 ` with a trailing
space — the exact failure that silently disabled the phase instrument during
measurement. A same-scene, same-build, 70-second A/B over an identical 259.18e9
cycle wall shows the depth-mask error interval falling from 4,600 to 71.6 cycles
per call, `grDepthMask` work from 6,574 to 2,026, and zero errors across 2,429
frame checks, so nothing is being masked.

### Honest limits

This A/B proves the mechanism and the safety, not the size of the win. In the
automated scene `glGetError` costs 4,600 cycles per call — 0.06% of wall against
6.24% in gameplay, a 107-fold scene difference — so the +9.3% frame delta sits
inside this scene's known run variance and is not claimed as the gain. Only a
gameplay re-capture can decide that. `grBufferSwap` work rose from 17.3M to 21.4M
cycles per call, but that increase is 84 times larger than the total `glGetError`
cost removed, so it is scene dynamics from the higher frame count rather than cost
migrating to the present; the gameplay scene needs its own check.

### Gameplay verification (2026-07-31, user capture) — decided

The gameplay re-capture this log had deferred was taken: 64.5 seconds, 1,788
frames, policy at its default. The removal matches the prediction. The depth-mask
error interval fell from 491,356 to **68.2 cycles per call** and its worst case
from 53,722,433 to **888**, `grDepthMask` fell from 542,835 to 53,204 cycles per
call, and its share of wall from 6.89% to **0.76%** — **6.13 percentage points
recovered** against the 6.24% predicted. All 1,788 frame checks reported zero
errors, and elision correctness held with census `same` 220,888 matching elision
`elided` 220,888 at zero voided.

The migration the design warned about also happened. `grBufferSwap` work rose from
212,582 to **6,220,464 cycles per call**, absorbing about 10.8e9 of the 12.16e9
removed, while total host GL work barely moved (16.79e9 to 16.62e9). The driver
work is real and relocates to the next synchronisation point; `grBufferSwap` is now
the single largest Glide ordinal at 4.72% of wall.

A net gain remains. Scene composition matches within 2% (85.8 to 84.0 triangles per
frame, 224.5 to 220.8 rendezvous per frame), so per-frame normalisation holds: wall
per frame fell 39.85 to 36.05 ms (**-9.6%**), the Glide gate 13.6%, and host GL work
**26.6%** — a real reduction rather than a relocation, consistent with the driver
being able to batch once the forced drain dropped from nineteen times a frame to
once. The fps claim is limited on purpose: 25.09 to 27.73 is +10.5%, but the
three-run baseline spanned 25.1 to 27.2 at 8.4% variance, so against its top the
gain is +2.0%. The defensible statements are "at or above the top of the baseline
range" and the per-frame -9.6%, the latter carrying more weight because the scenes
match.

The next measurement follows without new instrumentation: whether `grBufferSwap`'s
6,220,464 cycles are genuine deferred GPU work or the per-frame check this task
added. The existing swap instrument already times `present_end` to
`accounting_end`, which is exactly the interval containing that check, so one
`REPIU_GLIDE_SWAP_TIME_PROFILE=1` capture separates them.

### Next

The gameplay re-capture above decided the change. Task 365 batch two is sequenced
after it, since this change should have collapsed the `grDepthMask` elision ceiling
from 6.89% of wall to roughly 0.55%. The remaining axes are the 11.8 µs rendezvous
round trip across roughly 300,000 calls, the unmeasured kernel exception-delivery
cost hiding inside "unaccounted" at 861-972 exceptions per frame — which reopens
Task 368's closure — and the GLSL uniform path on the fog and combine setters at
about 0.9% of wall.
