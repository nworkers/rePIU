# 분석·지식 문서 Mermaid 도식 설계

## 목적

`docs/analysis/`와 `docs/kb/`의 관계, 실행 흐름, 주소 변환, 상태 전이를 Mermaid로 시각화하여 긴 설명을 읽기 전에 핵심 구조를 파악할 수 있게 한다.

## 적용 원칙

* 세 개 이상의 단계·분기·구성요소 관계가 있는 부분에만 도식을 넣는다.
* 본문을 대체하지 않고, 본문에서 설명하는 확인 사실과 동일한 범위만 표현한다.
* 분석 문서의 추정·미확정 내용을 확정된 흐름처럼 그리지 않는다.
* node label은 짧게 유지하고 상세 조건은 본문에 둔다.
* 한국어/영어 독자가 함께 사용할 수 있도록 opcode, register, 표준 용어 중심의 간결한 영문 label을 사용한다.
* Mermaid diagram은 각 문서의 한국어 본문 앞부분에 한 번만 두고 영어 절에서 중복하지 않는다.

## 도식 종류

* README: 문서 탐색 map
* executable/LE: loading과 relocation pipeline
* trampoline/HLE: direct execution과 exception dispatch flow
* memory: address-domain 선택과 arena/shadow/zero-page 관계
* file/INT3: 실패 흐름에서 정상 file I/O 흐름으로의 sequence
* interrupt/port: service router
* frontier: 완료된 milestone과 다음 blocker
* DPMI/x86/용어: layer, address translation, 개념 관계

## 검증

Markdown code fence 짝, Mermaid block header, node ID 중복과 local link를 정적 검사한다. 문서 전용 변경이므로 build는 요구하지 않는다.

# Mermaid Diagram Design for Analysis and Knowledge Documents

## Purpose

Use Mermaid to visualize relationships, execution flows, address translation, and state transitions in `docs/analysis/` and `docs/kb/`, allowing readers to understand the core structure before reading detailed prose.

## Principles

Add diagrams only where at least three steps, branches, or components benefit from visualization. Diagrams supplement rather than replace prose, do not present inferred or unresolved findings as confirmed, use short terminology-oriented labels, and appear once in the Korean-first portion rather than being duplicated in the English section.

## Verification

Statically check Markdown fence pairing, Mermaid headers, duplicate node IDs, and local links. No build is required for this documentation-only task.
