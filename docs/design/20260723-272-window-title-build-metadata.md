# 창 제목 버전·빌드 날짜 설계

## 한국어

### 목표

SDL3 Glide 창 제목에 프로젝트 버전과 빌드 날짜를 함께 표시합니다.

제목 형식:

```text
rePIU v<major.minor.patch> - Build <컴파일 날짜> - Glide 2 OpenGL
```

현재 예시는 `rePIU v0.0.85 - Build Jul 23 2026 - Glide 2 OpenGL`입니다.

### 메타데이터 출처

- 버전은 저장소 루트 `VERSION`의 첫 줄을 CMake configure 단계에서 읽습니다.
- 값은 `major.minor.patch` 형식을 검증한 뒤 `REPIU_VERSION` compile definition으로 `repiu_exe`에 전달합니다.
- `VERSION`을 CMake configure dependency로 등록하여 파일 변경 시 build system이 자동 재구성되게 합니다.
- 빌드 날짜는 `glide_opengl_backend.cpp`가 컴파일된 날짜를 나타내는 표준 컴파일러 매크로 `__DATE__`를 사용합니다.

버전의 단일 원천은 계속 `VERSION`이며, 창 backend가 별도 버전 상수를 소유하지 않습니다.

### 검증

- CMake 재구성과 Win32 x86 Debug 빌드 성공
- 실제 SDL 창의 title에서 `VERSION` 값과 컴파일 날짜 확인

## English

### Objective

Display the project version and build date in the SDL3 Glide window title.

Title format:

```text
rePIU v<major.minor.patch> - Build <compile date> - Glide 2 OpenGL
```

The current example is `rePIU v0.0.85 - Build Jul 23 2026 - Glide 2 OpenGL`.

### Metadata sources

- CMake reads the first line of the repository-root `VERSION` during configuration.
- After validating `major.minor.patch`, it passes the value to `repiu_exe` as the `REPIU_VERSION` compile definition.
- `VERSION` is registered as a CMake configure dependency so changing it automatically regenerates the build system.
- The build date uses the standard compiler `__DATE__` macro and therefore represents the date `glide_opengl_backend.cpp` was compiled.

`VERSION` remains the single version source; the window backend does not own a separate version constant.

### Verification

- Successful CMake regeneration and Win32 x86 Debug build
- Confirm the `VERSION` value and compilation date in the real SDL window title
