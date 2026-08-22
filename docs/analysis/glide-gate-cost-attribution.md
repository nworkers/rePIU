# Glide gate 비용 귀속 / Glide gate cost attribution

Glide gate 시간이 어디로 가는지에 대한 누적 문서입니다. Tasks 353~369의 계측
결과와, 2026-07-30 ~ 07-31에 사용자가 직접 캡처한 **실제 gameplay 장면 3회**를
근거로 합니다.

관련: [glide2x-ovl-and-opengl-hle.md](glide2x-ovl-and-opengl-hle.md),
[current-execution-frontier.md](current-execution-frontier.md),
[설계 369](../design/20260731-369-glide-gl-error-check-policy.md)

## 한국어

### 1. 장면이 결론을 바꾼다 — **확인됨**

같은 setter 집합의 wall 비중이 장면에 따라 4배 이상 달라집니다.

| 장면 | 상태 setter의 wall 비중 | `grLfbLock` | 출처 |
|---|---:|---:|---|
| Task 363 gameplay | 20.59% | 0회 | 수동 47.5초 |
| 사용자 gameplay 캡처 3회 | 11.5 ~ 12.6% | 0회 | 36.0 / 54.7 / 52.8초 |
| 자동 부팅 포함 장면 | 약 5.6% | 304회 | 60초 |

Tasks 364~368이 전부 자동 장면에서 측정돼 "이 장면에서는 이득이 작다"를 세 번
반복한 원인입니다. **자동 장면 단독으로는 최적화 대상을 선정할 수 없습니다.**

### 2. gameplay 장면 3회 캡처 — **확인됨**

| 지표 | 1차 | 2차 | 3차 |
|---|---:|---:|---:|
| 길이 / 프레임 | 36.0s / 964 | 54.7s / 1,485 | 52.8s / 1,326 |
| fps | 26.8 | 27.2 | 25.1 |
| VEH | 27.51% | 27.65% | 26.83% |
| Glide gate | 15.90% | 17.27% | 16.36% |
| unaccounted | 72.49% | 72.35% | 73.17% |
| 예외/프레임 | 861 | 890 | 972 |
| rendezvous 왕복 | 12.2 µs | 11.96 µs | 11.8 µs |
| gate prologue 평균 | 2,778 cy | 2,768 cy | 2,949 cy |

**실행 간 fps 편차는 8.4%**입니다(25.1~27.2). Task 335이 기록한 18%보다 작지만,
5% 임계 판정에는 여전히 2~3회 반복이 필요합니다.

TSC는 3회 모두 약 3.69 GHz로 일치했고, 이를 cycle→시간 환산의 기준으로 씁니다.

### 3. Glide gate 내부 분해 (3차 기준) — **확인됨**

| 구간 | cycles | gate 비중 | wall 비중 |
|---|---:|---:|---:|
| work (호스트 GL 본체) | 16.79e9 | 52.6% | 8.60% |
| wake | 7.29e9 | 22.8% | 3.73% |
| complete | 5.71e9 | 17.9% | 2.92% |
| trap + prologue | 1.96e9 | 6.1% | 1.00% |
| queue | 0.18e9 | 0.6% | 0.09% |

`wake + complete`가 **rendezvous 왕복 순수 비용**이며 3회 모두 호출당 11.8~12.2 µs로
일정합니다. 모든 ordinal에서 `direct=0`이므로 게스트 스레드가 GL을 직접 호출하는
경로는 한 번도 사용되지 않았습니다.

### 4. `glGetError`가 setter 비용의 99.81% — **확인됨**

Task 364의 phase 계측(`REPIU_GLIDE_SETTER_PHASE=1`)이 `grDepthMask` 본체를 분리한
결과입니다(3차 캡처, 24,774회 호출).

| 구간 | 총 cycles | 호출당 | 비율 |
|---|---:|---:|---:|
| `glDepthMask()` | 22,576,956 | 911 | 0.19% |
| `glGetError()` | 12,172,660,110 | **491,356** | **99.81%** |

* `errors=0` — 3회 캡처 전부에서 GL 에러가 한 번도 없었습니다.
* `max-error = 53,722,433` cycle(14.55 ms) — 단일 호출이 프레임 하나보다 길었습니다.
* 독립 계측인 ordinal `work`(12,222,557,126)와 **99.78% 일치**합니다.

`glGetError`는 OpenGL의 비동기 명령 스트림을 **동기화**시키는 호출입니다
(`glFinish`, `glReadPixels`와 같은 부류). 따라서 비용은 호출 자체가 아니라 **앞에
쌓인 명령량에 비례**하며, 평균 대비 최대의 110배 차이가 그 증거입니다.

### 5. 비용은 위치에 따라 100배 달라진다 — **확인됨**

Task 369의 A/B(자동 부팅 장면, 70초 고정, 동일 빌드)에서 같은 `grDepthMask`의
`glGetError`가 **호출당 4,600 cycle**로 측정됐습니다. gameplay 장면의 491,356
cycle과 **107배** 차이입니다.

