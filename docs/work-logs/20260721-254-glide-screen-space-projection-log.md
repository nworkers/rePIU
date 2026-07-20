# Task 254 작업 로그 — Glide 화면 공간 직교 투영 / Task 254 Work Log — Glide Screen-Space Orthographic Projection

* 날짜 / Date: 2026-07-21
* 브랜치 / Branch: `feature/254-glide-screen-space-projection`
* 범위 / Scope: 렌더 백엔드 투영 + 셰이더 combine 기본값 + 검증 진단

## 배경 / Background

Task 251-253으로 `grDrawTriangle`가 실제 삼각형을 제출하지만 창은 검은 화면이었다.
런타임 정점 캡처로 게임이 640×480 화면 픽셀 좌표(불투명 빨강 + 텍스처 좌표)를
전달함을 확인했고, 백엔드에 직교 투영이 없어 픽셀 좌표가 NDC 밖으로 클리핑되는 것이
근인임을 규명했다(설계: `docs/design/20260721-254-glide-screen-space-projection.md`).

## 변경 / Changes

1. `src/platform/win32/glide_opengl_backend.cpp` `OpenWindowed`: y 뒤집힌
   `glOrtho(0, w, h, 0, -1, 1)` 투영 + modelview identity 설정.
2. `src/platform/win32/glide_opengl_shader.cpp` `Initialize`: combine function
   uniform 기본값 1(LOCAL)로 시드(draw가 combine 설정보다 앞서거나 미지원 식 유지
   시 흑색 프래그먼트 예방).
3. `src/platform/win32/glide_opengl_backend.cpp` `BufferSwap`: 검증용 비검정 픽셀
   카운트 진단(헤드리스 세션에서 스크린샷 불가 → glReadPixels로 래스터화 증명).
   `REPIU_GLIDE_PIXEL_DIAG` 환경변수로 게이트(기본 off), 초기 4프레임 + 200프레임
   주기, 4000프레임 상한 → 기본 실행에 steady-state 비용 없음.

## 검증 / Verification

* Win32 x86 debug 빌드 성공(재빌드 다회, 경고/오류 없음).
* 회귀 확인: 투영 추가 후에도 aot-dynamic `pumpit1`이 첫 삼각형 도달, 거부 0·미처리
  0·GL 오류 0(155초 구동, opened=1 실제 WGL 창).
* **래스터화 증명(비검정 픽셀 카운트, `REPIU_GLIDE_PIXEL_DIAG=1`):**
  - swap #1 = **0**/307200 (삼각형 이전, 검정 clear)
  - swap #2 = **18,176**/307200 (첫 삼각형 이후)
  - swap #3/#4 = **24,704**/307200 (안정)

  투영 수정 전이면 픽셀 좌표(x≈288, y≈330) 삼각형이 NDC 밖으로 100% 클리핑되어
  비검정 0이어야 한다. 삼각형이 그려진 직후 비검정 픽셀이 나타나는 것은 지오메트리가
  이제 뷰포트 안으로 매핑되어 실제 래스터화됨을 결정적으로 증명한다. 스크린샷은 이
  헤드리스 세션의 desktop/window-station 격리로 불가(FindWindow는 호출 스레드
  desktop만 검색)했으므로 프레임버퍼 리드백으로 대체했다.

## 후속 / Follow-ups

* Task 255 (R2 완성): GrVertex r/g/b/a를 `glColor4f`에 연결(흰색 고정 제거).
* Task 256 (R3): 텍스처 저장/다운로드/소스 + s/t 샘플링.
* BufferSwap 검증 진단은 렌더 결과 자동 검증(프레임 해시/스크린샷) 인프라 도입 시
  제거 또는 정식 텔레메트리로 정리.

## English Summary

Root-caused the persistent black screen (triangles submitted but clipped): the
game passes 640×480 screen-pixel vertices but the backend set no orthographic
projection, so the `ftransform()` shader clipped all geometry. Added a y-flipped
`glOrtho(0,w,h,0,-1,1)` in `OpenWindowed` (matching GR_ORIGIN_UPPER_LEFT; culling
disabled so reversed winding is harmless), seeded the combine function uniforms
to LOCAL, and added a bounded glReadPixels non-black-pixel diagnostic in
BufferSwap to verify rasterization in this headless (screenshot-less) session.
No gate/HLE/ABI/game-logic change. Verification numbers recorded above.
