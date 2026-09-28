# Task 759: 실행 모델과 platform 이동 작업 지시

설계: [20260929-759](../design/20260929-759-execution-model-and-platform-moves.md)

## 한국어

0. 기준: 커밋 `a7ad129`에서 Linux x64 Release·Debug, Linux i386, Win32 x86 Debug를 빌드하고 probe 출력을
   모은다.
1. 단계 1: 규칙 문서를 고치고, Task 758 단계 2가 엔진 안에 만든 `win32/`, `linux/`, `x86/`, `linux/x64/`
   디렉터리를 설계의 표대로 없앤다 → 검증 → 커밋.
2. 단계 2: MSVC 인라인 어셈블리 thunk를 `src/platform/win32/`로 옮긴다 → 검증 → 커밋.
3. 단계 3: `execution_model.h`를 채우고 엔진 핵심의 아키텍처 분기를 바꾼다. 파일마다 검증 → 커밋.
4. 단계 4: 엔진 핵심의 Win32 API 호출을 플랫폼 함수로 떼어 낸다 → 검증 → 커밋.
5. 마지막에 플랫폼 계층 밖에 남은 분기를 다시 세어 설계의 분류 4·5와 probe 가드만 남았는지 확인하고, 작업
   로그를 쓴다.

## English

0. Baseline: at commit `a7ad129`, build Linux x64 Release and Debug, Linux i386 and Win32 x86 Debug and
   collect the probes' output.
1. Phase 1: correct the rule documents and remove the `win32/`, `linux/`, `x86/` and `linux/x64/`
   directories Task 758's phase 2 made inside the engine, by the design's table → verify → commit.
2. Phase 2: move the MSVC inline-assembly thunks to `src/platform/win32/` → verify → commit.
3. Phase 3: fill `execution_model.h` and replace the engine core's architecture branches, verifying and
   committing file by file.
4. Phase 4: take the engine core's Win32 API calls out into platform functions → verify → commit.
5. Finally, count the branches left outside the platform layer again to confirm only the design's classes
   4 and 5 and the probe guards remain, and write the work log.
