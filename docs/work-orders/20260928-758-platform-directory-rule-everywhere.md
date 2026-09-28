# Task 758: 플랫폼·아키텍처 디렉터리 규칙 전체 적용 작업 지시

설계: [20260928-758](../design/20260928-758-platform-directory-rule-everywhere.md)

## 한국어

0. 기준: Win32 core probe의 `CON:` 예외를 고치고(커밋 `48ca9e3`) Linux i386·x64, Web, Win32 기준 출력을 모은다.
1. CMake에 `REPIU_PLATFORM`, `REPIU_ARCH`를 둔다. `AGENTS.md`, `docs/CODING_STYLE.md`, `ARCHITECTURE.md`의
   규칙을 "모든 디렉터리"와 "헤더 선택 지점 예외"로 넓힌다.
2. 단계 1(플랫폼 계층, 컴파일러 shim) → 검증 → 커밋.
3. 단계 2(엔진 지원 파일) → 검증 → 커밋.
4. 단계 3(엔진 핵심) — 파일별로 나눠 진행하고 파일마다 검증 → 커밋.
5. 단계 4(호스트와 도구) → 검증 → 커밋.
6. 마지막에 플랫폼 디렉터리 밖에서 플랫폼 분기가 남은 파일이 선택 헤더뿐인지 전수 검사하고, 작업 로그를
   쓴다.

## English

0. Baseline: fix the Win32 core probe's `CON:` exception (commit `48ca9e3`) and collect Linux i386 and x64,
   Web and Win32 baseline output.
1. Add `REPIU_PLATFORM` and `REPIU_ARCH` to CMake. Widen the rule in `AGENTS.md`, `docs/CODING_STYLE.md` and
   `ARCHITECTURE.md` to "every directory" with the "header selection point" exception.
2. Phase 1 (platform layer, compiler shims) → verify → commit.
3. Phase 2 (engine support files) → verify → commit.
4. Phase 3 (engine core), file by file, verifying and committing each.
5. Phase 4 (hosts and tools) → verify → commit.
6. Finally, check every file outside the platform directories to confirm only selection headers keep a
   platform branch, and write the work log.
