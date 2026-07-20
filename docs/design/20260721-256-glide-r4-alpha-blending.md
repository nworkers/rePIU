# Glide R4 알파 블렌딩 설계 / Glide R4 Alpha Blending Design

* 작성일 / Date: 2026-07-21 (Task 256)
* 선행 / Predecessor: R3 텍스처 경로(Task 255)
* 상태 / Status: 관측 완료 → 구현 / Observed → Implement

## 1. 배경 및 근인 / Background and Root Cause

R3에서 콘텐츠의 SCALE_OTHER 텍스처 draw 중 투명 텍스처(addr=8, texel a=0)가
불투명 검정으로 렌더됐다. 근인은 **알파 블렌딩 미구현**이다: `SetAlphaBlend`가
ONE,ZERO(블렌딩 off)만 수용하고 그 외는 "unsupported"로 유지(retain)했다.

R3 left transparent SCALE_OTHER textures (addr=8, texel a=0) as opaque black
because alpha blending was unimplemented: `SetAlphaBlend` accepted only ONE,ZERO
and retained everything else as "unsupported".

## 2. 관측 (확인됨, REPIU_GLIDE_TEX_DIAG) / Observation

`grAlphaBlendFunction`(rgb_src, rgb_dst, a_src, a_dst) 관측값 2종:

| 인자 | Glide 의미 | 용도 |
|---|---|---|
| (4,0,4,0) | ONE, ZERO, ONE, ZERO | 블렌딩 없음(불투명) |
| (1,5,4,0) | SRC_ALPHA, ONE_MINUS_SRC_ALPHA, ONE, ZERO | 표준 투명 블렌딩 |

콘텐츠 알파 combine은 `grAlphaCombine(3,1,·,1,0)`=SCALE_OTHER=**텍스처 알파**이고
알파 테스트는 항상 7(ALWAYS). 즉 콘텐츠 draw의 프래그먼트 알파는 텍스처 알파이며,
SRC_ALPHA 블렌딩으로 배경에 합성되어야 한다. R3 셰이더는 이미 텍스처 RGBA를
출력하므로(SCALE_OTHER 색·알파), 블렌딩만 켜면 투명 부분이 올바르게 합성된다.

## 3. 설계 / Design

`GlideOpenGlBackend::SetAlphaBlend`를 일반화한다:

* Glide blend factor를 GL factor로 매핑한다(src/dst 문맥 구분):
  * 0 ZERO→GL_ZERO, 1 SRC_ALPHA→GL_SRC_ALPHA, 3 DST_ALPHA→GL_DST_ALPHA,
    4 ONE→GL_ONE, 5 ONE_MINUS_SRC_ALPHA→GL_ONE_MINUS_SRC_ALPHA,
    7 ONE_MINUS_DST_ALPHA→GL_ONE_MINUS_DST_ALPHA,
    15 ALPHA_SATURATE→GL_SRC_ALPHA_SATURATE.
  * 2/6은 src와 dst에서 의미가 다르다(Glide 동일 값): src 2→GL_DST_COLOR,
    dst 2→GL_SRC_COLOR; src 6→GL_ONE_MINUS_DST_COLOR, dst 6→GL_ONE_MINUS_SRC_COLOR.
* `rgb_source==ONE && rgb_destination==ZERO`이면 `glDisable(GL_BLEND)`(불투명 빠른 경로).
  그 외는 `glEnable(GL_BLEND); glBlendFunc(map_src(rgb_source), map_dst(rgb_destination))`.
* 알파 채널 factor는 GL_BLEND의 단일 `glBlendFunc`로 RGB와 공유한다(관측된
  a_src/a_dst=ONE,ZERO는 표시에 영향 없음; `glBlendFuncSeparate`는 GL 1.4 필요로
  현 단계에서 불필요).
* 미지원(알 수 없는) factor는 기존 유지 정책대로 상태 보존 + stdcall 반환(경계
  누수 방지).

경계 핸들러(`_GRALPHABLENDFUNCTION@16`)의 유지 정책은 그대로 두되, 이제 더 많은
함수가 실제 적용되므로 "unsupported"는 진짜 미지원 factor에만 남는다.

## 4. 검증 / Verification

- Win32 x86 debug 빌드.
- `REPIU_GLIDE_PIXEL_DIAG=1`로 블렌딩 적용 후 비검정 픽셀·평균색 관측. 기대: 투명
  텍스처 draw가 검정으로 칠하지 않아 배경/불투명 콘텐츠가 올바르게 남는다(검정
  영역 감소 또는 색 분포 변화). 거부 0·미처리 0·GL 오류 0·크래시 없음.
- 블렌딩은 draw 순서에 의존하므로, 결과는 게임 draw 순서에 따른다. 최소한 회귀(전체
  검정)가 없고 파이프라인이 안정적이어야 한다.

## 5. 파일 / Files

- `src/platform/win32/glide_opengl_backend.cpp`: `SetAlphaBlend` 일반화 + factor 매핑.
- 관측 계측은 `linexe_glide_boundary.cpp`의 기존 env-gated tex-diag 재사용(추가 게이트
  이름 포함).

## English Summary

R3's transparent SCALE_OTHER textures rendered as opaque black because alpha
blending was unimplemented. Observation confirmed the game uses two blend
functions — ONE,ZERO (opaque) and SRC_ALPHA/ONE_MINUS_SRC_ALPHA (transparency) —
with SCALE_OTHER alpha combine (texture alpha). Generalize `SetAlphaBlend` to map
Glide blend factors to GL and enable/disable GL_BLEND accordingly; the R3 shader
already outputs texture RGBA, so enabling blending composites transparent texels
correctly. Verify via the pixel diagnostic that transparent draws no longer paint
black and the pipeline stays stable with no rejects/GL errors.
