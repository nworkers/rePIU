# WIP 블로그 게시글 작성 작업 지시

## 목표

최초 커밋부터 현재 마지막 커밋까지의 진행 상황을 블로그 게시글 Markdown으로 작성하고, 이후 같은 형식으로 반복 작성할 수 있도록 지침을 저장한다.

## 범위

* `docs/post/README.md`에 재사용 작성 지침을 추가한다.
* `docs/post/yyyy-mm-dd-hhmmss-title.md` 형식의 첫 게시글을 추가한다.
* 게시글은 한국어 전체 문서 뒤에 영어 전체 문서를 배치한다.
* 주요 변경 사항과 사용 기술 스택을 설명한다.
* Mermaid 도식과 샘플 테스트 누적 차트를 포함한다.
* `piu_1st` 실행 로그와 OpenWatcom sample test 결과를 포함한다.
* 관련 커밋 링크를 포함한다.

## 검증

* `scripts/test_all.ps1`로 현재 빌드와 핵심 실행 관측점을 확인한다.
* 생성된 Markdown이 필수 소제목과 Mermaid 블록을 포함하는지 확인한다.

# WIP Blog Post Work Order

## Goal

Write a Markdown blog post covering progress from the first commit through the current latest commit, and save reusable instructions so future posts can use the same format.

## Scope

* Add reusable writing guidelines to `docs/post/README.md`.
* Add the first post using the `docs/post/yyyy-mm-dd-hhmmss-title.md` filename format.
* Place the complete Korean document before the complete English document.
* Explain major changes and the technology stack used.
* Include Mermaid diagrams and a cumulative sample-test chart.
* Include the `piu_1st` execution log and OpenWatcom sample test results.
* Include related commit links.

## Verification

* Use `scripts/test_all.ps1` to verify the current build and key execution observation points.
* Confirm that the generated Markdown includes the required subsection names and Mermaid blocks.