같은 코드, 같은 함수, 같은 드라이버인데 앞에 쌓인 양이 다릅니다. 4절의 메커니즘을
직접 확인해 주는 관측입니다.

**따라서 `glGetError` 제거의 이득은 자동 장면에서 측정할 수 없습니다.** 자동 장면의
제거 대상 총량은 wall의 0.06%, gameplay 장면은 6.24%입니다.

### 6. 코드베이스 자연 실험 — **확인됨**

`glide_opengl_backend.cpp`의 setter는 에러 체크 유무가 제각각이라 같은 프레임에서
비용을 비교할 수 있습니다(2차 캡처, work/call).

| setter | `glGetError` | work/call |
|---|---|---:|
| `SetTextureFilterMode` (void 반환) | 없음 | 988 |
| `SetConstantColor` | 없음 | 1,439 |
| `SetTextureClampMode` (void 반환) | 없음 | 2,056 |
| `DrawTriangle` | 없음 | 2,678 |
| `SetFogTable` (GLSL uniform) | 없음 | 31,830 |
| `SetFogColor` (GLSL uniform) | 없음 | 36,389 |
| **`SetDitherMode`** (`glEnable` 한 줄) | **있음** | **89,577** |
| **`SetDepthMask`** | **있음** | **493,362** |

`SetDitherMode`는 본체가 `glEnable(GL_DITHER)` 한 줄인데 89,577 cycle입니다.
`SetTextureFilterMode`는 `glTexParameteri`를 여러 번 호출하고도 988 cycle입니다.

중간 대역(fog 31~36k)은 `glGetError`와 무관한 **GLSL uniform 업로드 경로**로,
별도 과제입니다.

### 7. 에러 체크는 귀속도 부정확하다 — **확인됨(사양 근거)**

`glGetError`는 **컨텍스트 단위** 플래그를 반환합니다. `grDepthMask`가 낸 에러가
아니라 아직 실행되지 않았던 임의의 선행 명령이 낸 에러입니다. 따라서 값이 0이
아니었더라도 `linexe_glide_boundary.cpp`의
`decline_gate("depth-mask-backend-failure")`는 남의 에러를 `grDepthMask`에
귀속하게 됩니다.

### 8. 생략 상한 순위 (2차 캡처) — **확인됨**

상한 = `same × (호출당 gate − prologue) / wall`

| ordinal | 호출수 | same | 단가 | wall 상한 |
|---|---:|---:|---:|---:|
| `grDepthMask` | 26,690 | 88.8% | 588,826 | **6.89%** |
| `grTexClampMode` | 30,144 | 94.7% | 57,237 | 0.77% |
| `grTexFilterMode` | 30,144 | 99.8% | 48,462 | 0.68% |
| `grFogColorValue` | 15,185 | 100.0% | 85,046 | 0.62% |
| `grDitherMode` | 7,045 | 100.0% | 137,345 | 0.47% |
| `grConstantColorValue` | 16,120 | 72.5% | 50,921 | 0.28% |
| `grColorCombine` | 5,284 | 99.9% | 107,377 | 0.27% |
| `grAlphaCombine` | 5,284 | 99.9% | 93,072 | 0.24% |
| `grTexSource` | 30,144 | 20.7% | 45,437 | 0.13% |
| `grTexMipmapMode` | 30,144 | 99.8% | 2,321 | ~0 (이미 rendezvous 없음) |

**단, `grDepthMask`의 6.89%는 Task 369가 제거한 `glGetError` 비용과 같은 돈입니다.**
369 적용 후 이 ordinal의 단가가 588,826 → 약 50,000 cycle로 떨어지므로 생략 상한도
약 0.55%로 붕괴합니다. Task 365 batch 2는 그래서 369 뒤로 밀렸습니다.

### 9. Task 365 생략은 정확하다 — **확인됨**

3회 캡처 모두 census `same` 합계와 elision `elided`가 정확히 일치했습니다
(107,462 / 173,610 / 161,045), `voided=0`, `failure=0`, `unsupported=0`.
생략된 호출의 잔여 단가는 2,548 cycle로 gate prologue(2,768)와 같아 **생략의 상한에
도달**했습니다.

### 10. 아직 계측되지 않은 것 — **미확정**

`unaccounted` 72~73%는 순수 게스트 실행이 **아닙니다.** VEH 버킷은 핸들러
진입~퇴출만 재고([execution_time_profile.cpp](../../src/platform/win32/telemetry/execution_time_profile.cpp)),
커널의 예외 전달 왕복(`KiUserExceptionDispatcher` 진입 전 + 복귀)은 계측 창 밖입니다.

프레임당 예외가 861~972개이므로, 왕복을 보수적으로 10 µs만 잡아도
1,289,461 × 36,900 cycle = wall의 약 23%가 어느 버킷에도 귀속되지 않은 채
`unaccounted`에 앉아 있게 됩니다.

