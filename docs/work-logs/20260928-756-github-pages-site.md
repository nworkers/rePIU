# Task 756 작업 로그 — GitHub Pages 프로젝트 사이트

설계: [20260928-756](../design/20260928-756-github-pages-site.md)
작업 지시: [20260928-756](../work-orders/20260928-756-github-pages-site.md)

## 한국어

### 요약

`docs/sites/`를 소스로, `scripts/site/build_site.py`가 정적 사이트를 `build/site/`에 만들고
`.github/workflows/pages.yml`이 GitHub Pages에 배포합니다. 검정 바탕·VGA 16색·Galmuri 픽셀 폰트의 DOS
스타일이며, 소개·진행 상황(개발 기록과 릴리스 타임라인)·다운로드 세 페이지와 글 페이지를 한국어(`/`)와
영어(`/en/`)로 만듭니다. 로컬 빌드와 링크 검사, headless Edge 캡처로 확인했습니다. **실제 배포는 아직
확인하지 않았습니다** — main 머지 후 저장소 설정에서 Pages Source를 GitHub Actions로 바꿔야 합니다.

### 구현

* `docs/sites/`: `site.toml`(저장소·언어·경로·플랫폼 상태), `i18n/ko.toml`·`en.toml`, Jinja2 템플릿 6개,
  `static/`(CSS, JS, favicon, Galmuri 2.40.3 woff2 3개와 `OFL.txt`), `README.md`.
* `scripts/site/content.py`: fence를 추적하는 한/영 분리, markdown-it-py 렌더링(표·취소선·제목 id), 상대
  링크를 GitHub URL 또는 사이트 글 페이지로 재작성, 요약 추출, `target_profile.cpp`에서 ROM 세트 목록 읽기.
* `scripts/site/releases.py`: Releases API(페이지 넘김, `digest`에서 SHA-256) 또는 `--offline`(노트 파일과
  `git for-each-ref` 태그 날짜), 노트 파일 결합.
* `scripts/site/build_site.py`: 페이지 생성, 언어별 상대 경로·`hreflang`·첫 방문 언어 이동, 절대 경로 404,
  산출물 내부 링크 검사(깨지면 실패).
* `.github/workflows/pages.yml`: main 경로 변경·Release 성공(`workflow_run`)·수동 실행 때 배포, PR은 빌드만.
* 문서: `README.md`(사이트 링크, 디렉터리 표), `docs/post/README.md`(게시 규칙), `THIRD_PARTY_NOTICES.md`
  (Galmuri, 빌드 도구), `.gitattributes`(`*.woff2 binary`).

### 설계에서 바뀐 것

* **폰트 조합:** 처음 계획은 본문 Galmuri11이었으나, 픽셀 폰트는 설계 크기의 정수배에서만 선명해 본문을
  Galmuri14 15 px로 바꿨습니다. 제목은 Galmuri11-Bold 24·36·48 px, 코드 블록은 GalmuriMono11 12 px입니다.
  인라인 코드와 날짜는 12 px가 작아 본문 폰트 15 px로 표시합니다. 설계 문서에 반영했습니다.
* **소개 페이지 다이어그램은 ASCII:** fontTools로 확인한 결과 GalmuriMono11은 박스 문자(`─│┌`)와 화살표
  (`▼◄`)의 advance가 1200/1200 em(전각), 라틴과 `·`는 600(반각)이었습니다. 박스 문자를 쓰면 정렬이
  어긋나므로 `+ = - | v <`로 그렸습니다. 글과 릴리스 노트에는 박스 문자가 없음을 확인했습니다(0개).
* **action 버전:** 작성 시점 최신 메이저(checkout v7, setup-python v7, configure-pages v6,
  upload-pages-artifact v5, deploy-pages v5)를 썼습니다. 릴리스 노트의 파괴적 변경(`pull_request_target`
  기본값, `pip-install` 입력 제거, dotfile 제외)은 이 워크플로의 입력과 산출물에 해당하지 않습니다.

### 검증

WSL Ubuntu 24.04의 Python 3.12(시스템 Jinja2 3.1.2, markdown-it-py 4.2.0·mdit-py-plugins 0.6.1·mdurl
0.1.2 wheel)로 빌드했습니다. Windows에는 Python이 없고 WSL에는 pip이 없어 wheel을 풀어 `PYTHONPATH`로
썼습니다. CI는 `requirements.txt`의 고정 버전을 설치합니다.

| 항목 | 결과 |
|---|---|
| ROM 세트 | 카탈로그에서 22개 |
| 글 | 6편 모두 한/영 분리. 제목은 영어 제목 5편, 한국어 제목 1편(`2026-07-13`) |
| 릴리스(online) | API 28개, 그중 27개가 노트 파일 사용. `v0.0.136`은 노트 파일 없음(자동 생성 본문) |
| 릴리스(offline) | 노트 파일 29개(`v0.0.151`은 노트가 있으나 GitHub 릴리스 없음) |
| 다운로드 | 최신 `v0.0.194`, 자산 2개와 SHA-256 표시 |
| 내부 링크 검사 | online·offline 모두 통과 |
| 워크플로 | PyYAML로 구문 확인(트리거 4개, job 2개) |

headless Edge 캡처(로컬 `http.server`):

* 한국어 소개(1280 px), 한국어 진행 상황, 영어 다운로드, Mermaid가 있는 글(`2026-08-12`): 레이아웃, 폰트,
  이중 테두리 박스, 표, Mermaid VGA 테마가 의도대로 나왔습니다.
