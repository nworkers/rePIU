# Task 738: 창 제목 빌드 identity 작업 지시

설계: [20260926-738](../design/20260926-738-window-title-build-identity.md)

## 한국어

1. `build_identity.h/.cpp`에 플랫폼·아키텍처·빌드 구성 이름과 라벨 함수를 만든다.
2. CMake가 `REPIU_BUILD_CONFIG="$<CONFIG>"`를 정의하고 새 소스를 `repiu_exe`에 넣는다.
3. `GlideOpenGlBackend::BuildWindowTitle`이 버전 뒤에 `(<label>)`을 넣는다.
4. core probe `build_identity`를 추가한다(`repiu_aot_probe --build-identity`도).
5. Win32와 Linux x64에서 빌드·core probe·실행 창 제목을 확인한다.
6. ARCHITECTURE의 창 제목 서술을 갱신하고 작업 로그를 남긴 뒤 커밋한다.

## English

1. Add the platform, architecture and configuration names and the label function in
   `build_identity.h/.cpp`.
2. Have CMake define `REPIU_BUILD_CONFIG="$<CONFIG>"` and add the source to `repiu_exe`.
3. Put `(<label>)` after the version in `GlideOpenGlBackend::BuildWindowTitle`.
4. Add the `build_identity` core probe (and `repiu_aot_probe --build-identity`).
5. Check the build, the core probe and a run's window title on Win32 and Linux x64.
6. Update ARCHITECTURE's title description, write the work log, commit.