같은 이유로 `Glide gate prologue` 평균 2,768~2,949 cycle은 **exception-free
dispatch가 제거할 비용의 하한**입니다. Task 368이 이 값으로 예외 축을 종결한
판정은 재검토 대상입니다.

### 11. Task 369 적용 후 gameplay 재측정 — **확인됨**

64.5초 / 1,788프레임, 정책 기본 OFF.

| 지표 | 제거 전(3차) | 제거 후 |
|---|---:|---:|
| depth-mask `error`/call | 491,356 cy | **68.2 cy** |
| depth-mask `max-error` | 53,722,433 cy | **888 cy** |
| `grDepthMask` gate/call | 542,835 cy | **53,204 cy** |
| `grDepthMask` wall 비중 | 6.89% | **0.76%** |
| `grBufferSwap` work/call | 212,582 cy | **6,220,464 cy** |
| **전체 host GL work** | 16.79e9 | **16.62e9** |

**드라이버 작업은 사라지지 않고 이동합니다.** `grDepthMask`에서 빠진 12.16e9 중
약 10.8e9(89%)가 `grBufferSwap`으로 옮겨갔고, 전체 host GL work 총량은 거의
변하지 않았습니다. `grBufferSwap`이 이제 단일 최대 ordinal(wall의 4.72%)입니다.

**그럼에도 프레임당으로는 순감소입니다.** 장면 구성이 2% 이내로 일치합니다
(삼각형/프레임 85.8 → 84.0, rendezvous/프레임 224.5 → 220.8).

| 프레임당 | 제거 전 | 제거 후 | 변화 |
|---|---:|---:|---:|
| wall | 39.85 ms | 36.05 ms | **−9.6%** |
| Glide gate | 24,066,213 cy | 20,801,917 cy | −13.6% |
| host GL work | 12,658,735 cy | 9,293,465 cy | **−26.6%** |

같은 양을 렌더하면서 host GL work가 프레임당 26.6% 줄었습니다. 강제 배수가 프레임당
19회에서 1회로 줄어 드라이버가 배치를 모을 수 있게 된 결과로 해석합니다.

**fps는 25.09 → 27.73(+10.5%)이나 단일 실행이므로 확정하지 않습니다.** 기준선 3회가
26.8 / 27.2 / 25.1(편차 8.4%)이라 최고치 대비로는 +2.0%입니다. 장면이 일치하는
**프레임당 wall −9.6%**가 더 신뢰할 수 있는 지표입니다.

에러 은폐는 없습니다 — frame-checks 1,788회 전부 `frame-errors=0`,
`drain-iterations=0`. Task 365 생략 정확성도 유지(census `same` 220,888 =
`elided` 220,888, `voided=0`)입니다.

### 12. 프레임 대기의 정체 — **부분 확인, 재해석 필요**

Task 369가 present 직후에 넣은 프레임당 `glGetError`는 호출당 13,445,145 cycle
(3.64 ms), **wall의 10.71%**였습니다. swap 계측이 present(162,694 cycle)와 분리해
보여 줬습니다.

Task 370이 그 검사를 제거하자 **대기가 사라지지 않고 `present`로 이동**했습니다.

| swap 구간(호출당) | Task 369 | Task 370 |
|---|---:|---:|
| present | 162,694 | **13,240,331** |
| accounting(프레임 검사) | 13,445,145 | **19,123** |
| max-present | 836,051 | **59,737,352** (16.18 ms) |

`max-present`가 60 Hz 리프레시 한 주기와 정확히 일치합니다. 즉 그 3.6 ms는
`glGetError` 오버헤드가 아니라 **디스플레이/flip 큐 대기**이며, 앞서 "present가
44 µs이므로 GPU가 밀려 있지 않다"고 읽은 것은 **인과가 거꾸로**였습니다 — present가
빨랐던 이유는 직전 `glGetError`가 이미 배수를 끝냈기 때문입니다.

게스트는 `grBufferSwap`에서 매 프레임 **swap interval 1(vsync)** 을 요청합니다.
따라서 **Glide gate 시간의 상당 부분이 비용이 아니라 유휴 대기일 가능성**이 있고,
그렇다면 그 슬랙 안에서 CPU 작업을 줄여도 프레임은 늘지 않습니다. 이는 3절의 gate
분해 해석에 직접 영향을 줍니다.

판정은 swap interval 0 강제 캡처 하나로 됩니다.

### 13. 디스플레이 제한 판정 — **확인됨, 이전 분해를 재작성함**

Task 371이 `REPIU_GLIDE_SWAP_INTERVAL`로 swap interval을 강제해 판정했습니다.
자동 장면 70초 고정, 동일 빌드.

| 지표 | 1 (vsync) | -1 (adaptive) | **0 (immediate)** |
|---|---:|---:|---:|
| 프레임 | 2,124 | 2,175 | **3,444** |
| fps | 30.3 | 31.1 | **49.2** |
| present work/call | 10,210,872 | 9,767,372 | **158,506** |
| max-present | 64,616,449 | 55,425,445 | 4,295,589 |
| Glide gate | 20.20% | 19.77% | **12.52%** |
| unaccounted | 69.94% | 70.51% | **76.93%** |

