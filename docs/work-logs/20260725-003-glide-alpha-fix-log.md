# Work Log: 20260725-003-glide-alpha-fix

## 작업 요약
Glide HLE 환경에서 투명도(Alpha) 및 알파 테스트(Alpha Test)가 정상적으로 적용되지 않는 문제를 해결했습니다. 기존 Fragment Shader의 텍스처-버텍스 결합 로직 부재와 백엔드의 알파 테스트 제한을 개선했습니다.

## 상세 변경 내역
1. **GLSL Shader 결합 로직 개선 (`glide_opengl_shader.cpp`)**:
   - `repiuColorFunction` 및 `repiuAlphaFunction`이 3(SCALE_OTHER)일 때, 텍스처 색상과 버텍스(`repiuIteratedColor`)의 값을 곱하도록 Fragment Shader 텍스트를 재작성했습니다.
   - `SetColorCombine` 및 `SetAlphaCombine`에서 `function == 3U`인 경우를 유효한 입력으로 간주하도록 예외 조건을 완화했습니다.

2. **Alpha Test 연산자 매핑 지원 (`glide_opengl_backend.h`, `glide_opengl_backend.cpp`)**:
   - `GlideOpenGlBackend`에 `SetAlphaTestReferenceValue` 메서드를 추가하고, `alpha_test_reference_` 변수에 [0, 1] 범위로 스케일링된 레퍼런스 값을 저장하도록 구현했습니다.
   - `SetAlphaTestFunction`에서 Glide의 `GrCmpFnc_t` (0~7)를 OpenGL의 `GL_NEVER` ~ `GL_ALWAYS` 연산자로 정확히 매핑하여 `glAlphaFunc`를 호출하도록 수정했습니다.

3. **Boundary 및 State 업데이트 (`linexe_glide_boundary.cpp`, `glide_hle.cpp`, `glide_hle.h`)**:
   - `GlideState` 구조체에 `alpha_test_reference` 변수를 추가했습니다.
   - `glide_hle.cpp`의 `kObservedSignatures` 배열 크기를 97에서 98로 확장하고 `_GRALPHATESTREFERENCEVALUE@4` 수출 함수(Export function)를 등록했습니다.
   - `linexe_glide_boundary.cpp`에 `_GRALPHATESTREFERENCEVALUE@4` 게이트 핸들러를 추가해 HLE 백엔드로 값을 전달하도록 구현했습니다.

## 검증 결과
- CMake 및 MSBuild 빌드를 수행하여 모든 코드 변경 사항이 에러 없이 컴파일되고 링킹(Linking)에 성공함을 확인했습니다.
- 해당 작업을 포함한 `feature/glide-alpha-fix` 브랜치의 모든 변경 사항을 성공적으로 커밋했습니다.
