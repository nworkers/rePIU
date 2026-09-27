# Task 756: Android arm64 이식 준비 작업 지시

설계: [20260928-756](../design/20260928-756-android-arm64-execution.md)

## 한국어

1. 실행 엔진, 플랫폼 계층, 빌드 체계, 렌더러, 오디오, 자산 경로에서 호스트 CPU와 호스트 OS에
   묶인 곳을 조사하고 셈한다.
2. aarch64-linux-gnu 크로스 컴파일러로 저장소를 수정하지 않은 채 configure와 `repiu_exe` 빌드를
   시도해, 컴파일되는 소스 수와 실패 원인을 잰다.
3. 웹 설계 513과 Linux x64 설계 546에서 arm64로 이어지는 결정을 가려내고, Android arm64 실행
   설계(Stage 0)를 쓴다. 단계 계획, 플랫폼 헤더 표, 사용자가 결정할 항목을 담는다.
4. `docs/analysis/android-arm64-port-frontier.md`와 `docs/kb/aarch64-and-android-host-constraints.md`를
   만들고 두 디렉터리의 색인, `docs/TODO.md`를 갱신한다.
5. 작업 로그를 남기고 커밋한다. **코드는 바꾸지 않는다.** Stage 1 이후는 사용자 결정 뒤 별도
   작업 지시로 진행한다.

## English

1. Survey and count the places in the execution engine, the platform layer, the build system, the
   renderer, audio and asset paths that are tied to the host CPU or the host OS.
2. With the aarch64-linux-gnu cross compiler, attempt a configure and a `repiu_exe` build without
   modifying the repository, and measure how many sources compile and why the rest fail.
3. Identify the decisions that carry from web design 513 and Linux x64 design 546 to arm64, and write
   the Android arm64 execution design (Stage 0): the stage plan, the platform-header table and the
   items the user must decide.
4. Create `docs/analysis/android-arm64-port-frontier.md` and
   `docs/kb/aarch64-and-android-host-constraints.md`, and update both directory indexes and
   `docs/TODO.md`.
5. Leave the work log and commit. **No code changes.** Stage 1 onward proceeds under separate work
   orders after the user's decisions.
