# Task 433 설계 — Glide 정점 깊이를 파이프라인에 연결

**증상(사용자 보고 + 스크린샷, 2026-08-06):** gameplay의 **3D 댄서 모델이 깨져 보입니다.**
2D인 화살표·점수·배경은 정상입니다.

## 1. 확인됨 — 깊이가 파이프라인 전 구간에서 끊겨 있습니다

```mermaid
flowchart LR
    G["게스트 GrVertex<br/>15 dword"] -->|"z[2], ooz[6]<br/><b>읽지 않음</b>"| D["DecodeGlideProducerVertex"]
    D --> S["GlideDrawVertex<br/><b>깊이 멤버 없음</b>"]
    S -->|"glVertex3f(x, y, <b>0.0F</b>)"| GL["OpenGL"]
    style D fill:#c0392b,color:#fff
    style S fill:#c0392b,color:#fff
```

| 지점 | 근거 |
|---|---|
| 디코더가 `z`[2]·`ooz`[6]를 읽지 않음 | `glide_vertex.cpp` |
| 구조체에 깊이 멤버 없음 | `glide_vertex.h:20~32` |
| 모든 정점이 z=0 | `glide_opengl_backend.cpp:1198` |

**왜 정확히 그 화면인가:** 게임이 `grDepthBufferMode(1)`을 부르므로 우리는
`glEnable(GL_DEPTH_TEST)`를 정직하게 켭니다(`:1857`). **깊이 테스트는 켜져 있는데 모든
삼각형이 같은 평면**이므로, 한 픽셀을 먼저 차지한 삼각형이 이기고 나머지는 기각됩니다.
제출 순서 = 깊이 순서가 아니므로 모델이 뒤섞입니다. 2D가 멀쩡한 것도 같은 이유입니다.

**회귀가 아닙니다.** 깊이는 한 번도 연결된 적이 없습니다.

## 2. 측정으로 배제된 것

| 후보 | 측정값 | 판정 |
|---|---|---|
| 깊이 버퍼 미할당 | `grSstWinOpen` **nAuxBuffers=1** → 요청 24비트 / **실제 24비트** | **배제** |
| W-buffer 미지원 | `grDepthBufferMode` = **1**(ZBUFFER), `unsupported` 0건 | **해당 없음** |
| 비교 함수 매핑 오류 | `glDepthFunc(GL_NEVER + function)`은 `GR_CMP_*`(0~7)와 `GL_NEVER`~`GL_ALWAYS`가 **순서 일치** | **이미 정확** |

**비교 함수가 이미 옳다는 것이 설계를 단순하게 만듭니다.** 깊이 **값의 순서**만 보존하면
게임이 어떤 함수를 고르든 의미가 따라옵니다 — 어떤 함수를 쓰는지 알 필요가 없습니다.

## 3. 남은 미지수 — 어느 필드가 깊이를 싣는가

Glide `GrVertex`(15 dword) 중 깊이 후보는 둘입니다.

| index | 필드 | 통상 의미 |
|---|---|---|
| [2] | `z` | ZBUFFER 모드에서 쓰이기도 함 |
| [6] | `ooz` | `65535/Z` — 클수록 가까움 |
| [8] | `oow` | `1/W` — WBUFFER 모드용(이 게임은 미사용) |

**어느 쪽이 채워져 있는지 미측정입니다.** 비어 있는 필드로 매핑하면 화면이 다른 방식으로
깨질 뿐입니다. 이번 세션에서 **그럴듯한 산술로 예상을 세웠다가 틀린 전례**(Task 430 §10)가
있으므로 재고 결정합니다.

## 4. 계측 — 정점 깊이 필드 census

`REPIU_GLIDE_VERTEX_DEPTH_CENSUS=1`이면 draw 경로에서 정점 깊이 후보 세 필드를 표본해
종료 시 요약합니다. **동작은 바꾸지 않습니다.**

| 필드 | 왜 필요한가 |
|---|---|
| `z`·`ooz`·`oow` 각각의 min/max | 어느 필드가 채워져 있는지, 범위가 얼마인지 |
| 각 필드의 **meaningful 표본 수**(`\|v\| > 1e-6`) | 게스트가 쓴 적 없는 필드는 깨끗한 0이 아니라 **denormal 쓰레기**(`1e-37` 등)로 읽힙니다. `!= 0`으로는 "채워짐"과 "방치됨"이 구분되지 않아 판정이 성립하지 않습니다 |
| 각 필드가 0이 아닌 표본 수 | 위와 대조용(denormal 포함) |
| 표본 총수 | 위 비율의 분모 |
| 처음 N개 원본 표본 | 요약이 이상할 때 되짚을 근거 |

**판정 규칙(측정 전 고정):**

**표본은 gameplay 구간에서 떠야 합니다.** attract 구간에는 문제의 3D 모델이 없으므로,
그 표본으로 매핑을 정하면 대상이 아닌 기하로 결정하게 됩니다(2026-08-06 실측: attract
30초 표본 55,776개에 모델 정점 0개).

| 관측(gameplay 구간) | 결론 |
|---|---|
| `ooz` meaningful이 다수 | **`ooz`를 깊이로 사용.** `d = clamp(ooz, 0, 65535)/65535` |
| `ooz` meaningful ≈ 0이고 `z`가 다수 | **`z`를 사용.** 범위를 보고 정규화 방식 결정 |
| 둘 다 다수 | 범위로 판단하고 근거를 기록. 임의로 고르지 않음 |
| 둘 다 ≈ 0 | **깊이가 아예 안 실림** — 원인이 다른 곳(다른 draw 진입점·정점 포맷) |

