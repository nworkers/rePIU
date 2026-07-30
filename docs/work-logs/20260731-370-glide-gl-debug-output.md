# 작업 로그: GL 디버그 출력으로 프레임 검사 대체 / Work log: replace the frame check with GL debug output

Task 370. 설계 [20260731-370](../design/20260731-370-glide-gl-debug-output.md),
작업 지시 [20260731-370](../work-orders/20260731-370-glide-gl-debug-output.md)

## 한국어

### 구현

* `glide_gl_error_policy`에 디버그 출력 카운터, 고정 크기 첫 메시지 버퍼,
  프레임 주기 resolver를 추가했습니다.
* backend가 컨텍스트 생성 직후 `glDebugMessageCallback`을 설치합니다.
  `GL_DEBUG_OUTPUT`만 켜고 `GL_DEBUG_OUTPUT_SYNCHRONOUS`는 명시적으로 끕니다.
* 콜백 트램펄린은 `APIENTRY` 명명 함수입니다 — 람다 변환에 호출 규약을 맡기지
  않았습니다. 기록만 하며 락·할당·I/O가 없습니다.
* 프레임 검사는 주기화됐습니다. 디버그 출력 설치 시 0(완전 제거), 미지원 시 64,
  `REPIU_GLIDE_GL_ERROR_FRAME_INTERVAL`이 명시되면 그 값이 우선합니다.
* 요약에 debug output 줄을 추가했습니다.

### 검증

* Debug/Release 빌드 성공, `repiu_aot_probe` exit 0, probe **8개 항목 전부 true**
  (interval 파싱, 디버그 메시지 누적, 첫 에러 보존, NOTIFICATION 제외, 메시지
  절단·종단, `nullptr` 무해 포함).
* 자동 장면 70초 실측(`REPIU_GLIDE_SWAP_TIME_PROFILE=1`):

```
Glide GL error policy per-call-check/frame-interval/frame-checks/...: false/0/0/0/...
Glide GL debug output installed/messages/errors/first-id: true/2/0/0x00000000
```

**목표는 달성했습니다.** 디버그 출력이 실제로 설치됐고(메시지 2건, 에러 0),
프레임당 `glGetError`는 **0회**입니다. accounting 구간이 호출당 13,445,145 →
**19,123 cycle로 703배** 붕괴했습니다.

### 그러나 예측은 틀렸습니다 — **중요**

| swap 구간(호출당) | Task 369 | Task 370 |
|---|---:|---:|
| present (`SDL_GL_SwapWindow`) | 162,694 | **13,240,331** |
| accounting (프레임 검사) | 13,445,145 | **19,123** |
| max-present | 836,051 | **59,737,352** (16.18 ms) |

**대기가 사라진 것이 아니라 `present`로 옮겨갔습니다.** 설계에서 "wall의 10.71%
회수"를 기대했으나 회수되지 않았습니다.

`max-present` 59,737,352 cycle = **16.18 ms**는 60 Hz 리프레시 한 주기와 정확히
일치합니다. 즉 그 3.6 ms는 **`glGetError`의 오버헤드가 아니라 디스플레이/flip 큐
대기**였고, `glGetError`가 그 대기를 대신 치르고 있었을 뿐입니다.

설계에 적은 추론 — "present가 44 µs이므로 GPU가 밀려 있지 않다" — 이 **틀렸습니다.**
present가 빨랐던 이유는 바로 앞의 `glGetError`가 이미 배수를 끝내고 그 시간을
소진했기 때문입니다. 인과를 거꾸로 읽었습니다.

### 프레임 수 판정 불가

자동 장면 70초 고정, 동일 wall(259.15e9):

| 구성 | 프레임 | Glide gate |
|---|---:|---:|
| Task 369 OFF | 2,429 | 29.32% |
| Task 369 ON | 2,223 | 25.33% |
| **Task 370** | **2,260** | **23.27%** |

편차 약 9%로 이 장면의 알려진 범위(최대 18%) 안입니다. **370이 낫다고도 못하다고도
말할 수 없습니다.**

