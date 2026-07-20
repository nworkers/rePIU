# Glide R2 정점 색상 및 R3 텍스처 경로 설계 / Glide R2 Vertex Color and R3 Texture Path Design

* 작성일 / Date: 2026-07-21 (Task 255)
* 선행 / Predecessor: Task 254(직교 투영 — 지오메트리 래스터화 확인)
* 상태 / Status: R2 즉시 구현, R3 관측 후 구현 / R2 immediate, R3 observe-then-implement

## 1. 배경 / Background

Task 254로 삼각형이 화면 공간에 래스터화된다(비검정 픽셀 0→18,176 확인). 그러나
현재 `DrawTriangle`는 정점 x/y만 쓰고 색을 흰색으로 고정하며, 텍스처는 전면 no-op이다.
확정된 60바이트 GrVertex 레이아웃(2-TMU)은 정점당 색(r,g,b,a)과 텍스처 좌표(sow,tow)를
포함한다.

Task 254 rasterizes triangles into screen space, but `DrawTriangle` uses only
x/y with a hardcoded white color and textures are no-op. The confirmed 60-byte
2-TMU GrVertex layout carries per-vertex color (r,g,b,a) and texture coordinates
(sow, tow).

## 2. R2 정점 색상 (즉시 구현) / R2 Vertex Color (immediate)

**확인됨.** GrVertex dword 3/4/5/7 = r/g/b/a, 범위 [0..255] float. 첫 삼각형은
(255,0,0,255) = 불투명 빨강. 현재 셰이더는 `repiuColorFunction==1`(LOCAL)일 때
`repiuIteratedColor.rgb`(= `gl_Color`)를 출력하므로, 정점당 `glColor4f`를 실제
정점 색으로 설정하면 보간된 iterated 색이 그대로 출력된다.

**설계.**
- 백엔드 `DrawTriangle` 시그니처를 정점당 위치+색(+텍스처 좌표)을 받도록 확장한다.
  플랫폼 공용 정점 구조 `GlideDrawVertex {x, y; r,g,b,a(0..1); s,t}`를 도입한다.
- 경계 핸들러가 dword 0/1(x,y), 3/4/5/7(r,g,b,a; /255), 9/10(sow,tow)을 디코드해
  3정점을 전달한다.
- 백엔드는 `glColor4f(r,g,b,a)` + `glVertex3f(x,y,0)`를 정점마다 발행한다(흰색 고정
  제거). 텍스처 좌표는 R3에서 사용.

## 3. R3 텍스처 경로 (관측 후 구현) / R3 Texture Path (observe-then-implement)

**미확정 → 관측 필요.** 텍스처를 올바로 반영하려면 실제 호출 파라미터가 필요하다:
1. `grTexDownloadMipMapLevel@32`(8인자): tmu, startAddress, thisLod, largeLod,
   aspectRatio, format, evenOdd, data(guest ptr). — 텍셀 원본과 포맷/크기.
2. `grTexSource@16`(4인자): tmu, startAddress, evenOdd, GrTexInfo*. — 현재 텍스처 선택.
3. `grColorCombine@20`/`grTexCombine@28`: 텍스처를 색과 어떻게 혼합하는지(초기값
   (1,0,0,2,0)=LOCAL은 텍스처 미사용; 콘텐츠 draw는 다른 combine을 쓸 것).

초기화 게이트 로그(96건 캡)는 draw 이전이라 콘텐츠 draw 시점의 combine/texsource를
담지 못한다. 따라서 draw 활성 구간에서 이들 파라미터를 계측(env-gated 로깅)해 실제
포맷·크기·combine 모드를 확정한 뒤 디코드/업로드/샘플링을 구현한다.

### 관측 결과 (확인됨, 2026-07-21, REPIU_GLIDE_TEX_DIAG=1) / Observation results (confirmed)

* **combine 모드 전환:** init은 `grColorCombine(1,0,0,2,0)`=LOCAL(iterated 색)이지만
  콘텐츠 draw는 `grColorCombine(3,1,0/1,1,0)`=**function 3 = SCALE_OTHER, other=1
  = TEXTURE**로 전환한다. 즉 콘텐츠 색은 텍스처에서 온다. `grTexCombine(0,1,0,1,0,0,0)`도
  관측.
