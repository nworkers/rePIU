# Task 433 작업 로그 — 깊이가 한 번도 연결된 적이 없었습니다

설계: [20260806-433](../design/20260806-433-glide-vertex-depth.md) ·
작업 지시: [20260806-433](../work-orders/20260806-433-glide-vertex-depth.md)

## 1. 한 줄 결과

3D 모델 깨짐의 원인은 **정점 깊이가 파이프라인 전 구간에서 끊겨 있던 것**입니다. 게임이
`ooz`를 제대로 채우는데(gameplay 정점의 **95.4%**, 범위 0.75~65,426) 디코더가 읽지 않고
모든 정점이 `z=0`으로 나가고 있었습니다. **사용자 육안 확인 대기 중입니다.**

## 2. 증상이 그 화면이 되는 이유

게임은 `grDepthBufferMode(1)`을 부르고 우리는 `glEnable(GL_DEPTH_TEST)`를 정직하게
켭니다. **깊이 테스트는 켜져 있는데 모든 삼각형이 같은 평면**이므로 한 픽셀을 먼저
차지한 삼각형이 이기고 나머지가 기각됩니다. 제출 순서 ≠ 깊이 순서이므로 모델이
뒤섞입니다. 2D(화살표·점수·배경)가 멀쩡한 것도 같은 이유입니다 — 원래 한 평면입니다.

**회귀가 아닙니다.** 최근 타이머 작업과 무관하며, 깊이는 한 번도 연결된 적이 없습니다.

## 3. 측정으로 후보 셋을 배제했습니다

| 후보 | 측정 | 판정 |
|---|---|---|
| 깊이 버퍼 미할당 | `grSstWinOpen` nAuxBuffers=**1** → 요청 24 / **granted 24** | 배제 |
| W-buffer 미지원 | `grDepthBufferMode`=**1**(ZBUFFER), `unsupported` **0건** | 해당 없음 |
| 비교 함수 매핑 오류 | `glDepthFunc(GL_NEVER + function)` — `GR_CMP_*`(0~7)와 `GL_NEVER`~`GL_ALWAYS`가 **순서 일치** | 이미 정확 |

**세 번째가 설계를 결정적으로 단순화했습니다.** 비교 함수가 이미 직결이므로 **깊이 값의
순서만 보존하면 비교 결과가 그대로 옮겨집니다.** 게임이 어떤 함수를 고르는지도, 어느
쪽이 "가까움"인지도 따질 필요가 없어졌습니다 — 깊이 버퍼는 숫자를 저장하고 같은 함수로
비교할 뿐이고, 순서가 같으면 결과가 같습니다.

## 4. 어느 필드가 깊이인가 — 재서 정했습니다

gameplay 표본 **1,670,434개**:

| 필드 | meaningful(`\|v\|>1e-6`) | 비율 | 범위 |
|---|---:|---:|---|
| `z`[2] | 19,082 | 1.1% | [1.096e-05, 1.096e-05] — **상수 하나** |
| **`ooz`[6]** | **1,593,672** | **95.4%** | **[0.748, 65426.35]** |
| `oow`[8] | 1,670,434 | 100% | [1.78e-05, 1.0] |

**상한 65,426이 Glide 16비트 깊이 범위 65,535에 맞닿습니다** — 게임이 `ooz = 65535/z`를
계산한다는 교과서적 확인입니다. `z`는 단일 상수라 깊이가 아닙니다.

## 5. 계측을 두 번 고쳐야 했습니다

**(a) attract 표본으로는 판정이 불가능했습니다.** 첫 실행(30초)은 attract까지만 갔고
표본 55,776개에 **문제의 3D 모델 정점이 0개**였습니다. 대상이 아닌 기하로 매핑을 정할
뻔했습니다. gameplay 표본은 1,670,434개로 30배입니다.

**(b) "비영" 기준이 틀렸습니다.** 게스트가 쓴 적 없는 필드는 깨끗한 0이 아니라
**denormal**(`2.4e-38`, `3.9e-37` 등)로 읽힙니다. `!= 0`이 참이라 첫 판에서 `z`가
48,874개 "비영"으로 세어졌는데, 실제로는 전부 쓰레기였습니다. `|v| > 1e-6` 기준을
넣자 `z`는 1.1%로 내려앉았습니다. **판정표의 분모가 잘못돼 있었던 셈입니다** — Task 431의
분모 오류와 같은 부류이고, 이번에도 판정 전에 잡았습니다.

## 6. 수정

| 지점 | 내용 |
|---|---|
| `glide_vertex.h` | `GlideDrawVertex::ooz` 추가 |
| `glide_vertex.cpp` | `fields[6]` 디코드, meaningful 미만은 0으로 접음 |
| `glide_opengl_backend.cpp` | `GlideOozToOrthoEyeZ` → `glVertex3f`의 z |

```
d      = clamp(ooz, 0, 65535) / 65535      // 단조 증가
z_eye  = 1 - 2d                             // glOrtho(0,w,0,h,-1,1)에서 window depth = (1-z_eye)/2
```

65,535 포화은 Glide 16비트 깊이 버퍼의 **하드웨어 동작 그대로**입니다. 정점은 `ooz`를
원본으로 싣고 정규화는 backend에서 합니다(플랫폼 분리).

