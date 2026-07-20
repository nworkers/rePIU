# Glide 화면 공간 직교 투영 설계 / Glide Screen-Space Orthographic Projection Design

* 작성일 / Date: 2026-07-21 (Task 254)
* 상태 / Status: 설계 → 구현 / Design → Implementation
* 선행 / Predecessor: R2 삼각형 제출(Task 251-253), R0/R1(Task 250)

## 1. 배경 / Background

Task 251-253으로 `_GRDRAWTRIANGLE@12`가 실제 삼각형을 OpenGL 백엔드에 제출한다.
그러나 창은 여전히 검은 화면이다. 240초 직접 구동에서 삼각형은 거부 0건·미처리
0건으로 안정 제출되지만 화면에 나타나지 않는다.

Task 251-253 made `_GRDRAWTRIANGLE@12` submit real triangles to the OpenGL
backend, but the window is still black. A 240 s direct run submits triangles
steadily (zero rejects, zero unhandled) yet nothing appears.

## 2. 근인 / Root Cause

**확인됨 (코드 정독 + 런타임 정점 캡처).** 게임은 Glide 관례대로 **화면 픽셀 좌표**로
정점을 전달한다. 첫 삼각형 3정점의 실측 x/y는 `(288.0, 329.94)`, `(296.0, 329.94)`,
`(288.0, 313.94)`로 640×480 화면 좌표다. 그러나 `GlideOpenGlBackend`는 어디에도
직교 투영(`glOrtho`/`GL_PROJECTION`)을 설정하지 않는다. 정점 셰이더가
`gl_Position = ftransform()`(= `gl_ModelViewProjectionMatrix * gl_Vertex`)를 쓰는데,
투영행렬이 단위행렬이므로 픽셀 좌표(수백)가 NDC `[-1, 1]` 밖으로 나가 **모든
삼각형이 클리핑**된다.

**Confirmed (code reading + runtime vertex capture).** The game passes vertices
in **screen pixel coordinates** per Glide convention. The measured x/y of the
first triangle's three vertices are 640×480 screen coordinates. But the backend
never sets an orthographic projection, and the vertex shader uses
`ftransform()`; with an identity projection, pixel-magnitude coordinates fall
outside NDC `[-1, 1]`, so every triangle is clipped away.

### 관측된 GrVertex 60바이트 레이아웃 (확인됨) / Observed 60-byte GrVertex Layout (confirmed)

첫 삼각형 세 정점의 15 dword(60바이트) stride를 디코드한 결과, 표준 Glide GrVertex
(2 TMU)와 정확히 일치한다:

| dword | 필드 | V0 / V1 / V2 실측 |
|---:|---|---|
| 0 | x (screen) | 288.0 / 296.0 / 288.0 |
| 1 | y (screen) | 329.94 / 329.94 / 313.94 |
| 2 | z (Glide 무시) | (정크) |
| 3 | r [0..255] | 255.0 / 255.0 / 255.0 |
| 4 | g [0..255] | 0.0 / 0.0 / 0.0 |
| 5 | b [0..255] | 0.0 / 0.0 / 0.0 |
| 6 | ooz (65535/z) | (가변) |
| 7 | a [0..255] | 255.0 / 255.0 / 255.0 |
| 8 | oow (1/w) | 1.0 / 1.0 / 1.0 |
| 9 | tmu0.sow | 72.0 / 80.0 / 72.0 |
| 10 | tmu0.tow | 32.0 / 32.0 / 48.0 |
| 11..14 | tmu0.oow, tmu1.* | (가변) |

이 삼각형은 **불투명 빨강(r=255,g=0,b=0,a=255) + 텍스처 좌표**를 가진다. 현재 R2
코드는 x/y만 사용하고 색을 흰색으로 고정한다. 색 반영은 Task 255(R2 완성)에서
다루고, 본 작업은 **투영만** 도입한다(변경 최소화, 단일 책임).

The layout matches the standard 2-TMU Glide GrVertex exactly; the triangle is
opaque red with texture coordinates. Applying color is deferred to Task 255; this
task introduces **projection only**.

## 3. 설계 / Design

### 3.1 직교 투영 설정 / Orthographic Projection Setup

`OpenWindowed`가 WGL 컨텍스트를 활성화한 직후(뷰포트 설정과 함께) 화면 공간 직교
투영을 설정한다:

