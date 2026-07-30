# 작업 로그: swap interval 강제와 디스플레이 제한 판정 / Work log: swap interval override and the display-limit verdict

Task 371. 설계 [20260731-371](../design/20260731-371-glide-swap-interval-override.md),
작업 지시 [20260731-371](../work-orders/20260731-371-glide-swap-interval-override.md)

## 한국어

### 판정: **이 실행은 디스플레이에 제한되고 있었습니다**

자동 장면 70초 고정, 동일 빌드, `REPIU_GLIDE_SWAP_INTERVAL`만 다름.

| 지표 | 1 (vsync) | -1 (adaptive) | **0 (immediate)** |
|---|---:|---:|---:|
| 프레임 | 2,124 | 2,175 | **3,444** |
| fps | 30.3 | 31.1 | **49.2** |
| present work/call | 10,210,872 | 9,767,372 | **158,506** |
| max-present | 64,616,449 (17.5 ms) | 55,425,445 (15.0 ms) | 4,295,589 (1.16 ms) |
| Glide gate | 20.20% | 19.77% | **12.52%** |
| VEH | 30.06% | 29.49% | 23.07% |
| unaccounted | 69.94% | 70.51% | **76.93%** |

**vsync를 끄면 프레임이 +62.1% 늘어납니다.** present 비용은 호출당 10,210,872 →
**158,506 cycle(64배)** 로 붕괴하며, 이 값은 Task 370 이전에 측정된 present
162,694 cycle과 일치합니다. 즉 그때 present가 쌌던 이유가 확인됩니다.

adaptive(-1)는 적용됐지만(`applied=true`, `effective=-1`) **효과가 없습니다**
(2,175 vs 2,124). 이 드라이버에서 tear 동작이 실질적으로 작동하지 않는 것으로
보입니다.

### 게임 속도는 변하지 않았습니다 — 설계가 요구한 검증

리듬 게임이므로 프레임만 보면 안 됩니다. 설계에서 `timer tick delivery`를 함께
읽기로 했고, 결과는 이렇습니다.

| 지표 | 1 (vsync) | 0 (immediate) |
|---|---:|---:|
| tick due | 16,273 | 16,271 |
| tick injected | 14,954 | **16,180** |
| coalesced | 1,279 | **51** |
| **전달률** | 91.9% | **99.4%** |

**`due`가 사실상 동일**(16,273 vs 16,271)하므로 게임 시간이 빨라진 것이 아닙니다 —
타이머 요구량은 wall clock 기반입니다. 그런데 전달률이 91.9% → **99.4%**로
올랐습니다. 즉 vsync를 끄면 프레임만 느는 것이 아니라 **게임 로직이 더 정확하게
구동됩니다.** 블로킹이 줄어 tick 병합이 1,279 → 51로 감소했습니다.

### 이것이 지금까지의 분석을 재작성합니다

interval 1에서 present가 프레임당 약 2.77 ms를 **유휴 대기**로 썼습니다. 그 시간이
`grBufferSwap`의 gate 시간으로 잡혀 Glide gate 비중을 부풀렸습니다.

vsync를 끈 기준으로 다시 보면 **Glide gate는 wall의 12.52%, unaccounted는
76.93%**입니다. Glide 축은 지금까지 보이던 것보다 작고, 게스트 실행이 더 확실하게
지배합니다.

double buffer + interval 1에서 리프레시 마감을 놓치면 다음 주기까지 통째로
기다리므로 60 Hz의 절반인 30 fps로 양자화됩니다. 관측된 30.3 fps가 정확히 그
값입니다.

### 부수 확인: 게스트 요청은 적용된 적이 없었다 — **확인됨**

boundary가 `grBufferSwap`의 첫 인자를 `BufferSwap(swap_interval)`로 넘기지만
backend는 그 값을 텔레메트리에만 기록하고 `SDL_GL_SetSwapInterval`을 호출한 적이
없습니다. 지금까지의 vsync는 게스트 요청이 아니라 SDL/드라이버 기본값이었습니다.

### 구현·검증