**vsync를 끄면 프레임이 +62.1%.** present 단가가 64배 붕괴하고, 그 값(158,506)이
Task 370 이전 측정치(162,694)와 일치합니다.

**따라서 3절의 gate 분해와 이 문서 이전 절들의 Glide 비중은 유휴 대기를 포함하고
있었습니다.** interval 1에서 present가 프레임당 약 2.77 ms를 idle로 쓰고 그것이
`grBufferSwap` gate 시간에 계상됐습니다. **앞으로의 성능 측정은 interval 0으로
고정해야 합니다.**

게임 속도는 변하지 않았습니다. tick `due`가 16,273 vs 16,271로 동일하고, 전달률은
91.9% → **99.4%**로 오히려 개선됐습니다(coalesced 1,279 → 51). 즉 프레임 이득이
게임이 빨라진 착시가 아닙니다.

adaptive(-1)는 적용됐으나 효과가 없었습니다. double buffer + interval 1에서 60 Hz
마감을 놓치면 절반인 30 fps로 양자화되며, 관측된 30.3 fps가 그 값입니다.

**부수 확인:** 게스트의 `grBufferSwap` interval 인자는 적용된 적이 없습니다.
backend가 텔레메트리에만 기록하고 `SDL_GL_SetSwapInterval`을 호출하지 않았습니다.

### 14. 커널 예외 전달 비용 — **실측됨**

Task 372가 VEH 핸들러 퇴출과 다음 진입 사이의 간격을 재서, 10절이 "미확정"으로
남겨둔 값을 확정했습니다. interval 0 고정, 70초, wall 259,096,642,075, 예외
2,081,859건.

| gap 분류 | 건수 | 평균 cycles |
|---|---:|---:|
| single-step | 926,025 | **31,769** |
| breakpoint | 1,081,430 | 154,619 |
| other | 74,403 | 30,870 |
| 최소 gap | | **21,534** |

연속된 두 single-step 사이에서 게스트는 명령 1개만 실행하므로 **31,769
cycle(8.6 µs)이 커널 왕복**입니다.

| 항목 | wall 비중 |
|---|---:|
| VEH 핸들러 본체 | 23.18% |
| 커널 왕복(평균 기준) | **25.5%** |
| 커널 왕복(최소 기준, 보수적) | 17.3% |
| **예외 기구 총계** | **40.5 ~ 48.7%** |

교차 검증 3건이 일치합니다. 합성 캘리브레이션(single-step 25,855 / INT3 21,347)
대비 실측 평균이 23% 높고(무거운 핸들러이므로 예상 방향), **최소 gap 21,534는 합성
INT3 21,347과 0.9% 차이**이며, single-step 평균이 interval 1에서 31,761 / interval 0
에서 31,769로 **0.03% 차이**입니다. 마지막 항목이 이 값을 장면·구성과 무관한 고정
비용으로 확인해 줍니다.

**따라서 10절의 "보수적으로 10 µs를 잡아도 약 23%" 추정은 실측으로 대체됩니다** —
실제 왕복 단가는 8.6 µs이고 총량은 17.3~25.5%입니다.

**Task 368의 판정은 유효하며 이 측정이 뒷받침합니다.** 368은 prologue 2,768 cycle만
본 것이 아니라 커널 전이를 Task 336 가격으로 호출당 34,521 cycle 별도 계상했고,
실측 31,769와 8% 차이입니다. 368이 Glide gate에서 이득을 3.25%로 본 이유는 전이를
못 봐서가 아니라 **gate 본체가 호출당 약 235,000 cycle이고 예외를 없애도 남기
때문**입니다.

**모집단별로 본체 비용이 다른 것이 핵심입니다.** 사용자 캡처(623,056건) 기준:

| 모집단 | 건수 | 비중 | 예외당 본체 |
|---|---:|---:|---|
| Glide gate | 233,754 | 37.5% | 약 235,000 cycle — 크다 |
| 그 외 breakpoint | 97,849 | 15.7% | 미측정 |
| **single-step** | **264,561** | **42.5%** | **명령 1개 — 거의 0** |
| AV / 기타 | 26,892 | 4.3% | 미측정 |

Glide gate는 비싼 본체 위의 얇은 예외 층이라 제거 이득이 작고, **single-step은
본체가 거의 없어 31,003 cycle 왕복이 비용 전부**입니다. single-step gap만으로
wall의 9.06%이며, 예외 제거가 값나가는 곳은 여기입니다.

**계측 주의:** `PollThreadUntilExit`에 1초 무진행 watchdog이 있어 설정 타임아웃과
무관하게 조기 종료하고도 `timed_out=true`로 보고합니다. **A/B는 프레임 수만이 아니라
wall cycle을 함께 확인해야 합니다.** Task 371의 A/B를 이 기준으로 재검증했고 wall
0.016% 차이로 결론은 유효했습니다(프레임 +63.7%, 최초 +62.1% 재현).

