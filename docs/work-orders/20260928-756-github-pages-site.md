# Task 756: GitHub Pages 프로젝트 사이트 작업 지시

설계: [20260928-756](../design/20260928-756-github-pages-site.md)

## 한국어

1. `docs/sites/`에 `site.toml`, `i18n/ko.toml`, `i18n/en.toml`, Jinja2 템플릿(`base`, `index`, `wip`,
   `download`, `post`, `404`), `static/`(CSS, JS, favicon, Galmuri 2.40.3 woff2 세 개와 `OFL.txt`)을 둔다.
2. `scripts/site/`에 빌드를 둔다.
   * `content.py` — 한/영 분리, Markdown 렌더링, 상대 링크 재작성, 요약 추출.
   * `releases.py` — Releases API 조회와 `--offline` 대체, 노트 파일 결합.
   * `build_site.py` — 페이지 생성, static 복사, 내부 링크 검사.
   * `requirements.txt` — `markdown-it-py`, `mdit-py-plugins`, `Jinja2` 고정 버전.
3. `.github/workflows/pages.yml`을 설계의 트리거·권한·동시성대로 추가한다.
4. `docs/sites/README.md`(구조, 로컬 빌드, 콘텐츠 갱신 방법), `docs/post/README.md`(사이트 게시 규칙),
   `README.md`(사이트 링크), `THIRD_PARTY_NOTICES.md`(Galmuri, 빌드 의존성)를 갱신한다.
5. WSL에서 offline·online 빌드, 링크 검사, headless Edge 캡처로 확인하고 작업 로그를 남긴 뒤 커밋한다.

## English

1. Put `site.toml`, `i18n/ko.toml`, `i18n/en.toml`, the Jinja2 templates (`base`, `index`, `wip`,
   `download`, `post`, `404`) and `static/` (CSS, JS, favicon, three Galmuri 2.40.3 woff2 files and
   `OFL.txt`) under `docs/sites/`.
2. Put the build under `scripts/site/`.
   * `content.py` — the Korean/English split, Markdown rendering, relative-link rewriting, excerpts.
   * `releases.py` — the Releases API query with the `--offline` fallback, joined with notes files.
   * `build_site.py` — page generation, static copy, internal-link check.
   * `requirements.txt` — pinned `markdown-it-py`, `mdit-py-plugins`, `Jinja2`.
3. Add `.github/workflows/pages.yml` with the design's triggers, permissions and concurrency.
4. Update `docs/sites/README.md` (layout, local build, how content is refreshed), `docs/post/README.md`
   (publishing rule), `README.md` (site link) and `THIRD_PARTY_NOTICES.md` (Galmuri, build dependencies).
5. Check with offline and online builds in WSL, the link check and headless Edge captures, write the work
   log and commit.
