# Mermaid 문서화 규칙 설계

## 목적

Markdown 문서를 작성하거나 갱신할 때 구조, 관계, 흐름, 상태 전이, 주소 변환, 호출 순서를 도식화할 수 있으면 Mermaid를 적극적으로 사용하도록 프로젝트 규칙을 명문화한다.

## 규칙 범위

* analysis/kb뿐 아니라 설계, 작업 지시, 작업 로그, architecture 등 모든 Markdown 문서에 적용한다.
* 세 개 이상의 구성요소나 단계, 분기, 계층, 상태 변화가 있으면 Mermaid 적용 가능성을 검토한다.
* 도식이 본문 이해를 실질적으로 개선할 때 최대한 포함한다.
* flowchart, sequenceDiagram, stateDiagram, classDiagram 등 내용에 맞는 Mermaid diagram을 선택한다.
* 단순 사실 하나나 한 단계 설명에는 불필요한 도식을 강제하지 않는다.
* 도식은 본문과 동일한 사실만 표현하며 추정과 확인 사항을 혼동하지 않는다.

# Mermaid Documentation Rule Design

## Purpose

Formalize a project rule requiring active use of Mermaid when Markdown documentation can benefit from visualizing structure, relationships, flows, state transitions, address translation, or call sequences.

## Scope

The rule applies to all Markdown documentation, not only analysis and knowledge files. Authors should evaluate Mermaid whenever content contains three or more components, steps, branches, layers, or state changes; choose the diagram type appropriate to the content; maximize useful visualization without forcing diagrams for simple one-fact or one-step explanations; and keep diagrams consistent with the confirmation status of the prose.