```
glMatrixMode(GL_PROJECTION);
glLoadIdentity();
glOrtho(0.0, width, height, 0.0, -1.0, 1.0);   // top=0, bottom=height → y-flip
glMatrixMode(GL_MODELVIEW);
glLoadIdentity();
```

* **y 뒤집기 근거:** `grSstWinOpen` origin 인자는 `1`(GR_ORIGIN_UPPER_LEFT)로
  관측됐다. Glide upper-left 원점은 y=0이 화면 상단, y가 아래로 증가한다. OpenGL
  기본은 y=0이 하단이므로, `glOrtho(0, w, h, 0, ...)`로 top/bottom을 뒤집어
  Glide 원점과 일치시킨다.
* **와인딩:** y 뒤집기는 삼각형 와인딩(CCW↔CW)을 반전시키지만, 게임이
  `grCullMode(0)`(GR_CULL_DISABLE)로 컬링을 끄므로 앞/뒷면 구분이 없어 문제되지
  않는다(관측 확인됨).
* **z 범위:** DrawTriangle은 `glVertex3f(x, y, 0)`로 z=0을 쓴다. near=-1, far=1
  범위 안이며 `grDepthBufferFunction(7)`=GR_CMP_ALWAYS로 깊이 비교가 항상 통과한다.

The projection is set right after the WGL context is made current. y is flipped
to honor the observed GR_ORIGIN_UPPER_LEFT; culling is disabled so the reversed
winding is harmless; z=0 sits within the [-1, 1] range with ALWAYS depth compare.

### 3.2 combine uniform 안전 기본값 / Combine Uniform Safe Default (보조)

프래그먼트 셰이더는 `repiuColorFunction == 1`일 때만 iterated 색을 출력한다. 게임은
초기화에서 `grColorCombine(1,0,0,2,0)`/`grAlphaCombine(1,0,0,2,0)`을 호출해 이
uniform을 1로 설정하므로(게이트 로그 #15/#16 관측), draw 시점엔 이미 1이다. 다만
draw가 combine 설정보다 앞서는 경로나 미지원 식 유지 정책 상황을 대비해, 셰이더
Initialize에서 두 function uniform을 1(LOCAL)로 초기화해 흑색 프래그먼트를 예방한다.
이는 게임 로직이나 ABI를 바꾸지 않는다.

The fragment shader needs `repiuColorFunction == 1` to emit iterated color; the
game already sets it to 1 during init, but Initialize seeds both function
uniforms to 1 (LOCAL) defensively to avoid a black fragment if a draw ever
precedes the combine setup. No game logic or ABI change.

## 4. 영향 범위 / Scope

* `src/platform/win32/glide_opengl_backend.cpp` — `OpenWindowed`에 투영 설정 추가.
* `src/platform/win32/glide_opengl_shader.cpp` — Initialize에서 combine function
  uniform 기본값 1 설정(보조).
* 게이트 핸들러·HLE·ABI·게임 로직 변경 없음.

## 5. 검증 / Verification

1. Win32 x86 debug 빌드 성공.
2. aot-dynamic `pumpit1` 직접 구동으로 첫 삼각형 도달(로그 `first triangle`)과
   거부 0건·미처리 0건·OpenGL 오류 0건을 재확인(회귀 없음).
3. 화면 출력 육안/스크린샷 확인: 이전 검정 창 대비 삼각형(현재 흰색)이 화면에
   나타나는지 확인. 스크린샷 자동 캡처가 없으면 창 표시 상태와 스왑 카운트로 보완
   하고, 다음 작업(프레임 해시/스크린샷 경로)을 남긴다.

Verify: build succeeds; a direct aot-dynamic run still reaches the first triangle
with zero rejects/unhandled/GL errors (no regression); and visually confirm that
geometry now appears (white for now) where the window was previously black.

## 6. 후속 / Follow-ups

* Task 255 (R2 완성): GrVertex r/g/b/a를 `glColor4f`에 연결(흰색 고정 제거).
* Task 256 (R3): 텍스처 저장/다운로드/소스 + s/t 샘플링.
* 검증 인프라: 프레임 해시 또는 스크린샷 경로로 렌더 결과를 자동 검증.
