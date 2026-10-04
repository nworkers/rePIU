# Task 770 설계: 릴리스 스크린샷과 shader 개발 기록

작업 지시: [20261005-770](../work-orders/20261005-770-release-screenshots-and-shader-post.md) ·
작업 로그: [20261005-770](../work-logs/20261005-770-release-screenshots-and-shader-post.md)

## 배경

Task 768(화면 후처리 shader)과 769(전체화면·비율 유지)를 머지하기 전에 사용자가 다음을 요청했습니다(2026-10-05).

1. 게임별 스크린샷을 캡처해 `docs` 아래에 저장한다.
2. 그 스크린샷을 README와 프로젝트 사이트의 소개에 넣는다. 양식은 re2DJ의 작업 453(v0.0.63)을 따른다.
3. shader를 적용한 화면을 가장 잘 보이는 장면으로 종류별 3장씩 캡처하고, shader 구현에 대한 WIP 글을 써서
   스크린샷을 첨부한다.
4. 릴리스 노트를 쓰고 main에 머지한다.

## 결정

* **스크린샷 위치:** `docs/screenshots/`. README는 저장소 상대 경로로 쓰고, 사이트 빌드는 이 디렉터리를
  산출물의 `screenshots/`로 복사해 같은 파일을 씁니다. re2DJ와 같은 이유로 사이트 전용 자산(`static/`)과
  나눕니다. 목록과 캡처 방법은 디렉터리 README에 둡니다.
* **게임별 범위:** CHD가 있어 실행되는 16개 롬셋마다 타이틀(어트랙트) 화면 한 장. Win32 x86 Release로
  기본 창(2배, 1280×960)에서 입력 없이 실행하고, 3초마다 창의 클라이언트 영역을 캡처해 대조표에서 고릅니다.
  640×480 JPEG(품질 90)로 줄입니다(re2DJ와 같음).
* **캡처 방식:** 창이 가려지거나 다른 모니터에 열려도 찍히도록 `PrintWindow`(`PW_CLIENTONLY |
  PW_RENDERFULLCONTENT`)를 먼저 쓰고, 검게 나오면 화면 복사로 넘어갑니다. DPI 인식 프로세스에서 실행합니다
  (4K 150% 모니터). 스크립트는 일회성이라 scratchpad에만 둡니다.
* **shader 스크린샷:** `docs/screenshots/shaders/`. 종류(`crt`, `scanline`)별로 3장씩입니다. **640×480으로
  줄이지 않고 2배 창 그대로(1280×960) 둡니다.** 두 shader의 주사선은 출력 픽셀 단위 무늬라 원래 해상도로
  줄이면 사라지거나 모아레가 됩니다(Task 768에서 2배율 주사선을 맞춘 이유와 같음). 장면은 shader 효과가 잘
  보이는 밝고 색이 많은 화면(타이틀, 곡 선택, 플레이)을 고르고, 비교용으로 같은 장면의 `none`도 찍습니다.
* **WIP 글:** `docs/post/` 지침대로 한국어 전체 → `---` → 영어 전체, `## 주요 변경 사항`과
  `## 사용된 기술 스택`을 둡니다. 이미지는 `../screenshots/shaders/…` 상대 경로이며, 사이트 빌드가 GitHub raw
  주소로 바꿉니다.
* **사이트:** 목록과 순서는 `site.toml`의 `[[screenshots]]`(key, file), 설명은 `i18n/*.toml`의
  `[screenshots.captions]`. 소개 페이지 hero 아래에 격자로 보이고, 각 이미지는 원본 크기 파일로 연결합니다.
* **릴리스:** v0.0.199 이후 변경(Tasks 768~770)을 `docs/release-notes/v0.0.200.md`로 쓰고, AGENTS.md의 머지
  규칙대로 `VERSION`을 0.0.200으로 올려 브랜치 전체를 main에 squash 머지하고 `v0.0.200` annotated tag를
  로컬에 붙입니다. push는 하지 않습니다.

## 검증

* 사이트 `build_site.py --offline`이 성공하고 내부 링크 검사가 통과하며, 두 언어의 `index.html`에 이미지가
  16장 있습니다. 렌더링을 브라우저로 확인합니다.
* README와 글의 이미지 경로가 실제 파일을 가리킵니다.
* 제품 코드는 바꾸지 않습니다. 캡처에 쓴 Win32 Release 빌드가 이 브랜치로 성공했는지 확인합니다.

---

# Task 770 Design: Release Screenshots and the Shader Dev Log

Work order: [20261005-770](../work-orders/20261005-770-release-screenshots-and-shader-post.md) ·
Work log: [20261005-770](../work-logs/20261005-770-release-screenshots-and-shader-post.md)

## Background

Before merging Task 768 (post-processing shaders) and Task 769 (fullscreen and aspect), the user asked
(2026-10-05) to capture per-game screenshots under `docs`, show them in the README and on the project
site's introduction in the format of re2DJ's Task 453 (v0.0.63), capture three shots of each shader in
the scenes that show it best and write a WIP post on the shader implementation with them attached, and
then write the release notes and merge to main.

## Decisions

* **Location:** `docs/screenshots/`. The README links them by repository path and the site build copies
  the directory to `screenshots/` in its output, so both use one source, kept apart from the site-only
  assets in `static/` as in re2DJ. A README in the directory lists them and how they were taken.
* **Per-game scope:** one title (attract) screen for each of the 16 ROM sets that have a CHD and run,
  with the Win32 x86 Release build in the default 2x window (1280×960) and no input, the client area
  captured every 3 s and one frame chosen from contact sheets, scaled to 640×480 JPEG at quality 90 (as
  in re2DJ).
* **Capture:** `PrintWindow` (`PW_CLIENTONLY | PW_RENDERFULLCONTENT`) first, so a covered window or one
  on another monitor is still captured, falling back to a screen copy when it comes back black; run in
  a DPI-aware process (a 4K monitor at 150%). The script is one-off and stays in the scratchpad.
* **Shader screenshots:** `docs/screenshots/shaders/`, three each of `crt` and `scanline`, **kept at the
  2x window's 1280×960 rather than scaled to 640×480**: both shaders draw their scanlines per output
  pixel, and scaling back to the original resolution erases them or turns them into moiré (the same
  reason Task 768 aligned the 2x scanlines). The scenes are bright, colourful screens where the effect
  shows (title, song select, play), with the same scene under `none` taken for comparison.
* **WIP post:** in `docs/post/` per its guidelines: the full Korean document, `---`, the full English
  document, with `## 주요 변경 사항` and `## 사용된 기술 스택`. Images use relative
  `../screenshots/shaders/…` paths, which the site build turns into GitHub raw URLs.
* **Site:** the list and order in `[[screenshots]]` (key, file) of `site.toml`, captions in
  `[screenshots.captions]` of `i18n/*.toml`, shown as a grid below the hero with each image linking to
  its full-size file.
* **Release:** the changes since v0.0.199 (Tasks 768 to 770) as `docs/release-notes/v0.0.200.md`; per the
  AGENTS.md merge rules, `VERSION` goes to 0.0.200, the whole branch is squashed into main, and an
  annotated `v0.0.200` tag is created locally. Nothing is pushed.

## Verification

* The site's `build_site.py --offline` succeeds with its internal-link check, both languages'
  `index.html` hold 16 images, and the render is checked in a browser.
* The README's and the post's image paths name real files.
* No product code changes; the Win32 Release build used for the captures is confirmed to build from
  this branch.