* **텍스처 = 1×1 (솔리드 색):** 다운로드는 `startAddress` 0(format 0x0A=RGB565)과
  8(format 0x0C=ARGB4444), 둘 다 `largeLod=0, aspect=3, evenOdd=3`. startAddress
  간격이 8바이트뿐이라 각 텍스처는 매우 작다(1×1, Task 236의 1×1 확인과 일치;
  `width=1<<(lod+aspect-3)` 규약으로 lod=0,aspect=3 → 1×1). 정점 텍스처 좌표
  (sow 72/80, tow 32/48, oow=1)는 1×1에서 wrap로 동일 texel을 샘플하므로 무해하다.
* **grTexSource:** `(tmu=0, startAddress=0|8, evenOdd=3, GrTexInfo*)`. GrTexInfo에서
  format/lod/aspect를 읽어 현재 텍스처를 선택한다.
* **R2 색상 검증(부수 확인):** 정점 색 반영 후 비검정 픽셀의 평균 RGB가 흰색
  (255,255,255)이 아니라 **255,224,46(노랑/금색)** 으로 관측 — iterated 정점 색이
  정상 반영됨.

**결론.** 콘텐츠는 1×1 텍스처를 SCALE_OTHER로 출력한다(솔리드 색 패치). 따라서 R3는
1×1 텍스처의 texel 색을 SCALE_OTHER combine에서 출력하도록 구현하면 게임 의도와
일치한다. 더 큰 텍스처(로고/배경)는 이후 콘텐츠에서 나타날 수 있으므로 임의 크기
샘플링을 지원한다.

**구현 구조(관측 확정 후).**
- 플랫폼 공용 `src/hle/glide_texture_decode.{h,cpp}`: LOD/aspect→(w,h) 계산(기존
  `CalculateGlideTextureMemoryRequired` 규약 재사용), 포맷→RGBA8 디코드
  (RGB565/ARGB4444/ARGB1555/P8 등, Glide 2.4 GrTextureFormat_t).
- 백엔드 텍스처 캐시: startAddress→{RGBA8, w, h}. `grTexDownloadMipMapLevel`이 저장,
  `grTexSource`가 GL 텍스처 생성/바인딩, dirty 시 재업로드.
- 셰이더: texcoord varying + sampler2D + `repiuTextureEnable` uniform 추가. 텍스처
  바인딩 + combine이 텍스처를 쓸 때 `texture2D(...) * iterated`(MODULATE 기본),
  아니면 iterated 색만. s/t는 텍스처 크기로 정규화(sow/oow → s, s/w).

**단계 원칙.** R3는 관측으로 combine/format을 확정하기 전에는 렌더 결과를 바꾸지
않는다(현재 동작 보존). 확정 후 텍스처 샘플링을 활성화한다.

## 4. 검증 / Verification

- R2: 빌드 후 aot-dynamic `pumpit1`, `REPIU_GLIDE_PIXEL_DIAG=1`로 비검정 픽셀 유지
  + 색상 반영을 픽셀 샘플의 채널 분포로 확인(흰색 고정 대비 빨강 등 비회색 존재).
  거부 0·미처리 0·GL 오류 0.
- R3: 관측 로그로 실제 texdownload/texsource/combine 파라미터 확정 → 디코드 단위
  검증 → 텍스처 샘플 후 비검정 픽셀 및 색 분포 변화 확인.

## 5. 파일 배치 / File Layout

- `src/hle/glide_texture_decode.{h,cpp}` (플랫폼 공용, 신규): 포맷/크기 디코드.
- `src/platform/win32/glide_opengl_backend.{h,cpp}`: 정점 색(R2), 텍스처 캐시/바인딩(R3).
- `src/platform/win32/glide_opengl_shader.cpp`: 텍스처 샘플링 셰이더(R3).
- `src/platform/win32/boundary/linexe_glide_boundary.cpp`: 정점/텍스처 게이트 디코드.
