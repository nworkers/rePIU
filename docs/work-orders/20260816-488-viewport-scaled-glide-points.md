# Viewport 비례 Glide point 크기 작업 지시

관련 설계: [Viewport 비례 Glide point 크기 설계](../design/20260816-488-viewport-scaled-glide-points.md)

## 작업

1. drawable/logical 비율에서 정사각형 point 배율을 계산하는 순수 함수를 추가합니다.
2. viewport 갱신 시 OpenGL 지원 범위로 clamp한 point 크기를 저장합니다.
3. 직접·batch `GL_POINTS` 공용 상태 경로에서 `glPointSize()`를 적용합니다.
4. `glide_render_probe`에 1×, 2×, 3×, 비균등, 축소·무효 크기 검증을 추가합니다.
5. Release 빌드와 probe를 실행하고 architecture, analysis, 작업 로그를 갱신합니다.

## 완료 조건

* 1280×960 drawable에서 640×480 guest point가 2×2 physical pixel로 그려집니다.
* resize 후 다음 point draw부터 새 배율이 적용됩니다.
* 기존 draw state와 전체 `glide_render_probe`가 통과합니다.

# Viewport-Scaled Glide Point Size Work Order

Related design: [Viewport-Scaled Glide Point Size Design](../design/20260816-488-viewport-scaled-glide-points.md)

## Work

1. Add a pure square-point scale calculation from drawable/logical ratios.
2. Clamp and store point size against the OpenGL-supported range on viewport updates.
3. Apply `glPointSize()` in the common direct/batched `GL_POINTS` state path.
4. Cover 1×, 2×, 3×, non-uniform, downscaled, and invalid sizes in `glide_render_probe`.
5. Build Release, run the probe, and update architecture, analysis, and the work log.

## Completion criteria

* A 640×480 guest point occupies 2×2 physical pixels in a 1280×960 drawable.
* A resize affects the next point draw.
* Existing draw state and the complete `glide_render_probe` pass.
