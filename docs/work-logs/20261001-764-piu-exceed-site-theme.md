# 작업 로그 764: 프로젝트 사이트 Exceed 테마 적용

설계: [20261001-764-piu-exceed-site-theme.md](../design/20261001-764-piu-exceed-site-theme.md) ·
작업 지시: [20261001-764-piu-exceed-site-theme.md](../work-orders/20261001-764-piu-exceed-site-theme.md)

## 수행 내용

* `docs/sites/static/css/site.css`
  * VGA 팔레트에 빠져 있던 `--magenta`, `--light-magenta`를 추가하고, 네온 역할 토큰
    (`--neon-pink/cyan/yellow`)과 알파 글로우 색(`--glow-*`)을 도입했다. 면 색은 VGA 16색
    안에 머물고 글로우는 그림자 알파로만 표현한다.
  * 메뉴 바 아래 4px 그루브 미터풍 `.beatbar`(마젠타→노랑→시안), `body::after`의 정적
    주사선 오버레이, 마젠타 `::selection`과 `h3` 마커를 추가했다.
  * 히어로 타이틀에 `@supports (background-clip: text)` 분기로 흰색→시안→블루→마젠타
    그라데이션을 입히고, 기존 하드 섀도는 `drop-shadow` 필터로 유지했다. 미지원 환경은
    기존 흰색 + 파란 섀도에 핑크 글로우만 더해진다. 600px 미디어 쿼리에도 같은 분기를
    두어 섀도가 중복되지 않게 했다.
  * 발판 화살표 스타일(`.padline`, `.pad--up/down/center`)을 추가했다. 위 대각선 핑크,
    아래 대각선 시안, 중앙 노랑이며 중앙만 2초 글로우 펄스를 갖고
    `prefers-reduced-motion`에서 꺼진다.
  * 버튼 그라데이션과 호버 핑크 글로우, `.box`/`.box--warn` 프레임 글로우,
    `badge--latest`와 타임라인 최신 마커의 마젠타 전환을 적용했다.
* `docs/sites/templates/_padline.html`: 픽셀 화살표(↗) 하나를 rect로 그리고 나머지 세
  모서리는 미러 변환으로 만든 5패널 인라인 SVG 부분 템플릿. 빌드 스크립트는 템플릿을
  이름으로만 렌더링하므로 부분 템플릿이 페이지로 출력되지 않음을 확인했다.
* `docs/sites/templates/base.html`에 `.beatbar`, `index.html`·`404.html`에 패드라인
  include를 추가했다. `favicon.svg`는 커서 블록만 마젠타로 바꿨다.

## 검증

* Windows 호스트에 실제 Python이 없어(Store 스텁만 존재) WSL(Python 3.12.3)에서
  검증했다. WSL에 pip가 없고 PEP 668 보호가 있어 `get-pip.py`와
  `pip install --user --break-system-packages`로 의존성을 사용자 사이트에만 설치했다.
* `python3 scripts/site/build_site.py --offline` 통과: 대상 22개, 글 6편, 릴리스 31개,
  내부 링크 검사 ok.
* headless Edge로 `build/site/index.html`, `wip.html`을 캡처해 비트 바, 발판 화살표,
  타이틀 그라데이션, 박스 글로우, 주사선이 의도대로 렌더링되는 것을 확인했다.

## 회고

Exceed 레이어를 VGA 면 색 + 알파 글로우로 제한한 덕분에 DOS 텍스트 모드 정체성과
충돌하지 않았다. 픽셀 화살표를 rect 기반 한 벌 + 미러 변환으로 만들어 네 방향의 모양
일관성이 공짜로 얻어졌다.

---

# Work Log 764: Applying the Exceed theme to the project site

Design: [20261001-764-piu-exceed-site-theme.md](../design/20261001-764-piu-exceed-site-theme.md) ·
Work order: [20261001-764-piu-exceed-site-theme.md](../work-orders/20261001-764-piu-exceed-site-theme.md)

## Work performed

* `docs/sites/static/css/site.css`
  * Completed the VGA palette with the missing `--magenta` / `--light-magenta` and added
    neon role tokens (`--neon-pink/cyan/yellow`) plus alpha glow colours (`--glow-*`).
    Surfaces stay within VGA 16; glow lives only in shadow alphas.
  * Added the 4px groove-meter `.beatbar` under the menu bar (magenta→yellow→cyan), a
    static scanline overlay on `body::after`, and magenta `::selection` and `h3` markers.
  * Gave the hero title a white→cyan→blue→magenta gradient behind an
    `@supports (background-clip: text)` branch, keeping the hard shadow as a `drop-shadow`
    filter; unsupported browsers keep the old white-with-blue-shadow look plus a pink glow.
    The 600px media query got the same branch so shadows never double up.
  * Added the pad-arrow styles (`.padline`, `.pad--up/down/center`): upper diagonals pink,
    lower diagonals cyan, centre yellow, with a 2s glow pulse on the centre only, disabled
    under `prefers-reduced-motion`.
  * Applied the button gradient with a pink hover glow, frame glows on `.box` /
    `.box--warn`, and the magenta `badge--latest` and latest timeline marker.
* `docs/sites/templates/_padline.html`: a five-panel inline-SVG partial that draws one
  pixel up-right arrow from rects and mirrors it into the other three corners. The build
  script renders templates by explicit name, so the partial never becomes a page.
* Added the `.beatbar` to `base.html` and the padline include to `index.html` / `404.html`.
  Only the cursor block of `favicon.svg` turned magenta.

## Verification

* The Windows host has no real Python (only the Store stub), so verification ran in WSL
  (Python 3.12.3). WSL had no pip and enforces PEP 668, so dependencies were installed to
  the user site only, via `get-pip.py` and `pip install --user --break-system-packages`.
* `python3 scripts/site/build_site.py --offline` passed: 22 targets, 6 posts, 31 releases,
  internal link check ok.
* Captured `build/site/index.html` and `wip.html` with headless Edge and confirmed the
  beat bar, pad arrows, title gradient, box glows, and scanlines render as intended.

## Retrospective

Restricting the Exceed layer to VGA surfaces plus alpha glow kept it from fighting the DOS
text-mode identity. Building the pixel arrow once from rects and mirroring it bought the
four directions' consistency for free.
