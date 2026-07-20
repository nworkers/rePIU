# Task 255 작업 로그 — Glide R2 정점 색상 및 R3 텍스처 경로 / Task 255 Work Log — Glide R2 Vertex Color and R3 Texture Path

* 날짜 / Date: 2026-07-21
* 브랜치 / Branch: `feature/255-glide-r2-r3-color-texture`
* 범위 / Scope: 정점 색상(R2), 텍스처 저장/디코드/업로드/샘플링(R3), 관측 계측

## 배경 / Background

Task 254로 삼각형이 화면에 래스터화되나 x/y만 쓰고 흰색 고정, 텍스처는 no-op이었다.
확정된 60바이트 2-TMU GrVertex는 정점 색(r,g,b,a)과 텍스처 좌표(sow,tow)를 담는다.

## R2 정점 색상 / R2 Vertex Color

- 백엔드 `DrawTriangle`를 정점당 위치+색+텍스처좌표(`GlideDrawVertex`)를 받도록 확장.
- 경계 핸들러가 dword 0/1(x,y), 3/4/5/7(r,g,b,a; /255), 9/10(sow,tow)을 디코드.
- 백엔드가 정점마다 `glColor4f`를 발행(흰색 고정 제거).
- **검증(확인됨):** 비검정 픽셀 평균 RGB가 흰색(255,255,255)이 아닌
  **255,224,46(노랑/금색)** — iterated 정점 색 정상 반영. 회귀 없음(비검정 0→18,176→
  24,704 유지, 거부 0).

## R3 텍스처 경로 / R3 Texture Path

- **관측(env-gated `REPIU_GLIDE_TEX_DIAG`):** 콘텐츠 draw는 `grColorCombine(3,1,·,1,·)`
  =SCALE_OTHER(other=TEXTURE)로 텍스처를 출력(init의 LOCAL과 다름). 텍스처는
  startAddress 0(RGB565)/8(ARGB4444), largeLod=0·aspect=3 → 1×1(솔리드 색).
- **디코드 모듈(플랫폼 공용):** `src/hle/glide_texture_decode.{h,cpp}` — LOD/aspect→(w,h)
  (기존 규약: `1<<(lod+aspect-3)`), 포맷→RGBA8(RGB565/ARGB4444/ARGB1555/P8 등).
- **백엔드 텍스처 캐시:** startAddress→GL 텍스처. `grTexDownloadMipMapLevel`이
  디코드·업로드(`StoreTexture`), `grTexSource`가 현재 텍스처 선택(`SourceTexture`),
  `grColorCombine` function 3이 텍스처 경로 활성화(`SetTextureCombineEnabled`).
- **셰이더:** texcoord varying + `sampler2D` + `repiuTextureEnable` uniform. 활성 시
  `texture2D()` 출력, 아니면 iterated 색. 텍스처 좌표는 텍스처 크기로 정규화.
- **검증(확인됨, `REPIU_GLIDE_TEX_DIAG`/`REPIU_GLIDE_PIXEL_DIAG`):**
  - 텍스처 디코드 정상: `StoreTexture #1 addr=0 format=10(RGB565) 1×1 texel=140,150,148,255`(불투명 회색),
    `#2 addr=8 format=12(ARGB4444) 1×1 texel=0,0,0,0`(투명 검정). 디코드가 포맷별로
    정확히 동작함.
  - 콘텐츠 draw(SCALE_OTHER)가 텍스처를 샘플링: swap #3~#19 안정적으로 17,280/307200
    비검정, avg-rgb **255,255,0**. R2의 255,224,46(정점색)과 다른 값 = 텍스처 경로가
    출력을 구동함.
  - 비검정 픽셀이 R2(24,704)보다 적은 17,280인 이유: R2가 SCALE_OTHER 자리에 정점색을
    칠했던 것을 R3가 게임 의도(투명 텍스처)대로 렌더 → **회귀가 아니라 충실도 개선**.
  - 안정성: 거부 0, 미처리 0, GL 오류 0, 크래시 없음(180초).
- **알려진 한계:** 투명 텍스처(addr=8, a=0)를 반투명 합성하려면 알파 블렌딩이 필요하다
  (현재 관측된 init 블렌드는 ONE,ZERO=off). 반투명 콘텐츠 충실도는 후속(R4)에서
  블렌드 함수 관측 후 구현한다. 현재는 게임 combine 설정에 충실한 불투명 출력이다.

## 파일 변경 / Files

- `src/hle/glide_texture_decode.{h,cpp}` (신규, CMake 등록)
- `include/repiu/platform/win32/glide_opengl_backend.h`, `.cpp` (정점 색·텍스처 캐시)
- `include/repiu/platform/win32/glide_opengl_shader.h`, `.cpp` (텍스처 샘플링 셰이더)
- `src/platform/win32/boundary/linexe_glide_boundary.cpp` (정점/텍스처/combine 게이트)

## 후속 / Follow-ups

- 더 큰 텍스처(로고/배경) 콘텐츠 확인 시 임의 크기 샘플링 검증.
- grTexCombine/grTexFilterMode의 실제 필터·combine 반영(현재 backend 기본값).
- LFB 경로(R4)는 실사용 확인 후.

## English Summary

R2: extend `DrawTriangle` to per-vertex position/color/texcoord, decode r/g/b/a
from the confirmed GrVertex, and emit per-vertex `glColor4f`. Verified: non-black
average RGB is 255,224,46 (colored) instead of white. R3: observed that content
draws use SCALE_OTHER (texture) color combine with 1x1 textures; added a
platform-neutral texture decode module, a backend texture cache fed by
grTexDownloadMipMapLevel and selected by grTexSource, a texture-combine toggle on
grColorCombine function 3, and GLSL texture sampling. Verification numbers above.