### 15-1. pumpit3 정상 실행에서 gate가 지배 항목이다 — **확인됨 (Task 418)**

[Task 418 재기준선](../work-logs/20260804-418-cost-profile-rebaseline.md)이 pumpit3
정상 실행 7회(60초, 격리 0·세대 실패 0)에서 잰 값입니다. 이 문서의 기존 절이 pumpit1
gameplay 장면 기준인 것과 달리, 아래는 **pumpit3**입니다.

| 지표 | 값 |
|---|---:|
| glide-gate cycles / guest-run | **54~55%** (120.7~122.8G / 222.1G) |
| gate 호출 횟수(60초) | **2,459,898~2,518,493** |
| breakpoint 예외 비중 | **68.4~69.4%**, 그 중 HLE 경계 provenance **약 83%** |
| **host 표본에서 `InvokeOnHostThread`** | **74.8~76.3%** (2위 이하 전부 ≤1.6%) |
| 게스트 스레드 CPU share | **50.4~54.0%** |

**그 지점은 `glide_opengl_backend.cpp:295`의 조건변수 대기**
(`host_command_cv_.wait(lock, [] { return host_command_complete_; })`)입니다. CPU
share가 절반인 것과 같은 사실이며, 게스트 스레드는 Glide 호출마다 host thread 완료를
기다립니다.

### 15-2. 그 76%는 왕복 지연이었고, 스핀으로 걷어냈습니다 — **확인됨 (Task 419)**

집계 분해는 **Task 418 로그에 이미 있었습니다**(`REPIU_EXECUTION_TIME_PROFILE=1`이 함께
켬. 15-1이 "꺼져 있다"고 본 줄은 **ordinal별** 계측입니다).

| 구간 | pumpit3 spin off | pumpit3 spin 20 µs | pumpit1 off → on |
|---|---:|---:|---:|
| wake | 34.4~34.9% | **3.6~9.4%** | 4.16% → 0.88% |
| work | 33.3~34.2% | **86.0~94.1%** | 92.33% → 98.78% |
| complete | 30.8~31.4% | **1.8~3.6%** | 3.42% → 0.27% |
| **wake+complete** | **65.4~66.3%** | **5.3~12.9%** | 7.6% → 1.15% |
| 프레임(60초) | 2,229~2,442 | **3,036~3,084** | 3,119 → 3,154 |

**대기였습니다.** 호출당 왕복 고정비(약 70,000 cycle)가 호출당 작업(34,745 cycle)보다
컸고, 짧은 스핀 후 조건변수로 폴백하자 **프레임 중앙값이 +27.7%**(2,399 → 3,063)
올랐습니다. 스핀 hit율은 게스트측 99.5%+, 호스트측 약 94%입니다.

**gate가 짧아진 것이 아닙니다.** rendezvous가 1.03~1.06M → 1.267M(+20%)이고 프레임당
호출 수는 430~434 → 412~415로 사실상 같습니다. **대기가 작업으로 바뀐 것**입니다.

**pumpit1은 거의 변하지 않습니다**(+1.1%). 그 타이틀은 원래 work가 92%라 걷어낼 대기가
없습니다 — 15-1이 "이건 pumpit3 고유"라고 한 것이 그대로 확인됩니다.

전문: [Task 419 작업 로그](../work-logs/20260805-419-glide-rendezvous-spin-wait.md).
기본값은 `REPIU_GLIDE_RENDEZVOUS_SPIN_US=20`이며 `0`이 예전 동작입니다.

### 16. Task 482 pass 1 — 게이트 비용의 절반은 호스트에 닿지 않는 crossing입니다 (2026-08-22) — **확인됨, 3회 재현**

pumpit8 Release, vsync OFF, `REPIU_GLIDE_ORDINAL_TIME_PROFILE=1` +
`REPIU_EXECUTION_TIME_PROFILE=1`로 사용자가 3회 실행(약 53~57초, 33.4k~36.2k 프레임)한
로그입니다. Glide gate는 `guest-run`의 **23.67~24.71%**입니다.

**게이트 bucket의 구성 (3회 모두 1포인트 이내로 재현).**

| 구성 | bucket 대비 | guest-run 대비 |
|---|---:|---:|
| **호스트에 닿지 않는 crossing** | **45.2~47.2%** | **10.7~11.6%** |
| rendezvous `wake` (호스트 스레드 스케줄 대기) | 24.9~25.6% | 약 6% |
| rendezvous `work` (실제 GL) | 18.9~20.6% | 약 4.7% |
| rendezvous `complete` | 6.6~6.9% | — |
| rendezvous `queue` | 약 2.0% | — |

