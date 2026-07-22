# 창 제목 버전·빌드 날짜 작업 지시

## 한국어

### 작업 범위

1. CMake에서 루트 `VERSION`을 읽고 형식을 검증합니다.
2. `VERSION` 변경을 자동 재구성 대상으로 등록합니다.
3. `repiu_exe`에 `REPIU_VERSION` compile definition을 전달합니다.
4. SDL3 창 제목을 버전·`__DATE__`·renderer 이름으로 조합합니다.
5. 아키텍처와 작업 로그를 갱신하고 실제 창 제목을 검증합니다.

### 완료 조건

- 창 제목에 `v0.0.85`와 컴파일 날짜 표시
- Win32 x86 Debug 빌드 성공
- 실제 SDL 창 title 확인

## English

### Scope

1. Read and validate the root `VERSION` from CMake.
2. Register `VERSION` for automatic build-system regeneration.
3. Pass `REPIU_VERSION` to `repiu_exe` as a compile definition.
4. Compose the SDL3 title from version, `__DATE__`, and renderer name.
5. Update architecture/work-log documentation and verify the real title.

### Completion criteria

- The window title contains `v0.0.85` and the compilation date
- Win32 x86 Debug build succeeds
- The real SDL window title is confirmed
