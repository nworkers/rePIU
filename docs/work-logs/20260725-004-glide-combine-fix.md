# Work Log: 20260725-004-glide-combine-fix

## 작업 요약
이전에 수행된 투명도(Alpha) 수정 작업 중 `SCALE_OTHER` 블렌딩 공식을 하드코딩함에 따라 버텍스 컬러가 입력되지 않거나 특정 블렌딩 조합에서 텍스처가 까맣게 렌더링되던 문제(검은 화면)를 수정했습니다. GLSL 쉐이더 내에 Glide의 Combine 수학 방정식을 정밀하게 구현했습니다.

## 상세 변경 내역
1. **GLSL Fragment Shader의 결합 방정식 완전 모사 (`glide_opengl_shader.cpp`)**:
   - `Implementation` 구조체에 6개의 새로운 Uniform( `alpha_factor`, `alpha_local`, `alpha_other`, `color_factor`, `color_local`, `color_other` )을 추가했습니다.
   - Fragment Shader(`kFragmentSource`) 내에서 해당 파라미터 조합을 기반으로 한 Color/Alpha 블렌딩 값을 동적으로 산출하는 완전한 `C = other * factor + local` 방식의 공식 파이프라인을 구현했습니다.
   - 특히 Glide 특유의 팩터(0 = NONE, 8 = ONE) 및 로컬/아더(0 = ITERATED, 1 = CONSTANT / TEXTURE) 조합을 정확하게 분기하여 처리하도록 작성했습니다.

2. **Combine 설정 시 Uniform 바인딩 (`glide_opengl_shader.cpp`)**:
   - `SetColorCombine` 및 `SetAlphaCombine`에서 모든 조합의 수용을 허용하도록(`state.valid`만 체크) 조건을 완화했습니다.
   - 전달받은 상태 객체의 `factor`, `local`, `other` 열거형 값을 `glUniform1i`로 쉐이더에 주입했습니다.
   - 쉐이더 초기화 시점(Seeding)에 기본 블렌드(InitCombine) 환경에 맞추어 `color_other = 2`, `alpha_other = 2` 등 초기값을 설정했습니다.

## 검증 결과
- 모든 빌드 오류 없이 정상적으로 CMake/MSBuild가 통과되었습니다.
- 이전 작업 브랜치(`feature/glide-alpha-fix`)에 이어서 커밋을 남겨 작업 규칙(main이 아닐 때는 동일 브랜치 사용)을 준수했습니다.