### 그럼에도 이 변경을 유지하는 이유

1. 폴링 동기화를 제거하고 push 보고로 바꿨습니다. 진단 정보는 오히려 늘었습니다
   (source·type·severity·id·메시지, 실패 지점에서 발생).
2. 프레임당 `glGetError` 0회 — CPU 스톨이 디스플레이 대기로 바뀌었습니다. 총량이
   같더라도 폴링으로 태우는 것보다 낫습니다.
3. Task 369의 setter 경로 제거(gameplay에서 `grDepthMask` 6.89% → 0.76%)는 이번
   결과와 무관하게 유효합니다.

### 이 결과가 여는 질문 — 다음 작업

게스트는 `grBufferSwap`에서 **매 프레임 swap interval 1(vsync)을 요청**합니다
(`requested interval zero/one/other = 0/1876/0`). `max-present`가 정확히 한
리프레시 주기라는 것은 **이 실행이 디스플레이에 제한되고 있을 가능성**을 가리킵니다.

이것이 사실이면 Glide 축 분석의 전제가 바뀝니다 — gate 시간의 상당 부분이 비용이
아니라 **유휴 대기**이고, 그 슬랙 안에서는 CPU 작업을 줄여도 프레임이 늘지
않습니다.

판정 방법은 swap interval을 0으로 강제한 캡처 하나입니다. 프레임이 크게 오르면
디스플레이 제한이 확인되고, 오르지 않으면 CPU 제한이므로 Glide 축을 닫고 미계측
커널 예외 전달 비용으로 넘어갑니다.

---

## English

### Implementation and verification

The policy module gained debug-output counters, a fixed-size first-message buffer,
and a frame-interval resolver; the backend installs `glDebugMessageCallback` right
after context creation with `GL_DEBUG_OUTPUT` on and `GL_DEBUG_OUTPUT_SYNCHRONOUS`
explicitly off, through a named `APIENTRY` trampoline rather than relying on a
lambda conversion for the calling convention. The frame check became periodic —
zero when the callback installs, 64 otherwise, with the environment variable taking
precedence. Both configurations build, the probe exits zero with all eight
assertions true, and a 70-second automated capture confirms the callback installed
(two messages, zero errors) with **zero per-frame `glGetError` calls** and the
accounting interval collapsing from 13,445,145 to **19,123 cycles per call, a
703-fold reduction**.

### The prediction was wrong

The wait did not disappear; it moved into the present. `SDL_GL_SwapWindow` went
from 162,694 to 13,240,331 cycles per call, with a maximum of 59,737,352 cycles —
**16.18 ms, exactly one 60 Hz refresh period**. So the 3.6 ms was display and
flip-queue wait rather than `glGetError` overhead, and the error check had merely
been paying it. The design's inference that a 44 µs present proved the GPU was not
backed up read the causality backwards: the present was fast *because* the
preceding `glGetError` had already drained the pipeline and burned the time. The
predicted 10.71% recovery did not occur.

Frame counts over the same fixed 70 seconds — 2,429 for Task 369 off, 2,223 for
Task 369 on, 2,260 for Task 370 — span about 9%, inside this scene's known 18%
variance, so the change cannot be called better or worse on throughput.

### Why the change stays

It replaces polling synchronisation with push reporting and yields strictly more
diagnostic information; the per-frame `glGetError` is gone, converting a CPU stall
into an idle display wait even where the total is unchanged; and Task 369's
separate setter-path result in the gameplay scene (`grDepthMask` from 6.89% to
0.76% of wall) stands independently of this.

### What this opens

The guest requests swap interval 1 on every frame, and the maximum present is
exactly one refresh period, which points at this run being display-limited. If that
holds, a large part of the Glide gate is idle waiting rather than cost, and
reducing CPU work inside that slack will not move frames. One capture with the swap
interval forced to zero decides it: a large frame increase confirms the display
limit, and no increase closes the Glide axis in favour of the unmeasured kernel
exception-delivery cost.
