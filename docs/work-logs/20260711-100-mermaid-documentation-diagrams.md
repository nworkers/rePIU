# 분석·지식 문서 Mermaid 도식 작업 로그

## 결과

`docs/analysis/`와 `docs/kb/`의 README 및 모든 주제 문서에 총 14개의 Mermaid diagram을 추가했다.

분석 문서에는 다음 관계를 시각화했다.

* 전체 분석 문서 탐색 map
* MZ/LE parse부터 relocated entry까지의 loading pipeline
* original-code 직접 실행과 exception HLE dispatch
* runtime arena, shadow memory, DS zero page의 address-domain 선택
* DOS file 실패와 INT3, 정상 I/O 흐름의 sequence
* software interrupt와 port-I/O router
* 현재 실행 milestone과 `03 07` blocker

기술 지식 문서에는 다음 구조를 시각화했다.

* 기술 주제 간 관계
* DOS application, DOS extender, DPMI host, DOS, CPU 계층
* selector/descriptor/offset의 선형 주소 계산과 size override
* arena/frontier/boundary/sentinel/shadow 관계
* 주요 interrupt별 책임
* LE object/page/fixup pipeline과 relocation 대상 분류
* direct execution과 HLE contract 흐름

도식은 한국어 본문 앞부분에 한 번만 두고 영어 절에서 중복하지 않았다. node label은 opcode와 표준 기술 용어 중심의 짧은 영문으로 작성했다.

## 검증

* Mermaid block: 14개
* 모든 Markdown code fence가 짝을 이룸
* 모든 Mermaid block이 지원 대상으로 정한 diagram header로 시작함
* analysis/kb 내부 상대 링크가 모두 실제 파일로 해석됨
* `git diff --check`: 통과
* 문서 전용 변경이므로 build는 실행하지 않음

# Mermaid Diagram Work Log for Analysis and Knowledge Documents

## Result

Added 14 Mermaid diagrams across the README and every topic document under `docs/analysis/` and `docs/kb/`.

The analysis diagrams cover navigation, the MZ/LE loading pipeline, direct execution and exception dispatch, memory address-domain selection, DOS file/INT3 sequencing, interrupt and port routing, and the current execution frontier. The knowledge diagrams cover topic relationships, DOS extender/DPMI layering, x86 address translation and size overrides, memory terminology, interrupt responsibilities, LE fixup classification, and the direct-execution HLE contract.

Each diagram appears once in the Korean-first portion and uses concise opcode or standard technical labels that also serve English readers.

## Verification

* Mermaid blocks: 14
* All Markdown code fences are paired.
* Every Mermaid block begins with an allowed diagram header.
* Every relative link under analysis/kb resolves to an existing file.
* `git diff --check`: passed.
* No build was run because this is a documentation-only change.
