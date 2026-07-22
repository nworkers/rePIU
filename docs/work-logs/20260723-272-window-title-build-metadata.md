# 창 제목 버전·빌드 날짜 작업 로그

## 한국어

### 결과

- CMake가 루트 `VERSION`의 첫 줄을 읽고 `major.minor.patch` 형식을 검증하게 했습니다.
- `VERSION`을 configure dependency로 등록해 버전 변경 시 자동 재구성되게 했습니다.
- 검증된 값을 `REPIU_VERSION` compile definition으로 `repiu_exe`에 전달했습니다.
- SDL3 Glide 창 제목을 `rePIU v<version> - Build <__DATE__> - Glide 2 OpenGL` 형식으로 변경했습니다.
- CMake를 사용하지 않는 제한적 컴파일 경로에는 `unknown` fallback을 두되, 정규 프로젝트 빌드는 항상 `VERSION` 값을 사용합니다.

### 검증

- `cmake --build build\win32_x86_debug --config Debug -- /m:1`: CMake 재구성 및 전체 빌드 성공
- 실제 `pumpit1` SDL 창의 `MainWindowTitle`:
  - `rePIU v0.0.85 - Build Jul 23 2026 - Glide 2 OpenGL`
- 실제 실행에서 기본 2배 창 생성 유지 확인
- `SDL_IsMainThread` assertion 미발생

## English

### Result

- Made CMake read the first root `VERSION` line and validate `major.minor.patch` format.
- Registered `VERSION` as a configure dependency for automatic regeneration after version changes.
- Passed the validated value to `repiu_exe` as the `REPIU_VERSION` compile definition.
- Changed the SDL3 Glide title to `rePIU v<version> - Build <__DATE__> - Glide 2 OpenGL`.
- Added an `unknown` fallback for limited non-CMake compilation paths; normal project builds always use `VERSION`.

### Verification

- `cmake --build build\win32_x86_debug --config Debug -- /m:1`: CMake regeneration and full build passed
- Real `pumpit1` SDL window `MainWindowTitle`:
  - `rePIU v0.0.85 - Build Jul 23 2026 - Glide 2 OpenGL`
- Confirmed the default 2× window still opens
- No `SDL_IsMainThread` assertion
