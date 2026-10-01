# 작업 로그 765: CREDITS · LICENSE 정비와 사이트 크레딧 페이지

설계: [20261001-765-credits-and-license.md](../design/20261001-765-credits-and-license.md) ·
작업 지시: [20261001-765-credits-and-license.md](../work-orders/20261001-765-credits-and-license.md)

## 수행 내용

* 널리 쓰이는 관례대로 역할을 나눴다: `LICENSE`(프로젝트 자체 라이선스, GitHub 인식
  위치), `THIRD_PARTY_NOTICES.md`(법적 고지 목록), `CREDITS.md`(감사·출처 표기),
  사이트 크레딧 페이지(방문자용 요약 + 전문 링크).
* `LICENSE`: BSD 3-Clause 전문, `Copyright (c) 2026 Kitae Noh`. 라이선스 전문 관례대로
  영어 원문만 둔다.
* `THIRD_PARTY_NOTICES.md`: 누락돼 있던 SDL 3.4.10(zlib), spdlog v1.14.1(MIT, header-only,
  Emscripten 제외), GoogleTest v1.14.1(BSD-3, 테스트 전용) 절을 기존 한·영 형식으로
  추가했다. 이로써 CMakeLists.txt의 FetchContent·vendored 의존성이 모두 문서화됐다.
* `CREDITS.md`(신규, 한·영): 원작(Pump It Up, Andamiro) 비공식·무관 고지와 감사, MAME
  참조 코드(windyfairy, smf), 실행 파일 포함 오픈소스, 개발 도구, 사이트·문서 자원,
  전문 문서 링크.
* `README.md` 라이선스 절에서 "정식 LICENSE 파일이 없다"는 자리표시 문구를 제거하고
  `LICENSE`·`CREDITS.md` 링크로 바꿨다.
* 사이트: `site.toml`의 `[[credits]]`(언어 중립: key, group, name, version, license,
  url; 버전 표기는 `v3.4.10`/`ea99364`처럼 표시 문자열 그대로 둔다), 두 i18n 파일의
  `[nav] credits`·`[credits]` 절, `templates/credits.html`, `base.html` 메뉴의 F4
  크레딧(GitHub는 F5로), `build_site.py`의 `PAGES`·제목·그룹 컨텍스트,
  `site.css`의 `.credits` 그리드(모바일에서 한 열로 접힘), `docs/sites/README.md`의
  템플릿 목록 갱신.

## 검증

* WSL에서 `python3 scripts/site/build_site.py --offline` 통과, 내부 링크 검사 ok.
* headless Edge로 `/credits.html`(한국어)과 `/en/credits.html`(영어)을 캡처해 메뉴 항목,
  그룹 박스, 라이선스 배지, 버전 표기(minimp3는 commit `ea99364`), 전문 링크를 확인했다.
  처음 `v{{ version }}` 템플릿이 commit 해시에 `vea99364`로 붙는 문제를 발견해 버전
  문자열을 toml 데이터로 옮겼다.

## 회고

법적 고지(NOTICES)와 감사(CREDITS)를 분리하니 각 문서의 독자가 분명해졌다. 사이트
페이지를 site.toml 데이터로 만들어 두어 의존성이 바뀔 때 고칠 곳이 한 곳으로 모였다.

---

# Work Log 765: CREDITS · LICENSE cleanup and the site credits page

Design: [20261001-765-credits-and-license.md](../design/20261001-765-credits-and-license.md) ·
Work order: [20261001-765-credits-and-license.md](../work-orders/20261001-765-credits-and-license.md)

## Work performed

* Split the roles along widely used conventions: `LICENSE` (the project's own licence in
  the GitHub-recognised location), `THIRD_PARTY_NOTICES.md` (the legal inventory),
  `CREDITS.md` (acknowledgements), and a site credits page (visitor-facing summary with
  full-text links).
* `LICENSE`: the BSD 3-Clause text, `Copyright (c) 2026 Kitae Noh`, kept in its original
  English as licence texts conventionally are.
* `THIRD_PARTY_NOTICES.md`: added the missing SDL 3.4.10 (zlib), spdlog v1.14.1 (MIT,
  header-only, not fetched for Emscripten) and GoogleTest v1.14.1 (BSD-3, test-only)
  sections in the existing bilingual format. Every FetchContent and vendored dependency in
  `CMakeLists.txt` is now documented.
* `CREDITS.md` (new, bilingual): the unofficial/no-affiliation notice and thanks to the
  original game, the MAME references (windyfairy, smf), the open source in the
  executables, the development tools, the site and documentation resources, and links to
  the full documents.
* `README.md`: the "no formal LICENSE file" placeholder is gone, replaced with links to
  `LICENSE` and `CREDITS.md`.
* Site: the `[[credits]]` list in `site.toml` (language-neutral key, group, name, version,
  license, url; versions are display strings such as `v3.4.10` / `ea99364`), the `[nav]
  credits` and `[credits]` sections in both i18n files, `templates/credits.html`, the F4
  menu entry in `base.html` (GitHub moved to F5), the `PAGES` / title / group context in
  `build_site.py`, the `.credits` grid in `site.css` (collapsing to one column on
  mobile), and the template list in `docs/sites/README.md`.

## Verification

* `python3 scripts/site/build_site.py --offline` passed in WSL with the internal link
  check ok.
* Captured `/credits.html` (Korean) and `/en/credits.html` (English) with headless Edge
  and confirmed the menu entry, group boxes, licence badges, version strings (minimp3 as
  commit `ea99364`) and full-text links. An initial `v{{ version }}` template rendered the
  commit hash as `vea99364`, so the version strings moved into the toml data.

## Retrospective

Separating the legal notices from the acknowledgements made each document's audience
clear, and driving the site page from `site.toml` leaves one place to edit when a
dependency changes.
