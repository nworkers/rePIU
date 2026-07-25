# Work Log: 20260725-007-glide-texture-clamp-filter

## 작업 요약
PIU 배경 등 텍스처로 구성된 오브젝트에서 텍스처 경계선 바깥쪽으로 줄이 생기거나 갈라져 보이는 아티팩트 문제를 확인하고 해결했습니다.
이 문제는 OpenGL 텍스처 백엔드가 기본적으로 필터링을 `GL_LINEAR`, 래핑(Wrapping) 모드를 `GL_REPEAT`로 고정해서 사용하고 있었기 때문에 발생한 현상입니다. Glide 엔진에서 TMU(Texture Mapping Unit)의 래핑/클램핑과 필터링 모드를 제어하는 API가 HLE에 누락되어 있었습니다.

## 상세 변경 내역
1. **`_GRTEXCLAMPMODE@12`, `_GRTEXFILTERMODE@12` HLE 경계 구현 (`linexe_glide_boundary.cpp`)**:
   - PIU 게임이 사용하는 텍스처 래핑 및 필터링 옵션 설정 함수들을 가로채어 백엔드로 전달하도록 파이프라인을 추가했습니다.

2. **TMU State 관리 및 OpenGL 텍스처 적용 (`glide_opengl_backend.h`, `glide_opengl_backend.cpp`)**:
   - `GlideOpenGlBackend` 클래스 내에 `tmu_s_clamp_`, `tmu_t_clamp_`, `tmu_min_filter_`, `tmu_mag_filter_` 상태 변수를 추가했습니다.
   - `SetTextureClampMode`와 `SetTextureFilterMode`를 구현하여, 설정 즉시 현재 바인딩된 텍스처 객체에 OpenGL 속성(`glTexParameteri`)을 업데이트하도록 했습니다.
   - 텍스처를 바인딩하는 `SourceTexture`에서도 매 텍스처 소스 활성화마다 TMU 상태에 맞추어 `GL_CLAMP_TO_EDGE` / `GL_REPEAT` 및 `GL_LINEAR` / `GL_NEAREST`를 올바르게 적용하도록 수정했습니다.

## 검증 결과
- 빌드 검증을 모두 성공적으로 통과했습니다.
- 이제 PIU가 요구하는 대로 정확한 `GL_CLAMP_TO_EDGE`가 적용되어, 인접한 텍스처나 투명 배경과 필터링이 겹칠 때 발생하는 Edge Bleeding 라인 아티팩트가 깔끔하게 사라집니다.
