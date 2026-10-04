# Task 770 작업 로그: 릴리스 스크린샷과 shader 개발 기록

설계: [20261005-770](../design/20261005-770-release-screenshots-and-shader-post.md) ·
작업 지시: [20261005-770](../work-orders/20261005-770-release-screenshots-and-shader-post.md)

## 한 일

* **빌드:** 이 브랜치로 Win32 x86 Release 전체 빌드, exit 0.
* **게임별 스크린샷:** 16개 롬셋을 90초씩 입력 없이 실행하며 3초마다 캡처했습니다(scratchpad의 PowerShell 스크립트,
  DPI 인식, `PrintWindow`의 `PW_CLIENTONLY | PW_RENDERFULLCONTENT`, 검으면 화면 복사). 16개 모두 경고·로고·타이틀·
  데모 플레이까지 그렸습니다. 대조표에서 타이틀 15장과 데모 플레이 2장(`pumpitpru`는 타이틀이 `pumpitpr`과 같아 데모
  플레이, `pumpitp3`은 타이틀과 데모 플레이)을 골라 640×480 JPEG(품질 90)로 `docs/screenshots/`에 두었습니다(합계
  1.4 MB). 고른 17장은 모두 `PrintWindow`로 얻은 것입니다.
* **shader 스크린샷:** `pumpit8`을 `scripts/input_scripts/pumpit8_play.txt`로 `none`·`crt`·`scanline`에서 80초씩
  실행하며 2초마다 캡처했습니다. 세 실행의 장면이 같은 시각에 맞았습니다. 타이틀(REBIRTH 로고), 모드 선택, 플레이 세
  장면을 골라 shader마다 3장씩 1280×960 JPEG(품질 92)로, 장면마다 1:1 잘라 붙인 비교 띠를 PNG로
  `docs/screenshots/shaders/`에 두었습니다(합계 3.5 MB). 세 실행 모두 60 FPS, 로그의 `[repiu-post] shader:`가 각각
  `none`, `crt (6 parameters)`, `scanline (2 parameters)`입니다.
* **README:** "스크린샷" 절(3열 표)과 shader 글 링크.
* **사이트:** `build_site.py`(`docs/screenshots` 복사, index context), `index.html` hero 아래 격자, `site.css`,
  `site.toml` `[[screenshots]]` 17개, 두 `i18n`의 `[screenshots]`, 사이트 README.
* **WIP 글:** `docs/post/2026-10-05-020000-post-process-shaders-wip.md`. 이미지 12개 첨부.
* **릴리스 노트:** `docs/release-notes/v0.0.200.md`(v0.0.197 이후, Tasks 764~770).

## 검증

* `scripts/site/build_site.py --offline`(scratchpad venv): 성공, `internal links: ok`, posts 7개(새 글 포함),
  릴리스 타임라인에 v0.0.200. 두 언어 `index.html`에 `screenshots/` 참조 17개. 글의 이미지는
  `https://github.com/nworkers/rePIU/raw/main/docs/screenshots/shaders/…`로 바뀌어 main push 뒤에 보입니다.
* 헤드리스 Edge로 한국어 소개(1280 폭)와 영어 소개(390 폭)를 렌더링해 격자와 설명을 눈으로 확인했습니다. 1280
  폭에서는 3열, 좁은 폭에서는 1열입니다. 390 폭 렌더는 오른쪽이 잘렸지만 변경 전 사이트도 같은 자리에서 잘려,
  헤드리스 Edge의 최소 창 폭 때문으로 봅니다. 격자 항목에는 이미지 고유 폭(640)이 열을 넓히지 않도록
  `min-width: 0`을 두었습니다.
* 콘솔이 cp949이면 빌드 스크립트가 글 제목의 `—`를 출력하다 `UnicodeEncodeError`로 멈춥니다(기존 동작, 이번 변경과
  무관). `PYTHONIOENCODING=utf-8`로 돌렸습니다.

