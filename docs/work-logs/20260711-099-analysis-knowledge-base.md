# 분석 문서와 기술 지식 기반 작업 로그

## 결과

`docs/analysis/`와 `docs/kb/`를 새로 만들고 각각 링크 색인 `README.md`를 추가했다.

분석 문서는 현재까지의 프로젝트 고유 결과를 다음 6개 주제로 통합했다.

* 실행 파일 로딩과 relocation
* Win32 실행 trampoline과 예외 기반 HLE
* runtime arena, shadow memory, sentinel
* DOS 파일 I/O와 INT3 해결
* interrupt와 port I/O
* 현재 실행 frontier

기술 지식 기반은 다음 6개 주제로 구성했다.

* DOS/4GW와 DPMI
* x86 segmentation과 16/32비트 처리
* arena, sentinel, shadow memory 용어
* 주요 DOS/DPMI interrupt
* LE 형식과 fixup/relocation
* HLE와 예외 기반 직접 실행

외부 지식에는 Intel SDM, DPMI 사양, Open Watcom, RBIL 배포 페이지, Clang AddressSanitizer, Microsoft SEH/실행 형식 문서 링크를 포함했다. 프로젝트에서 직접 확인한 사실과 일반 기술 정의가 섞이지 않도록 디렉터리 책임을 분리했다.

`AGENTS.md`에는 새 디렉터리를 참조 문서에 추가하고, 새로운 분석 사실·용어가 등장할 때 관련 topic과 README 색인을 같은 작업에서 갱신하도록 한국어/영어 규칙을 추가했다. 작업 로그는 시간순 증거, analysis/kb는 주제별 누적 문서로 유지하도록 명시했다.

## 검증

* `docs/analysis/`와 `docs/kb/`의 모든 상대 Markdown 링크가 실제 파일로 해석됨
* `git diff --check`: 통과
* 문서 전용 변경이므로 build는 실행하지 않음

# Analysis Documentation and Knowledge Base Work Log

## Result

Created `docs/analysis/` and `docs/kb/`, each with a linked `README.md` index.

The analysis area consolidates project-specific findings into six topics: executable loading/relocation, the Win32 execution trampoline, runtime arena/shadow/sentinel behavior, DOS file I/O and INT3 resolution, interrupts/port I/O, and the current execution frontier.

The knowledge base contains six topics: DOS/4GW and DPMI, x86 segmentation and 16/32-bit behavior, arena/sentinel/shadow terminology, important DOS/DPMI interrupts, LE/fixup/relocation, and exception-driven HLE.

External knowledge includes links to Intel manuals, the DPMI specification, Open Watcom, the RBIL distribution page, Clang AddressSanitizer, and Microsoft SEH/executable-format documentation. Directory responsibilities keep project-confirmed findings separate from general technical definitions.

Updated `AGENTS.md` in Korean and English so new findings and terminology require related topic and README-index updates in the same task. Work logs remain chronological evidence; analysis and knowledge files remain cumulative topic references.

## Verification

* Every relative Markdown link under `docs/analysis/` and `docs/kb/` resolves to an existing file.
* `git diff --check`: passed.
* No build was run because this is a documentation-only change.
