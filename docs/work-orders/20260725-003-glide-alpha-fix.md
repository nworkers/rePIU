# Work Order: 20260725-003-glide-alpha-fix

## 목표
- Glide 에뮬레이션 상에서 게임의 투명도(Alpha) 및 알파 테스트(Alpha Test)가 무시되고 불투명하게 나오는 문제 해결.

## 분석 내용
- 현재 GLSL Fragment Shader는 텍스처가 활성화된 경우 버텍스 컬러/알파값을 무시하고 `texture2D`의 값을 그대로 출력하도록 하드코딩되어 있음.
- `grColorCombine` 및 `grAlphaCombine`에서 `SCALE_OTHER`(3) 방식을 사용할 때 텍스처와 버텍스의 결합을 수행하지 못함.
- `grAlphaTestFunction`에 대해 백엔드가 `ALWAYS`(7) 이외의 모든 옵션을 거부하고 `GL_ALPHA_TEST`를 비활성화함.

## 작업 내용
1. `src/platform/win32/glide_opengl_shader.cpp` 수정:
   - Fragment Shader 소스를 변경하여 `repiuColorFunction` 및 `repiuAlphaFunction`이 3일 때 `texture * repiuIteratedColor` 공식을 적용.
   - `SetColorCombine`, `SetAlphaCombine`에서 `function == 3U`인 경우를 허용.
2. `include/repiu/platform/win32/glide_opengl_backend.h` 수정:
   - `SetAlphaTestReferenceValue(std::uint32_t ref)` 추가.
3. `src/platform/win32/glide_opengl_backend.cpp` 수정:
   - `SetAlphaTestFunction`을 통해 Glide 비교 연산자를 OpenGL 연산자로 매핑(`glAlphaFunc`).
   - `SetAlphaTestReferenceValue` 구현.
4. `src/platform/win32/boundary/linexe_glide_boundary.cpp` 수정:
   - `_GRALPHATESTREFERENCEVALUE@4` 핸들러 추가.

## 검증 계획
- 전체 컴파일 및 링킹(CMake MSBuild) 통과 여부 확인.