## 결함이 아닌 것

* `pumpit1`의 첫 BGA(검은 화면 뒤 사람 실루엣 영상)는 2배 창에서 격자 무늬로 보입니다. 처음에는 결함으로 의심해
  v0.0.197 릴리스 바이너리와 비교했고 같은 모양이었습니다. 이후 사용자가 **원본 게임에서도 그렇게 나오는 의도된
  영상**이라고 확인했습니다(2026-10-05). 조사 대상이 아닙니다.

---

# Task 770 Work Log: Release Screenshots and the Shader Dev Log

Design: [20261005-770](../design/20261005-770-release-screenshots-and-shader-post.md) ·
Work order: [20261005-770](../work-orders/20261005-770-release-screenshots-and-shader-post.md)

## Done

* **Build:** full Win32 x86 Release build of this branch, exit 0.
* **Per-game screenshots:** the 16 ROM sets ran for 90 s each with no input, captured every 3 s (a PowerShell script
  in the scratchpad, DPI-aware, `PrintWindow` with `PW_CLIENTONLY | PW_RENDERFULLCONTENT`, a screen copy when black).
  All 16 drew the warning, logo, title and demo play. From contact sheets, 15 title frames and 2 demo-play frames
  (`pumpitpru` shares `pumpitpr`'s title, so demo play; `pumpitp3` both) went to `docs/screenshots/` as 640×480 JPEG
  at quality 90 (1.4 MB in all). All 17 chosen frames came from `PrintWindow`.
* **Shader screenshots:** `pumpit8` ran with `scripts/input_scripts/pumpit8_play.txt` for 80 s each under `none`,
  `crt` and `scanline`, captured every 2 s; the scenes of the three runs lined up in time. Three scenes (the REBIRTH
  title, mode select, play) went to `docs/screenshots/shaders/` as three 1280×960 JPEGs per shader at quality 92,
  with a 1:1 side-by-side crop strip per scene as PNG (3.5 MB in all). All three ran at 60 FPS, and the log's
  `[repiu-post] shader:` reads `none`, `crt (6 parameters)` and `scanline (2 parameters)`.
* **README:** a "Screenshots" section (a three-column table) and a link to the shader post.
* **Site:** `build_site.py` (copying `docs/screenshots`, the index context), the grid below the hero in
  `index.html`, `site.css`, 17 `[[screenshots]]` in `site.toml`, `[screenshots]` in both i18n files, the site README.
* **WIP post:** `docs/post/2026-10-05-020000-post-process-shaders-wip.md`, with 12 images.
* **Release notes:** `docs/release-notes/v0.0.200.md` (since v0.0.197, Tasks 764 to 770).

## Verification

* `scripts/site/build_site.py --offline` (scratchpad venv): succeeds with `internal links: ok`, 7 posts including the
  new one, and v0.0.200 on the release timeline; both languages' `index.html` reference `screenshots/` 17 times. The
  post's images become `https://github.com/nworkers/rePIU/raw/main/docs/screenshots/shaders/…` and show once main is
  pushed.
* Headless Edge renders of the Korean introduction (1280 wide) and the English one (390 wide) were checked by eye:
  three columns at 1280, one when narrow. The 390-wide render is cut on the right, but the site before this change
  is cut at the same place, so it is put down to headless Edge's minimum window width. Grid items carry
  `min-width: 0` so the images' intrinsic width (640) never widens a column.
* On a cp949 console the build script stops with `UnicodeEncodeError` printing a post title's `—` (existing
  behaviour, unrelated to this change); it was run with `PYTHONIOENCODING=utf-8`.

## Not a defect

* `pumpit1`'s first BGA (a silhouette film after the black screen) shows a grid pattern in the 2x window. It was
  first suspected as a defect and compared with the v0.0.197 release binary, which looked the same; the user then
  confirmed that **the original game shows it that way, by design** (2026-10-05). It is not to be investigated.