**확인됨 1 — crossing의 70.8~71.1%는 아무 일도 하지 않습니다.** 게이트 진입 15,235,411 /
15,655,476 / 13,170,874회 중 setter elision이 걸러낸 것이 10,789,176 / 11,128,742 /
9,331,423회입니다. Tasks 437·439·443·444의 elision은 **호스트 왕복**을 없앴지만
**crossing 자체는 그대로 남아** 회당 약 1,790 cycle을 씁니다. 아무 부작용도 없는 호출
1,300만 회가 게이트 bucket의 절반, `guest-run`의 11%입니다. **이 crossing 비용은 지금까지
측정된 적이 없습니다** — 이전 A/B들은 elision이 없앤 rendezvous만 셌습니다.

**확인됨 2 — 지배적인 ordinal은 있지만 "중복 host 작업"은 아닙니다.**

| ordinal | gate 대비 | 회당 | 구성 |
|---|---:|---:|---|
| `grTexSource` | 25.97 / 25.97 / 26.27% | 약 14,600 | **wake 63~65%**, work 약 11% |
| `grBufferSwap` | 14.82 / 15.18 / 16.67% | 약 220,000 | work 74~76% (실제 present) |
| `grDrawTriangle` | 12.42 / 12.63 / 12.03% | 약 2,235 | rendezvous 0 (batch) |
| `grConstantColorValue` | 11.49 / 11.29 / 11.23% | 약 3,450 | wake 22~24% |
| `grAlphaBlendFunction` | 7.88 / 7.80 / 7.12% | 약 5,100 | wake 40~42% |

`grTexSource`가 1위지만 그 시간의 대부분은 **호스트 스레드가 스케줄되기를 기다리는
시간**이고 실제 GL 작업은 11%입니다. rendezvous 수(981,733)가 호출 수(900,209)보다 많은
것은 텍스처 변경이 draw batch flush를 강제하기 때문으로 읽힙니다. `grBufferSwap`의 비용은
74~76%가 실제 present 작업이므로 줄일 대상이 아닙니다. 즉 **설계 20260814-482의
"중복 host 작업을 가진 지배적 ordinal" 분기는 해당 없음**입니다.

**확인됨 3 — wake 단가는 rendezvous당 6,513~7,352 cycle입니다.** host spin은
33.3~36.3% miss이고 guest spin은 1.0% miss입니다. 즉 게스트는 잘 기다리는데 **호스트
스레드가 제때 깨지 못합니다.** 2026-08-13 세션이 "게스트가 CPU를 포화시켜 호스트가
스케줄되지 못하는 증상"이라고 읽은 것에 크기를 붙인 값입니다 — `guest-run`의 약 6%.

**관측 (별건, 확인 필요).** draw batch 평균이 **5.05**입니다. Task 439가 승격 근거로 잰
16.02의 3분의 1이고, flush는 프레임당 약 18회(616,534회)입니다. 이 장면 고유인지
회귀인지는 별도 확인이 필요합니다.

**방법 주의.** 세 실행의 프레임당 삼각형은 82.2 / 81.4 / 73.6으로 Task 478의 3% 장면
동일성 규칙을 넘습니다. 그러나 이 분석은 실행 **간** 비교가 아니라 실행 **안**의 귀속이고,
비중이 세 실행에서 1포인트 이내로 재현되므로 결론은 유지됩니다. EEPROM은 실행별로
격리되지 않았습니다(`nvram\pumpit8\eeprom.dat` 공용).

### 15. 열린 질문

* 커널 예외 전달 비용의 실제 크기 (10절)
* rendezvous 왕복 11.8 µs × 30만 회를 제거할 수 있는가 (`direct` 경로 미사용).
  **pumpit3에서는 이 질문이 1순위가 됐습니다** — 15-1절 참조(호출 2.46M회,
  host 표본 76%)
* fog / combine setter의 GLSL uniform 경로 31~36k cycle의 정체
* exception-free dispatch가 실제로 어디까지 예외를 없앨 수 있는가. 상한은 1.68~1.95배
  이고 vsync 30 → 60 fps에는 1.22배면 충분합니다(14절).
* 예외 구성은 breakpoint 51.9% / single-step 44.5% / 나머지 3.6%이며 breakpoint
  대부분이 HLE 경계입니다. 경계를 예외 없이 통과시키는 경로의 범위가 다음 질문입니다.
* 기본 swap interval 정책 결정 (원본 충실 vsync / interval 0 / 프레임을 60 Hz 마감
  안으로). 사용자 판단 사항.
* Task 365 batch 2의 남은 가치 (`grDepthMask` 단가가 53,204 cycle로 떨어졌으므로
  생략 상한은 wall 6.89% → 약 0.7%로 붕괴했을 것)
* **elision 판정을 crossing 앞으로 옮길 수 있는가** (16절). 지금은 게이트를 넘은 뒤
  호스트 경계 처리기에서 같은 값인지 판정하므로, 판정이 참인 1,300만 회도 회당 약 1,790
  cycle을 이미 지불한 뒤입니다.
* draw batch 평균이 16.02에서 5.05로 떨어진 것이 장면 차이인지 회귀인지 (16절)

