# Mermaid 문서화 규칙 작업 로그

## 결과

`AGENTS.md`의 한국어와 영어 문서 작성 규칙에 Mermaid 우선 도식화 지침을 추가했다.

새 규칙은 다음을 요구한다.

* 모든 Markdown 문서에서 구조, 관계, 흐름, 호출 순서, 상태 전이, 주소 변환의 시각화 가능성을 검토한다.
* 세 개 이상의 구성요소, 단계, 분기, 계층이 있으면 Mermaid 적용 가능성을 확인한다.
* 내용에 따라 flowchart, sequenceDiagram, stateDiagram, classDiagram 등을 선택한다.
* 이해를 실질적으로 개선하는 도식은 최대한 포함한다.
* 단순 사실 하나나 한 단계 설명에는 도식을 불필요하게 강제하지 않는다.
* 도식과 본문의 사실 및 확인 상태를 일치시키고 본문도 함께 유지한다.

## 검증

* 한국어·영어 규칙에 각각 4개 지침이 대응함
* `git diff --check`: 통과
* 문서 규칙 변경이므로 build는 실행하지 않음

# Mermaid Documentation Rule Work Log

## Result

Added Mermaid-first visualization guidance to both the Korean and English documentation rules in `AGENTS.md`.

The rules require authors of all Markdown documents to evaluate visualization for structure, relationships, flows, call sequences, state transitions, and address translation; consider Mermaid for content with three or more components, steps, branches, or layers; choose a suitable diagram type; include diagrams broadly when useful; avoid forcing diagrams for trivial explanations; and keep diagrams consistent with the prose and its confirmation status.

## Verification

* Four corresponding guidance bullets exist in both Korean and English.
* `git diff --check`: passed.
* No build was run because this changes documentation rules only.
