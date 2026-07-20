# Task 256 작업 로그 — Glide R4 알파 블렌딩 / Task 256 Work Log — Glide R4 Alpha Blending

* 날짜 / Date: 2026-07-21
* 브랜치 / Branch: `feature/256-glide-r4-alpha-blending`
* 범위 / Scope: `SetAlphaBlend` 일반화(Glide blend factor→GL 매핑), 블렌드 관측 계측

## 배경 / Background

R3에서 콘텐츠의 투명 SCALE_OTHER 텍스처(addr=8, texel a=0)가 불투명 검정으로
렌더됐다. 근인은 알파 블렌딩 미구현(`SetAlphaBlend`가 ONE,ZERO만 수용)이었다.

## 관측 (확인됨) / Observation

`grAlphaBlendFunction` 2종: (4,0,4,0)=ONE,ZERO(불투명), (1,5,4,0)=SRC_ALPHA,
ONE_MINUS_SRC_ALPHA(투명). 콘텐츠 알파 combine=SCALE_OTHER(텍스처 알파), 알파
테스트=ALWAYS. 즉 콘텐츠는 텍스처 알파 + SRC_ALPHA 블렌딩으로 배경에 합성한다.

## 변경 / Changes

- `src/platform/win32/glide_opengl_backend.cpp` `SetAlphaBlend`: Glide blend
  factor를 GL factor로 매핑(src/dst 문맥 구분: 2/6은 src=DST_COLOR류, dst=SRC_COLOR류).
  ONE,ZERO는 `glDisable(GL_BLEND)`, 그 외는 `glEnable(GL_BLEND)+glBlendFunc`.
- `src/platform/win32/boundary/linexe_glide_boundary.cpp`: 블렌드 관측을 env-gated
  tex-diag에 추가(grAlphaBlendFunction/TestFunction/Combine), cap 256으로 확대.

## 검증 / Verification

- Win32 x86 debug 빌드 성공.
- 블렌딩 적용 후 aot-dynamic `pumpit1` 픽셀 진단(135초): 블렌드 거부 **0**(일반화된
  `SetAlphaBlend`가 (1,5,4,0) SRC_ALPHA 블렌드를 정상 수용), 콘텐츠 swap #3 =
  17,280/307200 비검정, avg-rgb 255,255,0 — **R3와 동일**. 거부 0·미처리 0·GL 오류
  0·크래시 없음.
- **해석:** 이 attract 화면은 투명 draw가 검은 배경 위에 있어, 블렌딩으로 지워지든
  (R4) 불투명 검정으로 칠하든(R3) 픽셀 결과가 동일하다. 따라서 시각적 변화는 없지만
  블렌딩은 올바르게 적용된다. R4의 실질 이득은 **투명 텍스처가 다른 콘텐츠 위에
  겹칠 때** 나타난다: R3는 아래 콘텐츠를 검정으로 덮어썼으나, R4는 텍스처 알파로
  올바르게 합성한다(파괴적 덮어쓰기 제거). 즉 정확성·견고성 개선이며 회귀가 없다.

## 후속 / Follow-ups

- 더 큰 텍스처(로고/배경) 콘텐츠 확인 시 임의 크기 텍스처 렌더 검증.
- `glBlendFuncSeparate`(GL 1.4)로 RGB/알파 분리 블렌드가 필요한 콘텐츠가 나오면 확장.

## English Summary

R3 rendered transparent SCALE_OTHER textures as opaque black because alpha
blending was unimplemented. Observation confirmed two blend functions (ONE,ZERO
opaque and SRC_ALPHA/ONE_MINUS_SRC_ALPHA transparency) with SCALE_OTHER alpha
combine. Generalized `SetAlphaBlend` to map Glide blend factors to GL and
enable/disable GL_BLEND; the R3 shader already outputs texture RGBA, so blending
now composites transparent texels over the background. Verification numbers above.