---

## English

**Task 482 pass 1: half the gate cost never reaches the host (2026-08-22, confirmed across
three runs).** Three user runs of pumpit8 on Release with vsync off,
`REPIU_GLIDE_ORDINAL_TIME_PROFILE=1` and `REPIU_EXECUTION_TIME_PROFILE=1`, each 53-57 seconds
and 33.4k-36.2k frames, put the Glide gate at 23.67-24.71% of `guest-run` and split that bucket
the same way to within one point every time: **45.2-47.2% is crossings that never reach the host
thread** (10.7-11.6% of `guest-run`), 24.9-25.6% is rendezvous `wake`, 18.9-20.6% is real GL
`work`, 6.6-6.9% `complete`, and about 2.0% `queue`.

*Confirmed 1 — 70.8-71.1% of all crossings do nothing.* Of 15,235,411 / 15,655,476 / 13,170,874
gate entries, setter elision discarded 10,789,176 / 11,128,742 / 9,331,423. The elision from
Tasks 437, 439, 443, and 444 removed the **host round trip** but left the **crossing itself**, at
about 1,790 cycles each. Thirteen million calls with no effect account for half the gate bucket
and 11% of `guest-run`. That crossing cost had never been measured: the earlier A/Bs counted only
the rendezvous the elision removed.

*Confirmed 2 — there is a dominant ordinal, but not a redundant one.* `grTexSource` leads at
25.97 / 25.97 / 26.27% of the gate, yet 63-65% of its time is `wake` and only about 11% is GL
work; its rendezvous count (981,733) exceeds its call count (900,209), which reads as texture
changes forcing draw-batch flushes. `grBufferSwap` follows at 14.82-16.67%, but 74-76% of that is
real presentation work and is not a target. `grDrawTriangle` (12.0-12.6%, zero rendezvous, about
2,235 cycles per call), `grConstantColorValue` (11.2-11.5%), and `grAlphaBlendFunction`
(7.1-7.9%) complete the top five. Design 20260814-482's "dominant ordinal with redundant host
work" branch therefore does not apply.

*Confirmed 3 — waking the host costs 6,513-7,352 cycles per rendezvous.* The host spin misses
33.3-36.3% of the time while the guest spin misses 1.0%: the guest waits well and the host thread
is not scheduled in time. That puts a size on what the 2026-08-13 session read as CPU saturation,
about 6% of `guest-run`.

*Separate observation, needs checking.* The mean draw batch is 5.05, a third of the 16.02 Task
439 used to promote batching, with about 18 flushes per frame (616,534 total). Whether that is
specific to this scene or a regression is an open question.

*Method note.* Triangles per frame were 82.2 / 81.4 / 73.6, outside the Task 478 3% scene rule,
but this is attribution *within* each run rather than a comparison *between* runs, and the shares
reproduce to within one point, so the conclusion stands. The EEPROM was not isolated per run.

**Scene composition decides the answer (confirmed).** The same state-setter set
holds 20.59% of wall in the Task 363 gameplay capture, 11.5–12.6% in three user
gameplay captures, and about 5.6% in the boot-inclusive automated scene where 304
`grLfbLock` calls dominate. Tasks 364 through 368 were all measured on the
automated scene, which is why they reported a small gain three times running.

**Three gameplay captures (confirmed).** 26.8 / 27.2 / 25.1 fps, VEH 26.8–27.7%,
Glide gate 15.9–17.3%, unaccounted 72.3–73.2%, 861–972 exceptions per frame, and a
rendezvous round trip of 11.8–12.2 µs that is stable to 3%. Run-to-run frame
variance is 8.4%, so a 5% threshold still needs two or three repetitions. The TSC
read 3.69 GHz in all three.

**The Glide gate splits into** host GL work 52.6%, wake 22.8%, complete 17.9%,
trap and prologue 6.1%, queue 0.6%. `wake + complete` is the pure cross-thread
round trip; no ordinal ever used the `direct` path.

**`glGetError` is 99.81% of `grDepthMask` (confirmed).** Task 364's phase
instrument separates the body: `glDepthMask` costs 911 cycles per call while the
trailing `glGetError` costs 491,356, across 24,774 calls that never once reported
an error, with a worst case of 14.55 ms in a single call. An independent
instrument agrees to 99.78%. `glGetError` is a synchronisation point in the same
family as `glFinish`, so its cost tracks what is queued ahead of it rather than
the call itself — the 110x spread between mean and maximum is the evidence.

**The same call costs 4,600 cycles in the automated scene (confirmed)** — 107x
less than in gameplay, with the same code and driver. That is the mechanism
observed directly, and it means the benefit of removing the check cannot be
measured on the automated scene at all: the addressable amount there is 0.06% of
wall against 6.24% in gameplay.

**The codebase is a natural experiment (confirmed).** Setters without the check
cost 988 to 2,678 cycles per call; `SetDitherMode`, whose body is a single
`glEnable`, costs 89,577 because it has one. The 31–36k middle band is the GLSL
uniform path on the fog setters, unrelated to `glGetError` and still open.

