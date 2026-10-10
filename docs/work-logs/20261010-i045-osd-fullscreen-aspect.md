# 작업 로그: OSD의 전체 화면·비율 유지 옵션과 `cfg/repiu.ini` 저장 (issue #45)

설계: `docs/design/20261010-i045-osd-fullscreen-aspect.md`
작업 지시: `docs/work-orders/20261010-i045-osd-fullscreen-aspect.md`

## 한 일

* **그림 사각형**: `ComputeGlidePictureRect(..., keep_aspect)`를 더했다. 켜면 Task 769의 letterbox
  사각형, 끄면 drawable 전체다. 백엔드의 `ApplyDrawableViewport`가 이것을 쓴다.
* **세션 sink**: `repiu/engine/session_display_preferences.h/.cpp`. 엔진은 런처 타입을 모르고, 사용자
  조작으로 값이 바뀌었을 때만 sink를 부른다.
* **백엔드**: `keep_aspect_`(`REPIU_GLIDE_KEEP_ASPECT`, 기본 켬)와 시작 시 전체 화면
  (`REPIU_GLIDE_FULLSCREEN`, 기본 끔; 배율 창을 만든 뒤 진입). `ToggleFullscreen`을 `IsFullscreen`·
  `SetFullscreen(fullscreen, report)`로 나눴고, OSD 요청은 다음 이벤트 펌프의
  `ApplyPendingDisplayOptions`가 적용한 뒤 두 값을 한 번에 알린다. SDL의 전환이 비동기라 알리는 값은
  창 플래그를 다시 읽지 않고 요청한 값이다.
* **OSD**: 정보 줄 아래 맨 위에 "Fullscreen", "Keep aspect ratio" 체크박스(`GlideOsdDisplayOptions`).
* **런처 설정**: `[Video] fullscreen`·`keep_aspect`의 읽기(0/1 외 경고)·쓰기, 환경 변수 게시와 호출자
  우선, `SettingsDiffer`. 0/1 키 읽기를 `LoadVideoSwitch`로 묶어 `texture_full_precision`도 그것을 쓴다.
* **로더**: 게시 로그 줄에 두 값, 게임 실행 전 sink 등록(`StoreSessionDisplayPreferences`: 파일을 다시
  읽어 두 키만 바꿔 저장).
* **런처 창(추가 요청)**: 저장값대로 전체 화면으로 열고 체크박스·`Alt+Enter`로 바로 전환(Alt+Enter는
  ImGui로 넘기지 않음), 비율 유지면 960×640 비율 영역을 가운데에 그리고 둘레를 검게. Options 순서는
  Fullscreen, Keep aspect ratio, vsync, Full-precision textures, Screen shader, Sound gain(사용자 요청).
* **probe**: letterbox probe에 늘림 사각형(`glide_letterbox_stretch`)을 더하고 core probe에도 넣었다.
  런처 probe에 두 키의 왕복·희소 저장·잘못된 값 경고(3→5개)·게시·한쪽만 호출자가 정한 경우.
* **스크립트**: `scripts/survey_romsets.sh`가 `REPIU_GLIDE_FULLSCREEN=0`을 기본으로 둔다.
* **문서**: 설계·작업 지시, ARCHITECTURE(letterbox, OSD, 런처), README.

## 검증

이 환경은 Linux(Ubuntu 26.04.1, RTX 4090, 실행은 x11)라 Win32는 빌드하지 못했다.

