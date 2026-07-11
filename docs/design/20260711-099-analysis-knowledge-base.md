# 분석 문서와 기술 지식 기반 설계

## 목적

작업 단위 설계·로그에 흩어진 바이너리 분석 결과를 주제별 장기 문서로 재구성하고, 프로젝트 이해에 필요한 표준 기술 지식을 별도 지식 기반으로 분리한다.

## 디렉터리 책임

* `docs/analysis/`: PIU 실행 파일과 현재 구현을 직접 관찰하여 확인한 프로젝트 고유 사실, 가설, 해결 이력, 현재 미확정 사항
* `docs/kb/`: DOS/4GW, DPMI, x86, LE, interrupt, HLE 등 외부에서도 성립하는 기술 정의와 배경 지식
* 각 디렉터리의 `README.md`: 문서 목적, 상태 표기 규칙, 주제별 링크 색인

분석 문서는 확인됨/추정/미확정을 구분한다. 지식 문서는 외부 자료에서 얻은 내용에 원문 링크를 가까이 배치하며, 프로젝트 적용 해석은 일반 정의와 구분한다.

## 초기 주제

분석은 실행 파일 로딩·relocation, 실행 trampoline, 메모리 arena/shadow/sentinel, DOS 파일 I/O와 INT3, interrupt·port I/O, 현재 실행 frontier로 나눈다. 지식 기반은 DOS/4GW·DPMI, x86 segmentation과 16/32비트, 메모리 용어, 주요 interrupt, LE/fixup, HLE·예외 실행으로 나눈다.

## 지속 갱신

`AGENTS.md`에 다음 규칙을 추가한다.

* 바이너리 분석으로 새로운 사실이나 기존 결론의 변경이 생기면 관련 `docs/analysis/` 문서를 갱신한다.
* 작업 중 새로운 전문 용어·표준·외부 기술이 핵심 이해에 필요해지면 `docs/kb/`를 갱신하고 출처 링크를 남긴다.
* 새 파일을 만들면 해당 README 색인도 같은 작업에서 갱신한다.
* 작업 로그는 시간순 증거이고 analysis/kb는 주제별 누적 문서라는 역할을 유지한다.

# Analysis Documentation and Knowledge Base Design

## Purpose

Reorganize binary-analysis findings scattered across task-specific designs and logs into durable topic documents, while separating generally applicable technical knowledge needed to understand the project.

## Directory Responsibilities

* `docs/analysis/`: project-specific facts, hypotheses, resolved issues, and open questions confirmed by observing the PIU executable and current implementation
* `docs/kb/`: generally applicable definitions and background for DOS/4GW, DPMI, x86, LE, interrupts, HLE, and related technologies
* `README.md` in each directory: purpose, status conventions, and linked topic index

Analysis documents distinguish confirmed, inferred, and unresolved statements. Knowledge documents place links near information derived from external sources and separate general definitions from project-specific interpretation.

## Initial Topics

Analysis is divided into executable loading and relocation, execution trampoline, memory arena/shadow/sentinel, DOS file I/O and INT3, interrupts and port I/O, and the current execution frontier. The knowledge base covers DOS/4GW and DPMI, x86 segmentation and 16/32-bit behavior, memory terminology, important interrupts, LE/fixups, and exception-driven HLE.

## Ongoing Maintenance

Add rules to `AGENTS.md` requiring updates to analysis documents for new or changed binary findings, updates to knowledge documents for important new terminology or standards with source links, and README index updates whenever topic files are added. Work logs remain chronological evidence; analysis and knowledge documents remain cumulative topic references.
