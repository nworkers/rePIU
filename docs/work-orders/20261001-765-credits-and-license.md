# 작업 지시 765: CREDITS · LICENSE 정비와 사이트 크레딧 페이지

설계: [20261001-765-credits-and-license.md](../design/20261001-765-credits-and-license.md)

## 작업 항목

1. 저장소 루트
   * `LICENSE`: BSD 3-Clause 전문 (`Copyright (c) 2026 Kitae Noh`).
   * `CREDITS.md`: 원작 고지·감사, MAME 참조, 오픈소스·글꼴 목록, 전문 링크 (한·영).
   * `THIRD_PARTY_NOTICES.md`: SDL 3.4.10, spdlog v1.14.1, GoogleTest v1.14.1 절 추가.
   * `README.md`: 라이선스 절을 `LICENSE`·`CREDITS.md` 기준으로 갱신.
2. 사이트
   * `site.toml`: `[[credits]]` 목록(runtime / dev / site 그룹).
   * `i18n/ko.toml`, `en.toml`: `[nav] credits`, `[credits]` 절 (같은 키 구조).
   * `templates/credits.html` 신규, `templates/base.html` 메뉴에 F4 크레딧 추가
     (GitHub는 F5).
   * `scripts/site/build_site.py`: `PAGES`에 credits 추가, 제목·그룹 컨텍스트.
   * `static/css/site.css`: `.credits` 목록 스타일.

## 완료 조건

* `python scripts/site/build_site.py --offline`과 내부 링크 검사 통과.
* `/credits.html`, `/en/credits.html`이 의도대로 렌더링되고 메뉴에서 접근 가능.
* THIRD_PARTY_NOTICES.md가 CMakeLists.txt의 FetchContent·vendored 의존성을 모두 다룬다.

---

# Work Order 765: CREDITS · LICENSE cleanup and a site credits page

Design: [20261001-765-credits-and-license.md](../design/20261001-765-credits-and-license.md)

## Tasks

1. Repository root: `LICENSE` (BSD 3-Clause, `Copyright (c) 2026 Kitae Noh`); a bilingual
   `CREDITS.md` (original-game notice and thanks, MAME references, open-source and font
   lists, full-text links); `THIRD_PARTY_NOTICES.md` sections for SDL 3.4.10, spdlog
   v1.14.1 and GoogleTest v1.14.1; a README licence section updated around the real
   `LICENSE` and `CREDITS.md`.
2. Site: the `[[credits]]` list in `site.toml` (groups runtime / dev / site); `[nav]
   credits` and a `[credits]` section in both i18n files with identical key structure; a
   new `templates/credits.html`; the F4 menu entry in `base.html` (GitHub moves to F5);
   `credits` in `PAGES` of `build_site.py` with title and group context; `.credits` styles
   in `site.css`.

## Completion criteria

* `python scripts/site/build_site.py --offline` and its internal link check pass.
* `/credits.html` and `/en/credits.html` render as intended and are reachable from the menu.
* `THIRD_PARTY_NOTICES.md` covers every FetchContent and vendored dependency in
  `CMakeLists.txt`.
