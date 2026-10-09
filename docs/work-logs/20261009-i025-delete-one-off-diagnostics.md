# 작업 로그: 이미 답이 나온 질문을 위한 일회성 진단 삭제 (issue #25)

설계: `docs/design/20261009-i025-delete-one-off-diagnostics.md`
작업 지시: `docs/work-orders/20261009-i025-delete-one-off-diagnostics.md`

## 한 일

후보 13개를 코드에서 다시 읽어 **9개를 지우고 4개를 남겼다.** 약 710줄이 줄었다.

* **Glide gate 경계**(`linexe_glide_boundary.cpp`): `CALL_AUDIT`, `TEX_CENSUS`(세 곳 —
  `grTexMemRequired` 인자 로그, 다운로드 인자·형식 로그, 저장 실패 로그), `DRAW_CENSUS`,
  `TRI_CENSUS`, `FRAME_DUMP`(함수·전역 3개·swap 호출), `DUMP_LFB_BMP`(BMP writer와 LFB
  present마다 부르던 `getenv`)를 지우고, 그 writer만 쓰던 `<fstream>`·`<filesystem>`을 뺐다.
  저장 실패는 바로 위의 `record_unsupported`/`record_backend_failure`가 이미 기록한다.
* **vertex depth census**: 헤더·소스와 loader 출력, CMake 항목을 지웠다. 디코더가 쓰는
  크기 하한 `kGlideVertexDepthMeaningfulMagnitude`는 `glide_vertex.cpp`의 파일 내부 상수로
  옮겼다.
* **`REPIU_LOWMEM_TRACE`**: 로그 블록과, 이미 사라진 NOP 패치를 설명하던 주석.
* **`REPIU_AOT_PROBE_GUEST`**: fault 시 캐시 덤프 블록, `ThreadContext`·`ExecutionAttempt`의
  결과 필드 4개, loader 출력.
* **유지**: `REPIU_AOT_DBT_CALL_TRACE`, `REPIU_AOT_RETIRED_TRAP_PROFILE`,
  `REPIU_GLIDE_SETTER_CENSUS`/`_PHASE` — 근거는 설계와 환경 변수 목록에 적었다.
* **문서**: Glide analysis의 방법 메모 두 곳에 제거 사실을 적고, 환경 변수 목록과 TODO를
  갱신했다. ARCHITECTURE는 유지한 두 항목만 언급해 고칠 곳이 없었다.

### 과정에서 고친 실수

* 첫 Linux 빌드가 `execution_trampoline.cpp`의 괄호 불일치로 실패했다. `AOT_PROBE_GUEST`
  블록을 줄 번호로 지우면서 안쪽 `if`의 닫는 괄호까지만 지워 바깥 `if`의 괄호 하나가 남았다.
  그 줄을 지웠다.
* 첫 Win32 빌드가 `TEX_CENSUS`의 세 번째 사용처(저장 실패 로그)를 놓쳐 실패했다. 그 블록도
  지웠다.

## 검증

* **빌드**: Win32 Release(`build/win32_x86_debug`) 오류 0. Linux x64 Release 통과, 경고는
  손대지 않은 파일의 기존 것뿐.
* **aot_probe 전체 체인**(`MASTER/PIU_1ST/PIU/PIU.EXE`): 종료 코드 0, 539줄로 #24 때와 같다.
* **core probe**: Linux x64 모두 통과. Win32는 `stack_bridge` 하나 실패(#8 기존).
* **Glide probe**: `repiu_glide_issue_probe`, `repiu_glide_render_probe` 통과.
* **변수 읽기**: `src/`·`include/`에 9개 변수 문자열이 없다.
* **게임 실행 안 함**: 지운 코드는 모두 변수가 있을 때만 도는 블록이다(LFB present의
  `getenv` 한 번 제외 — 이는 호출만 사라진다).

## Linux 검증 (2026-10-10)

릴리스 노트(v0.0.210)에 남긴 Linux i386 빌드를 확인했다. 환경과 트리는 #24 작업 로그의 같은
절과 같다(Ubuntu 26.04.1, main `2809668`).

* **빌드**: Linux i386·x64 Release, 모든 기본 타깃 통과, 경고는 원래 있던 것뿐.
* **core probe**: 두 아키텍처 모두 `core_probe_failures=0`, 종료 코드 0.

---

# Work log: delete one-off diagnostics for answered questions (issue #25)

**Done.** Re-reading the thirteen candidates in code, **nine went and four stayed**, about 710
lines. In the Glide gate boundary: `CALL_AUDIT`, `TEX_CENSUS` (three sites — the
`grTexMemRequired` argument log, the download argument and format log, and a store-failure log
that the `record_unsupported`/`record_backend_failure` calls just above already cover),
`DRAW_CENSUS`, `TRI_CENSUS`, `FRAME_DUMP` (function, three globals and the swap-path call) and
`DUMP_LFB_BMP` (the BMP writer and the `getenv` it ran on every LFB present), with the
`<fstream>`/`<filesystem>` includes only the writer used. The vertex depth census lost its header,
source, loader output and CMake entry; the decoder's magnitude floor moved into `glide_vertex.cpp`
as a file-local constant. `REPIU_LOWMEM_TRACE` went with a comment about the long-gone NOP patch,
and `REPIU_AOT_PROBE_GUEST` with its fault-time cache dump, four result fields and loader output.
`REPIU_AOT_DBT_CALL_TRACE`, `REPIU_AOT_RETIRED_TRAP_PROFILE` and `REPIU_GLIDE_SETTER_CENSUS`/
`_PHASE` stay, for the reasons in the design and the inventory. The Glide analysis method notes,
the inventory and the TODO record the change; ARCHITECTURE mentions only the kept items.

**Mistakes fixed on the way.** The first Linux build failed on unbalanced braces: deleting the
`AOT_PROBE_GUEST` block by line numbers stopped at the inner `if`'s brace and left the outer one.
The first Win32 build failed on a third `TEX_CENSUS` use (the store-failure log), now deleted too.

**Verification.** Win32 Release build with no errors; Linux x64 Release build with only
pre-existing warnings. The full aot_probe chain exits 0 with 539 lines, as in #24. Core probe: all
pass on Linux x64; on Win32 only `stack_bridge` fails (#8). Both Glide probes pass. No source reads
the nine variables. No game run: every deleted block runs only with its variable set (the one
`getenv` per LFB present simply stops being called).

**Linux verification (2026-10-10).** The Linux i386 build left open in the v0.0.210 release notes
was checked, in the environment and tree of the same section in #24's work log (Ubuntu 26.04.1,
main `2809668`): Linux i386 and x64 Release builds of every default target pass with only
pre-existing warnings, and the core probe reports `core_probe_failures=0` and exits 0 on both.