**The check is imprecise as well as expensive (confirmed by specification).** The
flag is context-wide, so a nonzero result names whichever earlier command failed,
not the setter observing it — the existing `decline_gate` attribution would be
wrong.

**Elision ceilings** rank `grDepthMask` first at 6.89% of wall, an order above
everything else, but that is the same money Task 369 removes: once the per-call
cost falls, the ceiling collapses to about 0.55%. Task 365 batch two is therefore
sequenced behind Task 369. Elision correctness held in all three captures, with
census `same` matching elision `elided` exactly and no voided applications.

**Task 369 re-measurement (confirmed).** After moving the per-call check behind
`REPIU_GLIDE_GL_ERROR_CHECK`, a 64.5-second, 1,788-frame gameplay capture at the
default setting cut the depth-mask error interval from 491,356 to 68.2 cycles per
call and its worst case from 53,722,433 to 888, dropping `grDepthMask` from 6.89%
to 0.76% of wall — 6.13 points recovered against the 6.24% predicted, with zero
errors across all 1,788 frame checks and elision correctness intact.

The driver work relocates rather than disappears: `grBufferSwap` work rose from
212,582 to 6,220,464 cycles per call, absorbing about 10.8e9 of the 12.16e9
removed, and total host GL work barely moved (16.79e9 to 16.62e9), making
`grBufferSwap` the largest single ordinal at 4.72% of wall. A net gain still holds
per frame, where scene composition matches within 2%: wall per frame fell from
39.85 to 36.05 ms (-9.6%), the Glide gate 13.6%, and host GL work 26.6% — a real
reduction consistent with the driver batching once the forced drain fell from
nineteen times a frame to once. The fps move, 25.09 to 27.73, is not claimed as
+10.5%: the three-run baseline spanned 25.1 to 27.2 at 8.4% variance, so against
its top the gain is +2.0%, and the per-frame figure is the more reliable one.

**What is still unmeasured (unresolved).** The 72–73% "unaccounted" is not pure
guest execution: the VEH bucket times handler entry to exit, so the kernel's
exception delivery round trip falls outside every bucket. At 861–972 exceptions
per frame, even a conservative 10 µs round trip would place roughly 23% of wall
in that gap. For the same reason the 2,768–2,949 cycle gate prologue is a lower
bound on what exception-free dispatch would remove, which puts Task 368's
closure of the exception axis back up for review.

**pumpit3's healthy run is gate-dominated, and the gate is a wait (confirmed, Task 418).**
Across seven healthy 60-second pumpit3 runs with zero quarantines,
[the re-baseline](../work-logs/20260804-418-cost-profile-rebaseline.md) measured Glide gate
cycles at **54-55% of guest-run** over **2.46-2.52 M gate calls**, breakpoint exceptions at
**68.4-69.4%** with **about 83%** of them raised at the HLE boundary, and — decisively — a
single host call site holding **74.8-76.3%** of guest-thread samples with nothing else above
1.6%: the condition-variable wait in `InvokeOnHostThread` at `glide_opengl_backend.cpp:295`.
The guest thread's CPU share of **50.4-54.0%** states the same fact from the other side. Note
these are pumpit3 figures, where the earlier sections measure pumpit1 gameplay. **What is not
yet separated** is whether that 76% is time spent waiting while the host works or the cost of
the round trip itself; the instrumentation that splits it (`RecordGlideGatePublish`,
`RecordGlideGateResume`, `RecordGlideOrdinalRendezvous`) already exists and simply read
`rendezvous/direct: 0/0` in these runs. The two answers demand opposite remedies, which is why
that switch is the next measurement.

**That 76% was round-trip latency, and a spin removed it (confirmed, Task 419).** The aggregate
decomposition turned out to be in Task 418's logs already — `REPIU_EXECUTION_TIME_PROFILE=1`
enables it, and the line read as "off" above belongs to the per-ordinal profile. With the spin
off, pumpit3 splits 34.4-34.9% wake, 33.3-34.2% work and 30.8-31.4% complete; with a 20 µs
spin-then-wait it becomes 3.6-9.4% / 86.0-94.1% / 1.8-3.6%, taking `wake + complete` from
**65.4-66.3% to 5.3-12.9%** and frames from 2,229-2,442 to **3,036-3,084**, a median gain of
**+27.7%**. Spin hit rates are over 99.5% on the guest side and about 94% on the host side.
**The gate did not shrink**: rendezvous counts rose from 1.03-1.06 M to 1.267 M (+20%) while
calls per frame stayed at 412-434, so waiting turned into working. **pumpit1 gains 1.1%**,
since 92% of its gate time was already work — the title-specific split holds. The default is
`REPIU_GLIDE_RENDEZVOUS_SPIN_US=20`, with `0` restoring the old behaviour; detail in the
[Task 419 work log](../work-logs/20260805-419-glide-rendezvous-spin-wait.md).