**투영은 바꾸지 않았습니다.** `d=0.5`가 `z_eye=0`이므로 깊이를 싣지 않는 경로의 동작이
그대로 유지됩니다. near/far를 건드리면 그 경로까지 함께 움직입니다.

## 7. 검증

빌드 통과, 스모크에서 `unimplemented/unsupported` **0/0**, GL 오류 0, 1,293프레임.

**미완 — 육안 확인이 완료 판정입니다.** 3D 모델은 gameplay에 나오므로 제 스모크로는
확인할 수 없습니다. 스모크가 보증하는 것은 "깨지지 않고 돈다"까지입니다.

## 8. 회고

* **"크래시 없음 ≠ 정확 동작"이 또 성립했습니다.** 깊이는 한 번도 연결된 적이 없는데
  실행은 내내 멀쩡했고, 2D만 보면 화면도 그럴듯했습니다.
* **먼저 재고 나중에 고른 것이 맞았습니다.** `z`와 `ooz` 중 직관으로 골랐다면 절반 확률로
  틀렸고, 화면은 다른 방식으로 깨졌을 뿐이라 원인을 되짚기 어려웠을 것입니다.
* **계측 자체를 두 번 고쳤습니다**(§5). 구간이 틀렸고 기준이 틀렸습니다. 둘 다 판정
  **전에** 잡혔는데, 판정 규칙을 측정 전에 적어 둔 덕분에 "이 표본으로 이 규칙을 적용할
  수 있는가"를 물을 수 있었습니다.

---

# Task 433 Work Log — depth had never been connected

## 1. Result in one line

The 3D model corruption comes from **vertex depth being severed at every stage of the
pipeline**. The game populates `ooz` properly — **95.4%** of gameplay vertices, ranging
0.75-65,426 — but the decoder never read it and every vertex left as `z = 0`. **Awaiting the
user's visual confirmation.**

## 2. Why that produces this screen

The game calls `grDepthBufferMode(1)`, so we honestly enable `GL_DEPTH_TEST` — and with the test
enabled but **every triangle on one plane**, whichever triangle reaches a pixel first keeps it.
Submission order is not depth order, so the model scrambles, while the 2D layer is unaffected
because it was always one plane. **Not a regression**, and unrelated to the timer work: depth has
never been connected.

## 3. Three candidates excluded by measurement

The depth buffer is not missing (`grSstWinOpen` passes nAuxBuffers=**1**; 24 bits requested,
**24 granted**); W-buffer support is not needed (`grDepthBufferMode` is **1**, zero `unsupported`
reports); and the comparison mapping is already correct, since `glDepthFunc(GL_NEVER + function)`
works off `GR_CMP_*` and `GL_NEVER`..`GL_ALWAYS` sharing an order. **That third point simplified
the design decisively:** with the comparison already direct, **preserving only the ordering of
the depth value carries the comparison result across** — so neither which function the game picks
nor which end counts as "near" ever has to be decided.

## 4. Which field carries depth — measured, not assumed

Over **1,670,434** gameplay samples, `z`[2] is meaningful in 1.1% at a **single constant value**,
`oow`[8] in 100% across 1.78e-05..1.0, and **`ooz`[6] in 95.4% across 0.748..65,426.35**. That
ceiling meeting Glide's 16-bit depth range of 65,535 is a textbook confirmation that the game
computes `ooz = 65535/z`.

## 5. The instrument needed fixing twice

**(a) The attract sample could not decide anything.** The first 30-second run reached only the
attract demo, and its 55,776 samples contained **zero vertices of the model in question** — the
mapping was nearly chosen from geometry that was not the subject. The gameplay sample is thirty
times larger. **(b) The "non-zero" test was wrong.** A field the guest never wrote reads back as
**denormal** rather than clean zero, so `!= 0` counted 48,874 `z` samples as populated when all
of them were garbage; a `|v| > 1e-6` floor dropped `z` to 1.1%. **The verdict's denominator was
wrong** — the same family of error as Task 431's, and caught before the verdict again.

## 6. The fix

`GlideDrawVertex` gains `ooz`, the decoder reads `fields[6]` and folds sub-meaningful values to
zero, and the backend maps `d = clamp(ooz, 0, 65535) / 65535` to `z_eye = 1 - 2d`, since window
depth under `glOrtho(0, w, 0, h, -1, 1)` is `(1 - z_eye)/2`. Saturating at 65,535 is the Glide
depth buffer's **own hardware behaviour**. The vertex carries raw `ooz` with normalisation in the
backend, keeping the platform mapping out of the HLE. **The projection is deliberately unchanged**
— `d = 0.5` is `z_eye = 0`, so every path carrying no depth behaves exactly as before.

## 7. Verification

The build passes and a smoke run reports `unimplemented/unsupported` at **0/0** with no GL errors
over 1,293 frames. **Incomplete — the visual check is what closes this**, since the model appears
only in gameplay; the smoke establishes only that nothing broke.

## 8. Retrospective

"No crash is not correct behaviour" held again: depth was never connected, yet the run was
healthy throughout and the 2D layer looked plausible. Measuring before choosing was right — a
coin flip between `z` and `ooz` would have broken the screen a different way and made the cause
harder to trace back. And the instrument itself was wrong twice, in its window and in its
threshold; both were caught **before** the verdict, because writing the readings down first makes
"can this rule be applied to this sample?" a question you actually ask.
