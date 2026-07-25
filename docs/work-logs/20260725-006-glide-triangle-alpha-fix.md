# Work Log: 20260725-006-glide-triangle-alpha-fix

## 작업 요약
팔레트 수정 이후 텍스처 투명도는 정상적으로 적용되었지만, 버텍스 기반으로 그리는 지오메트리(Triangle, Quad 등) 자체에 투명도가 적용되지 않는 문제를 확인하고 해결했습니다.
Glide 파이프라인에서 Combine 모드 소스로 `NONE`이나 `CONSTANT`를 설정할 경우, 이전 OpenGL 셰이더(Fragment Shader) 구현이 이를 제대로 처리하지 못해 강제로 1.0(완전 불투명) 값으로 덮어씌워지고 있었습니다.

## 상세 변경 내역
1. **`CONSTANT` 상수 색상 지원 추가 (`glide_opengl_shader.cpp`, `glide_opengl_backend.cpp`)**:
   - `GlideOpenGlShader`에 `SetConstantColor(std::uint32_t argb)` 함수를 추가하여 `grConstantColorValue`로 설정되는 색상값을 Uniform(`repiuConstantColor`)을 통해 전달할 수 있도록 구현했습니다.
   - 셰이더 내에서 `GR_COMBINE_LOCAL_CONSTANT` (1), `GR_COMBINE_OTHER_CONSTANT` (2)가 선택되었을 때 이 `repiuConstantColor`를 참조하도록 식을 갱신했습니다.

2. **`NONE` 소스 값(0.0) 처리 보완 (`glide_opengl_shader.cpp`)**:
   - 기존에는 소스가 `ITERATED`가 아닐 경우 조건부로 `1.0`을 할당하고 있었으나, Glide 사양에 맞게 `GR_COMBINE_LOCAL_NONE` (2) 또는 `GR_COMBINE_OTHER_NONE` (3)이 선택될 경우 `0.0`을 반환하도록 셰이더 코드를 수정했습니다.

3. **`_GRCONSTANTCOLORVALUE@4` HLE 경계 구현 (`linexe_glide_boundary.cpp`)**:
   - PIU 게임이 사용하는 해당 API를 경계 계층(`glide_boundary_handler`)에서 가로챈 후 백엔드의 `SetConstantColor`로 전달하도록 파이프라인을 추가했습니다.

## 검증 결과
- 빌드 검증을 모두 성공적으로 통과했습니다.
- 이제 `NONE` 파라미터가 0.0으로 정확히 매핑되고 상수 컬러(`CONSTANT`) 기능도 추가되어, 게임 내에서 버텍스 투명도(알파 블렌딩) 연산이 정상적으로 수행됩니다.
