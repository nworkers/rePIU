# Viewport 비례 Glide point 크기 작업 로그

관련 문서: [설계](../design/20260816-488-viewport-scaled-glide-points.md),
[작업 지시](../work-orders/20260816-488-viewport-scaled-glide-points.md)

## 결과

`CalculateGlidePointSize()`가 drawable/logical 가로·세로 비율 중 작은 값을 선택하고 최소
1 pixel을 보장합니다. `ApplyDrawableViewport()`는 OpenGL이 보고한 aliased point 지원
범위로 값을 제한해 저장하며, `PrepareDrawState(GL_POINTS)`가 직접·batch point 양쪽에
`glPointSize()`를 적용합니다. close는 cached point 크기를 1로 초기화합니다.

## 검증

* `cmd /c scripts\build_win32_x86_release.bat repiu_glide_render_probe`: 성공
  * 첫 실행은 컴파일 도중 120초 명령 제한에 걸렸고, 증분 재실행에서 링크까지 성공
* `build\win32_x86_debug\Release\repiu_glide_render_probe.exe`: 종료 코드 0,
  `glide_render_probe=pass`
* `cmd /c scripts\build_win32_x86_release.bat repiu`: 성공
  * `build\win32_x86_debug\Release\repiu.exe` 링크 완료
* 계산 검증: 640×480=1, 1280×960=2, 1920×1440=3, 1280×720=1.5,
  320×240 및 무효 크기=1

실제 `pumpit8` 서비스 화면의 육안 확인은 수행하지 않았습니다. Release 실행 파일로 같은
화면을 확인하면 기본 2× 창에서 각 dot가 2×2 physical pixel로 표시되어야 합니다.

# Viewport-Scaled Glide Point Size Work Log

Related documents: [design](../design/20260816-488-viewport-scaled-glide-points.md),
[work order](../work-orders/20260816-488-viewport-scaled-glide-points.md)

## Result

`CalculateGlidePointSize()` selects the smaller drawable/logical axis ratio and
floors it at one pixel. `ApplyDrawableViewport()` clamps it to the reported
aliased-point range, and `PrepareDrawState(GL_POINTS)` applies `glPointSize()`
to both direct and batched points. Close resets the cached size to one.

## Verification

* `cmd /c scripts\build_win32_x86_release.bat repiu_glide_render_probe`: passed
  * The first invocation reached its 120-second command limit during compilation;
    the incremental rerun completed the link.
* `build\win32_x86_debug\Release\repiu_glide_render_probe.exe`: exit code 0,
  `glide_render_probe=pass`
* `cmd /c scripts\build_win32_x86_release.bat repiu`: passed
  * Linked `build\win32_x86_debug\Release\repiu.exe`.
* Calculation cases: 640×480=1, 1280×960=2, 1920×1440=3,
  1280×720=1.5, and 320×240 or invalid dimensions=1.

The actual `pumpit8` service screen was not visually checked. With the Release
executable, each dot should occupy 2×2 physical pixels in the default 2× window.
