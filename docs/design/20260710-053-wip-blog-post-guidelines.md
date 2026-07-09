# WIP 블로그 게시글 작성 지침 설계

## 배경

프로젝트 진행 상황을 정기적으로 블로그에 게시하기 위해 반복 가능한 Markdown 작성 양식이 필요하다.

이번 첫 게시글은 저장소의 최초 커밋부터 현재 마지막 커밋까지의 work in progress를 대상으로 한다.

## 설계

블로그 게시글은 `docs/post/` 아래에 보관한다. 파일명은 `yyyy-mm-dd-hhmmss-title.md` 형식을 사용하고, title 부분은 영어 소문자와 하이픈을 사용한다.

게시글의 표시 제목은 영어만 사용한다. 본문은 한국어 전체 문서를 먼저 작성하고, 같은 내용을 영어 전체 문서로 다시 작성한다.

한국어 문서의 필수 소제목은 다음 이름을 유지한다.

* `주요 변경 사항`
* `사용된 기술 스택`

영문 문서의 대응 소제목은 다음 이름을 사용한다.

* `Major Changes`
* `Technology Stack Used`

도식화는 Mermaid를 사용한다. 주요 코드 흐름은 `flowchart` 또는 `sequenceDiagram`으로 설명하고, 누적 수치 변화는 Mermaid `xychart-beta`를 우선 사용한다.

## 검증

이번 작업의 검증은 문서 파일 존재 여부, Markdown 구조, Mermaid 코드 블록 포함 여부, 그리고 `scripts/test_all.ps1` 실행 결과 기록으로 한다.

# WIP Blog Post Guideline Design

## Background

The project needs a repeatable Markdown format for regularly publishing development progress posts.

This first post covers the work in progress from the repository's first commit through the current latest commit.

## Design

Blog posts are stored under `docs/post/`. Filenames use the `yyyy-mm-dd-hhmmss-title.md` format, with the title portion written in lowercase English words separated by hyphens.

The displayed post title uses English only. The body first contains the complete Korean document, followed by the complete English document for the same content.

The Korean document keeps these required subsection names:

* `주요 변경 사항`
* `사용된 기술 스택`

The English document uses these corresponding subsection names:

* `Major Changes`
* `Technology Stack Used`

Diagrams use Mermaid. Main code flow should use `flowchart` or `sequenceDiagram`, and cumulative metric changes should prefer Mermaid `xychart-beta`.

## Verification

Verification for this task checks that the document files exist, the Markdown structure is present, Mermaid code blocks are included, and the `scripts/test_all.ps1` result is recorded.
