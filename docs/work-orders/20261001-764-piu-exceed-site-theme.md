# 작업 지시 764: 프로젝트 사이트 Exceed 테마 적용

설계: [20261001-764-piu-exceed-site-theme.md](../design/20261001-764-piu-exceed-site-theme.md)

## 작업 항목

1. `docs/sites/static/css/site.css`
   * VGA 팔레트에 `--magenta`, `--light-magenta` 추가, 네온 역할 토큰과 글로우 색 추가.
   * 비트 바, 주사선 오버레이, 히어로 타이틀 그라데이션(`@supports` 분기), 발판 화살표
     스타일(글로우, 중앙 펄스 + reduced-motion 예외), 버튼·박스·배지·타임라인·선택
     영역·`h3` 마커의 네온 강조를 구현.
2. `docs/sites/templates/base.html`: 메뉴 바 아래 `.beatbar` 추가.
3. `docs/sites/templates/index.html`, `404.html`: 히어로에 5패널 픽셀 화살표 인라인 SVG
   (`aria-hidden`) 추가.
4. `docs/sites/static/favicon.svg`: 커서 블록을 마젠타로 변경.

## 완료 조건

* `python scripts/site/build_site.py --offline` 빌드와 내부 링크 검사가 통과한다.
* index / wip / download / post / 404 페이지가 로컬에서 의도대로 렌더링된다.
* 문구(i18n), 빌드 스크립트, Mermaid 테마는 변경되지 않는다.

---

# Work Order 764: Apply the Exceed theme to the project site

Design: [20261001-764-piu-exceed-site-theme.md](../design/20261001-764-piu-exceed-site-theme.md)

## Tasks

1. `docs/sites/static/css/site.css`
   * Add `--magenta` / `--light-magenta` to the VGA palette, plus neon role tokens and glow
     colours.
   * Implement the beat bar, the scanline overlay, the hero title gradient (with an
     `@supports` branch), the pad-arrow styles (glow, centre pulse with a reduced-motion
     exception), and the neon accents on buttons, boxes, badges, the timeline, selection,
     and the `h3` marker.
2. `docs/sites/templates/base.html`: add the `.beatbar` under the menu bar.
3. `docs/sites/templates/index.html`, `404.html`: add the five-panel inline pixel-arrow SVG
   (`aria-hidden`) to the hero.
4. `docs/sites/static/favicon.svg`: turn the cursor block magenta.

## Completion criteria

* `python scripts/site/build_site.py --offline` passes, including the internal link check.
* The index / wip / download / post / 404 pages render as intended locally.
* The i18n copy, the build script, and the Mermaid theme are unchanged.