* 신규 `glide_swap_interval_policy.{h,cpp}` — `-1`~`4`만 수용, 후행/선행 공백과
  범위 밖 값 거부.
* `OpenWindowed`의 `SDL_GL_MakeCurrent` 직후 1회 적용, 실효값 되읽기.
  **미설정 시 SDL 호출을 하지 않으므로 기본 동작은 완전히 그대로입니다.**
* 요약 한 줄 추가. 실효값을 에코가 아니라 드라이버에서 되읽어, 거부·클램프가
  숨지 않습니다.
* Debug/Release 빌드 성공, `repiu_aot_probe` 양 구성 exit 0, 신규 probe 4개 항목
  전부 true.

### 남은 결정과 다음 작업

**기본값 변경은 사용자 판단 사항이라 하지 않았습니다.** 선택지는 셋입니다.

1. 현행 유지(vsync). 원본 아케이드 하드웨어에 충실하지만 30 fps로 양자화됩니다.
2. interval 0 기본. +62% 프레임, tick 전달률 99.4%. 티어링 가능.
3. 프레임을 60 Hz 마감 안으로 넣기. 근본적이지만 CPU 축 작업이 선행돼야 합니다.

측정 축은 이제 명확합니다. **interval 0 기준으로 unaccounted가 76.93%**이므로,
다음은 그 안에 숨어 있는 **미계측 커널 예외 전달 비용**입니다. 이후의 모든 성능
측정은 interval 0으로 고정해야 유휴 대기가 섞이지 않습니다.

---

## English

### Verdict: the run was display-limited

Over a fixed 70 seconds on the same build with only `REPIU_GLIDE_SWAP_INTERVAL`
changing, frames went 2,124 at vsync, 2,175 at adaptive, and **3,444 at immediate —
plus 62.1%**. Present work per call collapsed from 10,210,872 to **158,506 cycles**,
matching the 162,694 measured before Task 370 and explaining why the present looked
cheap then. Adaptive applied (`applied=true`, `effective=-1`) but achieved nothing,
so tear control is not effectively working on this driver. The Glide gate fell from
20.20% to 12.52% of wall and unaccounted rose to 76.93%.

### Game speed did not change

The design required reading timer delivery alongside frames, since this is a rhythm
game. Ticks due were essentially identical (16,273 against 16,271), so game time is
not accelerating — the demand is wall-clock driven. Delivery nonetheless improved
from 91.9% to **99.4%**, with coalescing dropping from 1,279 to 51: turning vsync
off does not merely raise frames, it lets the guest's timer path run more
faithfully.

### This rewrites the attribution

At interval 1 the present spent roughly 2.77 ms per frame idle, and that time was
counted as `grBufferSwap` gate time, inflating the Glide share. Measured without
vsync, the Glide gate is 12.52% of wall and guest execution dominates more clearly
than any previous capture suggested. The 30.3 fps observed under vsync is the
classic double-buffered half-rate quantisation: missing a 60 Hz deadline costs the
whole next refresh.

### A confirmed side finding

The guest's `grBufferSwap` interval argument has never been applied — the backend
recorded it and never called `SDL_GL_SetSwapInterval` — so every capture so far ran
under SDL's or the driver's default rather than the guest's request.

### Implementation and verification

A new `glide_swap_interval_policy` module resolves `-1` through `4`, rejecting
out-of-range and whitespace-padded values; the backend applies it once after
`SDL_GL_MakeCurrent` and makes no SDL call when unset, so the default path is
untouched. The summary reads the effective value back from the driver rather than
echoing the request. Both configurations build, the probe exits zero in both with
all four new assertions true.

### Open decision and next step

The default is deliberately unchanged: keeping vsync is faithful to the original
arcade hardware but quantises to 30 fps; forcing interval 0 gains 62% of frames and
a 99.4% tick delivery rate at the cost of possible tearing; and fitting the frame
inside the 60 Hz deadline is the real fix but needs the CPU axis first. That axis is
now unambiguous — unaccounted is 76.93% at interval 0 — so the unmeasured kernel
exception-delivery cost is next, and every future performance measurement should
pin interval 0 so idle waiting does not contaminate it.
