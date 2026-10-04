# Task 770 작업 지시: 릴리스 스크린샷과 shader 개발 기록

설계: [20261005-770](../design/20261005-770-release-screenshots-and-shader-post.md)

## 절차

1. 이 브랜치로 Win32 x86 Release를 빌드합니다.
2. 16개 롬셋을 입력 없이 90초씩 실행하며 3초마다 캡처하고, 대조표에서 타이틀 화면을 골라 640×480 JPEG로
   `docs/screenshots/`에 둡니다. 디렉터리 README에 목록과 캡처 방법을 적습니다.
3. `crt`·`scanline`·`none`으로 같은 장면을 캡처해 종류별 3장을 1280×960 JPEG로 `docs/screenshots/shaders/`에
   둡니다.
4. README에 스크린샷 절을 더합니다.
5. 사이트: `site.toml` `[[screenshots]]`, `i18n` `[screenshots]`, `index.html` 격자, `site.css`, `build_site.py`의
   복사와 context, 사이트 README.
6. `docs/post/`에 shader 구현 WIP 글을 쓰고 shader 스크린샷을 첨부합니다.
7. `docs/release-notes/v0.0.200.md`, `VERSION` 0.0.200.
8. 사이트 빌드·링크 검사 후 브랜치 전체를 main에 squash 머지하고 `v0.0.200` 태그를 로컬에 답니다.

## 완료 조건

사이트 빌드와 링크 검사가 통과하고 렌더링이 확인되며, Win32 Release 빌드가 성공하고, main에 v0.0.200 커밋과
태그가 있습니다.

---

# Task 770 Work Order: Release Screenshots and the Shader Dev Log

Design: [20261005-770](../design/20261005-770-release-screenshots-and-shader-post.md)

## Steps

1. Build Win32 x86 Release from this branch.
2. Run the 16 ROM sets for 90 s each with no input, capturing every 3 s; choose a title screen from the
   contact sheets and put it in `docs/screenshots/` as 640×480 JPEG, with a README listing them and how
   they were taken.
3. Capture the same scenes under `crt`, `scanline` and `none`, and put three of each shader in
   `docs/screenshots/shaders/` as 1280×960 JPEG.
4. Add a screenshots section to the README.
5. Site: `[[screenshots]]` in `site.toml`, `[screenshots]` in the i18n files, the grid in `index.html`,
   `site.css`, the copy and context in `build_site.py`, and the site README.
6. Write a WIP post on the shader implementation in `docs/post/` with the shader screenshots attached.
7. `docs/release-notes/v0.0.200.md` and `VERSION` 0.0.200.
8. After the site build and link check, squash the whole branch into main and tag `v0.0.200` locally.

## Done when

The site build and link check pass with the render checked, the Win32 Release build succeeds, and main
carries the v0.0.200 commit and tag.
