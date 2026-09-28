# 블로그 게시글 작성 지침

## 파일 위치와 이름

게시글은 `docs/post/` 아래에 Markdown 파일로 작성한다.

파일명은 `yyyy-mm-dd-hhmmss-title.md` 형식을 사용한다. `title`은 영어만 사용하고, 단어는 하이픈으로 연결한다.

예시:

```text
docs/post/2026-07-10-010119-preserving-dos4gw-execution-wip.md
```

## 문서 언어 순서

게시글 제목은 영어만 사용한다.

본문은 한국어 전체 문서를 먼저 작성하고, 그 다음에 같은 내용을 영어 전체 문서로 작성한다.

## 사이트 게시

main에 머지된 글은 [프로젝트 사이트](https://nworkers.github.io/rePIU/)의 개발 기록에 자동으로 게시된다
([사이트 README](../sites/README.md)). 사이트 빌드가 두 언어를 나누는 규칙을 지킨다.

* 한국어 문서와 영어 문서 사이에는 `---` 한 줄만 둔다. 그 다음 비어 있지 않은 줄은 영어 문서의 `#` 제목이어야
  한다. 코드 블록 안의 `---`와 `#` 줄은 분리에 쓰이지 않는다.
* 각 언어 문서의 첫 줄은 `#` 제목이다. 사이트는 이 제목을 페이지 제목으로 쓰고 본문에서 뺀다.
* 글 목록의 요약은 제목 뒤 첫 문단 중 60자 이상인 첫 문단이다. 범위 링크 줄 다음에 글을 소개하는 문단을 둔다.
* 저장소 안 파일을 가리키는 상대 링크는 GitHub의 해당 파일로, 다른 글을 가리키는 링크는 사이트의 글 페이지로
  바뀐다.

## 필수 구성

한국어 문서에는 아래 소제목 이름을 유지한다.

```markdown
## 주요 변경 사항
## 사용된 기술 스택
```

영문 문서에는 아래 대응 소제목을 사용한다.

```markdown
## Major Changes
## Technology Stack Used
```

## 주요 변경 사항 작성 규칙

* 지정된 커밋 범위의 주요 변경 사항을 요약한다.
* 변경 사항의 핵심 코드 동작 구조를 Mermaid로 도식화한다.
* `piu_1st` 실행 로그 중 현재 진행 지점과 blocker를 보여 주는 핵심 부분을 포함한다.
* sample test 결과 요약을 포함한다.
* sample test의 누적 결과는 Mermaid `xychart-beta`로 표시한다.
* 변경 내용을 설명하는 커밋 링크를 포함한다.

## 사용된 기술 스택 작성 규칙

* 이번 범위에서 사용했거나 새로 발견한 기술 스택을 설명한다.
* DOS interrupt, DPMI, LE fixup, Win32 memory API처럼 동작 이해가 필요한 주제는 배경과 구현 방식을 함께 설명한다.
* 필요한 경우 Mermaid로 처리 흐름을 추가한다.
* 참고 링크는 공식 문서, 원문 저장소, 품질이 높은 기술 문서를 우선한다.

# Blog Post Writing Guidelines

## File Location and Name

Write posts as Markdown files under `docs/post/`.

Use the `yyyy-mm-dd-hhmmss-title.md` filename format. The `title` portion must be English-only, with words separated by hyphens.

Example:

```text
docs/post/2026-07-10-010119-preserving-dos4gw-execution-wip.md
```

## Language Order

The post title must be English-only.

Write the complete Korean document first, followed by the complete English document for the same content.

## Site Publishing

Posts merged to main are published automatically in the dev log of the
[project site](https://nworkers.github.io/rePIU/) ([site README](../sites/README.md)). Keep to the rules
the site build uses to separate the two languages.

* Put a single `---` line between the Korean and English documents. The next non-blank line must be the
  English document's `#` title. `---` and `#` lines inside code blocks are never used for the split.
* Each language document starts with its `#` title. The site uses it as the page title and removes it from
  the body.
* The excerpt in the post list is the first paragraph after the title with at least 60 characters. Put a
  paragraph introducing the post after the range-link line.
* Relative links to files in the repository become links to those files on GitHub, and links to another
  post become links to that post's site page.

## Required Structure

Keep these subsection names in the Korean document:

```markdown
## 주요 변경 사항
## 사용된 기술 스택
```

Use these corresponding subsection names in the English document:

```markdown
## Major Changes
## Technology Stack Used
```

## Major Changes Rules

* Summarize the major changes in the specified commit range.
* Visualize the core code behavior with Mermaid.
* Include the key `piu_1st` execution log lines showing the current progress point and blocker.
* Include sample test result summaries.
* Show cumulative sample test results with Mermaid `xychart-beta`.
* Link to commits that explain the changes.

## Technology Stack Rules

* Explain the technology stack used or discovered in the covered range.
* For topics that need behavioral context, such as DOS interrupts, DPMI, LE fixups, and Win32 memory APIs, explain both the background and the project implementation.
* Add Mermaid flow diagrams when useful.
* Prefer official documentation, upstream repositories, and high-quality technical references.