**왜 65535로 정규화하는가:** Glide 깊이 버퍼는 16비트로 0~65535를 담으므로 그 상한에서
포화되는 것이 하드웨어 동작 그대로입니다. 그리고 §2에서 비교 함수 의미가 보존되므로,
**`d`가 단조 증가이기만 하면 물리적 원근 방향을 따질 필요가 없습니다** — 깊이 버퍼는
숫자를 저장하고 같은 함수로 비교할 뿐이고, 순서가 같으면 결과가 같습니다.

## 5. 매핑 설계 (필드 확정 후 적용)

현재 투영은 `glOrtho(0, w, 0, h, -1, 1)`이고, 그 아래에서 window depth는
`(1 − z_eye)/2`입니다. 따라서 원하는 깊이 `d ∈ [0,1]`에 대해 **`z_eye = 1 − 2d`** 를
넘깁니다.

**투영은 바꾸지 않습니다.** `d = 0.5`가 `z_eye = 0`이므로 **깊이를 싣지 않는 기존
경로(2D blit 등)의 동작이 그대로 유지**됩니다. near/far를 건드리면 그 경로까지 함께
움직입니다.

순서 보존이 핵심입니다 — `d`가 Glide 깊이 값의 **단조 증가** 함수이면 §2의 비교 함수
매핑이 의미를 그대로 옮깁니다.

## 6. 이 작업에서 하지 않을 것

* **W-buffer 모드**(`grDepthBufferMode` 2·3·4)는 구현하지 않습니다. 이 게임이 쓰지
  않으므로 근거 없는 코드가 됩니다.
* **cull mode 재검토는 별개 축**입니다. 측정값이 `grCullMode(0)`(disable)이므로 현재
  주석(`:781`)과 일치하며, 깊이를 고친 뒤에도 남는 문제가 있으면 그때 봅니다.
* **투영 near/far 변경**은 하지 않습니다(§5).

---

# Task 433 Design — connect Glide vertex depth to the pipeline

**Symptom (user, with screenshot, 2026-08-06):** the **3D dancer model renders corrupted** in
gameplay, while the 2D arrows, score and background are fine.

## 1. Confirmed — depth is severed at every stage

The decoder never reads `z`[2] or `ooz`[6] (`glide_vertex.cpp`), `GlideDrawVertex` has no depth
member at all (`glide_vertex.h:20-32`), and every vertex is emitted as
`glVertex3f(x, y, 0.0F)` (`glide_opengl_backend.cpp:1198`).

**Why that produces exactly this screen:** the game calls `grDepthBufferMode(1)`, so we honestly
enable `GL_DEPTH_TEST` (`:1857`) — and with the test enabled but **every triangle on the same
plane**, whichever triangle reaches a pixel first keeps it and the rest are rejected. Submission
order is not depth order, so the model scrambles. The same reasoning explains why the 2D layer
is unaffected. **This is not a regression**; depth has never been connected.

## 2. Excluded by measurement

The depth buffer is **not** missing: `grSstWinOpen` passes **nAuxBuffers=1**, and the driver
granted **24 bits against 24 requested**. W-buffer support is **not** required:
`grDepthBufferMode` is **1** (ZBUFFER) with zero `unsupported` reports. And the comparison
mapping is **already correct** — `glDepthFunc(GL_NEVER + function)` works because `GR_CMP_*`
(0-7) and `GL_NEVER`..`GL_ALWAYS` share an order.

**That last point simplifies the design decisively:** preserving only the **ordering of the
depth value** carries the comparison semantics across, so *which* function the game selects
never has to be known.

## 3. The open unknown — which field carries depth

Of the 15-dword `GrVertex`, `z`[2] and `ooz`[6] are the candidates (`oow`[8] serves WBUFFER
mode, unused here). **Which one is populated is unmeasured**, and mapping from an empty field
would merely break the screen differently. Given that a plausible-arithmetic expectation already
proved wrong once this session (Task 430 §10), this is measured rather than assumed.

## 4. Instrumentation — a vertex depth census

Under `REPIU_GLIDE_VERTEX_DEPTH_CENSUS=1`, sample `z`, `ooz` and `oow` on the draw path and
summarise at exit, **changing no behaviour**: min and max per field, the count of **non-zero**
samples per field (so "empty" is distinguishable from "zero is the real value"), the total
sample count as their denominator, and the first N raw samples as something to fall back on if
the summary looks wrong.

**Pre-registered readings:** `ooz` non-zero across roughly 0..65535 means **use `ooz`** with
`d = ooz/65535`; `ooz` all zero with `z` non-zero means **use `z`**, normalising by its observed
range; both non-zero decides on range **with the reasoning recorded** rather than picking one
arbitrarily; and both all zero means depth never arrives at all, putting the cause elsewhere —
another draw entry point or vertex layout.

## 5. The mapping, applied once the field is known

Under the current `glOrtho(0, w, 0, h, -1, 1)`, window depth is `(1 − z_eye)/2`, so a desired
depth `d ∈ [0,1]` is emitted as **`z_eye = 1 − 2d`**. **The projection is deliberately left
alone**: `d = 0.5` maps to `z_eye = 0`, which keeps the behaviour of every path that supplies no
depth (the 2D blits) exactly as it is today, whereas changing near and far would move those too.
Order preservation is the point — as long as `d` is **monotonically increasing** in the Glide
depth value, the comparison mapping from section 2 carries the meaning across unchanged.

## 6. Out of scope

**W-buffer modes** (`grDepthBufferMode` 2, 3, 4) are not implemented, since this title does not
use them and the code would rest on nothing. **Cull mode is a separate axis** — the measured
`grCullMode(0)` matches the existing comment at `:781`, and it is revisited only if something
remains after depth is fixed. And the **projection's near and far planes are not changed**.