* 360 px: headless 창은 최소 폭 때문에 360 px 캡처가 잘려 보였고, 360 px iframe 세 개로 다시 확인해 가로
  넘침이 없음을 확인했습니다. 넓은 표는 박스 안에서만 스크롤됩니다.
* `--lang=en-US`로 한국어 `index.html`을 열면 영어 페이지로 이동했습니다(저장된 선택 없음).

확인하지 못한 것: GitHub Actions에서의 실제 실행과 배포, `workflow_run` 연결, 404 페이지의 절대 경로(배포
주소에서만 의미가 있음).

### 사용자 조치

1. main 머지 후 **Settings → Pages → Build and deployment → Source = GitHub Actions**.
2. Actions에서 Pages 워크플로를 수동 실행하거나 다음 main push를 기다립니다.
3. <https://reexec.github.io/rePIU/>에서 두 언어 페이지를 확인합니다.

## English

### Summary

With `docs/sites/` as the source, `scripts/site/build_site.py` writes a static site to `build/site/` and
`.github/workflows/pages.yml` deploys it to GitHub Pages. It is DOS-styled — black background, the VGA
16-colour palette, Galmuri pixel fonts — and produces the introduction, WIP (dev log and release
timeline) and download pages plus post pages in Korean (`/`) and English (`/en/`). Checked with local
builds, the link check and headless Edge captures. **The real deployment is not confirmed yet** — after the
merge to main, the Pages source must be switched to GitHub Actions in the repository settings.

### Implementation

* `docs/sites/`: `site.toml` (repository, languages, paths, platform status), `i18n/ko.toml`/`en.toml`, six
  Jinja2 templates, `static/` (CSS, JS, favicon, three Galmuri 2.40.3 woff2 files and `OFL.txt`),
  `README.md`.
* `scripts/site/content.py`: the fence-aware Korean/English split, markdown-it-py rendering (tables,
  strikethrough, heading ids), relative links rewritten to GitHub URLs or site post pages, excerpts, and the
  ROM set list read from `target_profile.cpp`.
* `scripts/site/releases.py`: the Releases API (paginated, SHA-256 from `digest`) or `--offline` (notes
  files and tag dates from `git for-each-ref`), joined with notes files.
* `scripts/site/build_site.py`: page generation, per-language relative paths, `hreflang`, the first-visit
  language move, an absolute-URL 404, and the internal-link check of the output (fails when broken).
* `.github/workflows/pages.yml`: deploys on relevant path changes on main, a successful Release
  (`workflow_run`) and manual runs; pull requests only build.
* Documents: `README.md` (site link, layout table), `docs/post/README.md` (publishing rules),
  `THIRD_PARTY_NOTICES.md` (Galmuri, build tools), `.gitattributes` (`*.woff2 binary`).

### Changes from the design

* **Font set:** the plan had Galmuri11 for body text, but a pixel font is crisp only at whole multiples of
  its design size, so body text became Galmuri14 at 15 px. Headings are Galmuri11-Bold at 24/36/48 px and
  code blocks GalmuriMono11 at 12 px. Inline code and dates read too small at 12 px and use the body font
  at 15 px. The design document was updated.
* **The introduction diagram is ASCII:** fontTools showed GalmuriMono11's box-drawing glyphs (`─│┌`) and
  arrows (`▼◄`) advance 1200/1200 em (full width) while Latin and `·` advance 600 (half width). Box
  characters would misalign, so the diagram uses `+ = - | v <`. Posts and release notes contain no box
  characters (zero found).
* **Action versions:** the latest majors at the time of writing (checkout v7, setup-python v7,
  configure-pages v6, upload-pages-artifact v5, deploy-pages v5). Their breaking changes
  (`pull_request_target` defaults, the removed `pip-install` input, dotfiles excluded) do not touch this
  workflow's inputs or output.

### Verification

Built with Python 3.12 on WSL Ubuntu 24.04 (system Jinja2 3.1.2, with the markdown-it-py 4.2.0,
mdit-py-plugins 0.6.1 and mdurl 0.1.2 wheels). Windows has no Python and WSL has no pip, so the wheels were
unpacked and used through `PYTHONPATH`. CI installs the pinned versions in `requirements.txt`.

| Item | Result |
|---|---|
| ROM sets | 22 from the catalog |
| Posts | All six split into Korean and English. Five have English titles, one a Korean title (`2026-07-13`) |
| Releases (online) | 28 from the API, 27 using notes files. `v0.0.136` has no notes file (generated body) |
| Releases (offline) | 29 notes files (`v0.0.151` has notes but no GitHub release) |
| Downloads | Latest `v0.0.194`, two assets with SHA-256 |
| Internal-link check | Passed online and offline |
| Workflow | Syntax parsed with PyYAML (four triggers, two jobs) |

Headless Edge captures (local `http.server`):

* Korean introduction (1280 px), Korean WIP, English downloads, and a post with Mermaid (`2026-08-12`):
  layout, fonts, double-border boxes, tables and the VGA Mermaid theme came out as intended.
* 360 px: the headless window has a minimum width, so a 360 px capture looked cut off; three 360 px iframes
  confirmed there is no horizontal overflow. Wide tables scroll only inside their box.
* Opening the Korean `index.html` with `--lang=en-US` moved to the English page (no stored choice).

Not confirmed: the real run and deployment on GitHub Actions, the `workflow_run` chain, and the 404 page's
absolute URLs (meaningful only at the deployed address).

### User action

1. After the merge to main: **Settings → Pages → Build and deployment → Source = GitHub Actions**.
2. Run the Pages workflow manually from Actions, or wait for the next push to main.
3. Check both languages at <https://reexec.github.io/rePIU/>.