| 검증 | 결과 |
|---|---|
| Linux x64·i386 Release 빌드(모든 기본 타깃) | 통과, 새 경고 없음 |
| core probe | 두 아키텍처 모두 `glide_letterbox_all=true`(stretch 포함), `launcher_all=true`, 실패 0 |
| x64 pumpitea, OSD 조작(사용자) | 전체 화면 8회(켬 4·끔 4), 비율 유지 6회 전환, 저장 14회(한 번에 하나씩). 마지막 상태 `fullscreen=0 keep_aspect=1`이 파일에 남음. 창을 닫을 때 저장 없음 |
| x64 pumpitea, 저장값 `fullscreen=1 keep_aspect=0`으로 시작 | 시작하자마자 전체 화면·늘림, 시작 시 저장 없음. 변경 없이 닫은 뒤 파일 그대로(전체 화면 해제가 저장되지 않음) |
| 런처 → pumpitpx | 런처에서 비율 유지를 켠 값이 시작 전에 저장되고, 게임이 전체 화면·비율 유지로 시작 |
| 런처 창 | 저장값대로 전체 화면으로 열림, 체크박스로 끈 `fullscreen=0`이 저장되고 pumpitpru 시작 |
| 화면 확인 | 사용자가 OSD·재실행·런처·Options 순서 모두 정상이라고 확인(2026-10-10) |
| Win32(PR #46 CI) | Debug 빌드(모든 타깃, 기존 `NOMINMAX` 경고만), `repiu_glide_issue_probe`, `repiu_aot_probe --timer-safe-point` 통과 |

## 확인하지 않은 것

* Win32의 core·런처 probe와 실행(빌드는 CI가 확인).
* i386 빌드의 게임 실행(빌드와 core probe만). 바뀐 코드는 아키텍처 공용이다.
* 런처를 거친 게임에서 OSD로 바꾼 값이 돌아온 런처 화면에 반영되는지(인자 실행에서 OSD 저장은 확인함,
  런처 루프는 게임이 끝나면 파일을 다시 읽는다).
* Wayland 드라이버에서의 전체 화면 전환(실행은 모두 x11).

---

# Work log: OSD fullscreen and keep-aspect options stored in `cfg/repiu.ini` (issue #45)

**Done.** `ComputeGlidePictureRect(..., keep_aspect)` returns Task 769's letterbox rectangle when on and
the whole drawable when off, and the backend's `ApplyDrawableViewport` uses it. A session sink
(`repiu/engine/session_display_preferences`) carries a change out of the engine, which knows no launcher
types and calls it only when a user action changed a value. The backend reads `keep_aspect_` from
`REPIU_GLIDE_KEEP_ASPECT` (on by default) and enters fullscreen at start when `REPIU_GLIDE_FULLSCREEN`
asks (off by default; after the scaled window exists). `ToggleFullscreen` became `IsFullscreen` and
`SetFullscreen(fullscreen, report)`, and an OSD request is applied by `ApplyPendingDisplayOptions` at the
next event pump, which reports both values once; the reported value is the requested one, not the flag
read back, since SDL's switch is asynchronous. The OSD shows "Fullscreen" and "Keep aspect ratio" first
below the info lines (`GlideOsdDisplayOptions`). The launcher settings read (warning on anything but
0/1), write and publish `[Video] fullscreen` and `keep_aspect` under the caller-wins rule, with
`SettingsDiffer` extended and the 0/1 reading folded into `LoadVideoSwitch`, which
`texture_full_precision` now uses too. The loader logs both in its publish line and registers the sink
before the game runs (`StoreSessionDisplayPreferences` re-reads the file and changes only the two keys).
**Launcher window (added request):** it opens fullscreen when stored, switches at once from its checkbox
or `Alt+Enter` (kept from ImGui), and with keep-aspect on draws in a centred 960×640-ratio area with
black around it; Options now read Fullscreen, Keep aspect ratio, vsync, Full-precision textures, Screen
shader, Sound gain (the user's order). The letterbox probe gains a stretch case
(`glide_letterbox_stretch`) and joins the core probe; the launcher probe covers both keys' round trip,
sparse saving, malformed-value warnings (3 → 5), publishing, and a caller holding back only one.
`scripts/survey_romsets.sh` defaults `REPIU_GLIDE_FULLSCREEN=0`. Docs: design, work order, ARCHITECTURE
(letterbox, OSD, launcher), README.

**Verified** on Linux (Ubuntu 26.04.1, RTX 4090, running on x11); Win32 cannot be built here. Linux x64
and i386 Release builds of every default target pass with no new warnings, and the core probe reports
`glide_letterbox_all=true` (stretch included), `launcher_all=true` and no failures on both. In an x64
pumpitea run the user switched fullscreen 8 times (4 on, 4 off) and keep-aspect 6 times, giving 14
stores, one per change; the last state, `fullscreen=0 keep_aspect=1`, stayed in the file and closing the
window stored nothing. Started with `fullscreen=1 keep_aspect=0` stored, the game came up fullscreen and
stretched without storing anything, and closing it unchanged left the file as it was (leaving fullscreen
on close was not stored). Through the launcher, keep-aspect turned on there was stored before pumpitpx
started fullscreen with the aspect kept; the launcher window opened fullscreen as stored, and turning it
off there stored `fullscreen=0` before pumpitpru started. The user confirmed the OSD, the rerun, the
launcher and the Options order on screen (2026-10-10). The Win32 CI of PR #46 built every target in
Debug (only the existing `NOMINMAX` warnings) and passed `repiu_glide_issue_probe` and
`repiu_aot_probe --timer-safe-point`.

**Not checked.** Win32's core and launcher probes and a Win32 run (CI checked the build); a game run on the i386 build (build and core probe only; the
changed code is architecture-neutral); whether an OSD change in a game started from the launcher shows in
the launcher it returns to (OSD storing was checked in an argument run, and the launcher loop re-reads
the file when the game ends); fullscreen switching under the Wayland driver (all runs used x11).
