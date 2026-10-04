# Task 769 작업 로그: 전체화면 전환과 화면 비율 유지

설계: [20261004-769](../design/20261004-769-fullscreen-toggle-and-aspect.md) ·
작업 지시: [20261004-769](../work-orders/20261004-769-fullscreen-toggle-and-aspect.md)

## 요약

Glide 게임 창에서 더블클릭 또는 `Alt+Enter`로 테두리 없는 전체화면(디스플레이 모드 변경 없음)과
창 모드를 오가게 했습니다. 창 크기 조절과 전체화면 모두에서 게임 화면은 논리 해상도 비율을
유지하고, 남는 부분은 매 present마다 검은 띠로 지웁니다. 원인 조사 결과, 전에는 전체화면 코드가
아예 없었고, viewport가 drawable 전체라 창을 늘리면 그림도 비율 없이 늘어났습니다.

## 바꾼 것

* `glide_letterbox.h/.cpp`: 순수 함수 `ComputeGlideLetterboxRect`(정수 교차곱으로 축 결정,
  반올림, 홀수 픽셀은 오른쪽·위).
* `glide_opengl_backend`: `content_rect_`, `ApplyDrawableViewport`가 viewport·scissor·point size를
  rect로, `ToggleFullscreen`, `ClearLetterboxBars`(띠가 없으면 즉시 반환), `Alt+Enter`·더블클릭
  처리(OSD가 마우스를 쓰는 중이면 더블클릭 무시), 전체화면 중 `Alt+1~4` 무시, `ReadbackFramebuffer`와
  픽셀 진단이 rect만 읽음, `Close`에서 rect 초기화.
* `glide_osd`: `WantsMouse()`.
* `glide_post_process`: `Apply`가 rect를 받아 그 영역에서 복사하고 그 영역에 그림(Task 768 pass가
  띠를 처리하지 않게).
* probe: `glide_letterbox_probe`(`--glide-letterbox`), `glide_render_probe --opengl-post-shader`에
  pillarbox viewport·readback 검사와 전체화면 왕복 검사 추가.
* 문서: 설계, 작업 지시, `ARCHITECTURE.md` resize·readback 문단, README 조작법.

## 검증

* Win32 x86 Debug 전체 빌드 exit 0, 새 경고 없음.
* `repiu_aot_probe --glide-letterbox`: `same_ratio/pillarbox/letterbox/odd/degenerate/all` 모두
  `true` (1920×1080 → x=240, 1440×1080 포함).
* `repiu_aot_probe --post-shader`, `--launcher`: 모두 `all=true`.
* `repiu_glide_render_probe --opengl-post-shader` (Intel Arc 130T):
  * 창을 256×96으로 넓히면 viewport `64,0,128,96`, 전체를 검게 지운 뒤에도 readback이 전부 흰색
    → `glide_letterbox_gl=true` (게스트가 읽는 값에 띠가 섞이지 않음).
  * 합성한 `Alt+Enter` 이벤트로 전체화면 진입: 1920×1200 데스크톱, **디스플레이 모드 그대로**,
    viewport `160,0,1600,1200`(4:3). 합성한 더블클릭으로 창 모드 복귀.
    `glide_fullscreen_toggle=true`. 이벤트는 backend의 실제 `PumpEvents`를 지났습니다.
  * Task 768 검사(주사선 48/48/96, crt, 상태 복원)도 계속 통과. `--opengl-lfb` pass.
* pumpit1 Debug 15초 실행: `none` 413·411·406프레임, `crt` 395프레임, 모두 exit 0, 정상 teardown.
  Task 768 때(458)보다 낮지만 이번 측정 중 CPU 부하가 42%였고, 프레임 수가 실행 길이와 비례하지
  않습니다(8.6초 406, 10.7초 411). 그래서 속도 비교 지표로 쓸 수 없습니다. 코드상 기본 창(비율이 맞음)에서는
  `ClearLetterboxBars`가 GL 호출 없이 반환하므로 프레임당 추가 GL 작업은 없습니다. 같은 조건의 A/B는
  하지 않았습니다.

## WSL 검증 (Task 768·769 함께)

처음 기록한 "WSL에 cmake·g++ 없음"은 틀렸습니다. WSL Ubuntu-24.04에는 cmake 3.28과 g++가 있습니다.

* `build/linux_x64_debug` 전체 빌드(`scripts/build_linux_x64.sh --config Debug`) exit 0. 새로 쓰거나
  고친 파일에서 경고가 없고, 경고 두 개는 기존 파일(`execution_trampoline.cpp`, `fault_handler_arch.cpp`)에서
  나왔습니다.
* `repiu_aot_probe`와 `repiu_glide_render_probe`는 CMake `if(WIN32)` 안에 있어 Linux에서는 만들어지지
  않습니다. 저장소를 바꾸지 않고 확인하려고, 세 probe 소스와 작은 driver, 그리고 `_WIN32` 가드만 푼
  render probe 사본을 `librepiu_exe.a`에 링크해 임시로 빌드했습니다(`-Wall -Wextra` 경고 없음).
* `--glide-letterbox`, `--post-shader`, `--launcher`: 모두 `all=true`.
* `--opengl-lfb`: x11·Wayland(d3d12) pass.
* `--opengl-post-shader`: x11·Wayland × d3d12(RTX 4090)·llvmpipe 네 조합 모두 주사선 48/48/96,
  crt, none, 상태 복원, 전체화면 진입(3840×2160, 모드 유지, viewport `480,0,2880,2160`)은 통과했습니다.
  **`glide_letterbox_gl`은 네 조합 모두 false**였습니다.
  * 원인은 probe입니다. 창을 256×96으로 넓힌 직후 SDL은 drawable 256×96을 보고하지만, Mesa(GLX·EGL)는
    다음 swap에서야 back buffer 크기를 바꿉니다. 그래서 viewport `64,0,128,96`의 왼쪽 절반(x 64..127)만
    그려지고, 128 이후는 버퍼 밖이라 0으로 읽혔습니다. WGL은 바로 바꾸므로 Windows에서는 통과합니다.
  * 사본에서 resize 뒤 `SDL_GL_SwapWindow` 한 번을 넣으면 네 조합 모두 흰색 3072/3072,
    `glide_letterbox_gl=true`입니다. 게임은 매 프레임 swap하므로 엔진 결함은 아닙니다. 다만 Linux에서는
    창 크기를 바꾼 직후 한 프레임이 옛 크기 버퍼에 새 viewport로 그려질 수 있습니다.
* **WSLg Wayland에서는 전체화면 해제가 느리고 일정하지 않습니다.** 위 swap을 넣은 사본에서 더블클릭 뒤
  `SDL_SyncWindow`가 돌아와도 플래그가 남았습니다. swap을 반복하며 기다리면 13·43·50·368번 만에 풀렸고,
  다섯 번 중 한 번은 600번 안에 풀리지 않았습니다. backend는 `SDL_SetWindowFullscreen(false)`를 정상으로
  호출했고(`windowed mode` 로그), 버퍼를 한 번도 commit하지 않은 창에서는 바로 풀렸습니다. WSLg
  compositor의 응답 문제로 보이며, GNOME 같은 실제 Wayland에서는 확인하지 않았습니다.
* 실제 게임: pumpit1 Debug, `SDL_VIDEO_DRIVER=x11`, `REPIU_POST_SHADER=crt`, 30초. XTest로 실제 X 입력을
  보냈습니다. `Alt+Enter` → 3840×2160 at 0,0, 더블클릭 → 1280×960 at 1286,627, `Alt+Enter` 두 번 왕복도
  같았습니다. 전체화면 스크린샷에서 crt 주사선이 적용된 ANDAMIRO 로고가 화면 가운데에 있었습니다(배경이
  검정이라 띠 경계는 눈으로 구분할 수 없음). 566프레임, GL 오류 0, `reason=timeout`. exit 3은 Linux x64가
  시간 제한에서 늘 거치는 기존 `immediate-exit` 경로입니다.
* Wayland 드라이버로 실제 게임 전환은 하지 못했습니다. XTest가 Wayland 창에 닿지 않고, 엔진 입력
  스크립트는 수정키를 보내지 못합니다.

## 하지 않은 것

* **사람 손으로 하는 확인:** 실제 마우스 더블클릭과 키보드 `Alt+Enter`, OSD 슬라이더 위 더블클릭이
  전환을 일으키지 않는지는 눈으로 보지 않았습니다(이벤트 합성으로 같은 경로를 확인). Linux x11에서는
  XTest로 실제 X 입력 경로를 확인했습니다(위 절).
* **Linux i386 빌드**와 실제 Wayland(GNOME 등) 확인.
* 전체화면에서 커서 숨기기, 전체화면으로 시작하는 설정은 요청 범위 밖이라 넣지 않았습니다.

---

# Task 769 Work Log: Fullscreen Toggle and Aspect-Preserving Scaling

Design: [20261004-769](../design/20261004-769-fullscreen-toggle-and-aspect.md) ·
Work order: [20261004-769](../work-orders/20261004-769-fullscreen-toggle-and-aspect.md)

## Summary

A double click or `Alt+Enter` on the Glide game window now switches between borderless fullscreen
(no display-mode change) and windowed mode. Through window resizes and fullscreen alike, the picture
keeps the logical resolution's ratio, with the leftover cleared to black bars at every present. The
investigation found no fullscreen code at all before, and a whole-drawable viewport that stretched the
picture with the window.

## Changes

`ComputeGlideLetterboxRect` (pure; integer cross-multiplication picks the binding axis, rounds to
nearest, odd pixel right and top); the backend's `content_rect_`, viewport/scissor/point size on that
rect, `ToggleFullscreen`, `ClearLetterboxBars` (returns at once when there are no bars), `Alt+Enter`
and double-click handling (a double click is ignored while the OSD wants the mouse), `Alt+1..4` ignored
while fullscreen, readback and pixel diagnostics reading the rect only, and the rect reset in `Close`;
`GlideOsd::WantsMouse()`; `GlidePostProcess::Apply` taking the rect so the Task 768 pass never touches
the bars; the `glide_letterbox_probe` (`--glide-letterbox`) and pillarbox and fullscreen round-trip
checks in `glide_render_probe --opengl-post-shader`; the design, work order, `ARCHITECTURE.md` resize and
readback paragraphs, and README controls.

## Verification

* Full Win32 x86 Debug build, exit 0, no new warnings.
* `repiu_aot_probe --glide-letterbox`: all `true`, including 1920×1080 → 1440×1080 at x=240.
  `--post-shader` and `--launcher` still `all=true`.
* `repiu_glide_render_probe --opengl-post-shader` (Intel Arc 130T): widened to 256×96, the viewport is
  `64,0,128,96` and, with the whole drawable cleared black first, the readback is all white
  (`glide_letterbox_gl=true`), so the bars never reach what the guest reads. A synthesized `Alt+Enter`
  entered fullscreen at the 1920×1200 desktop **with the display mode unchanged** and a 4:3 viewport of
  `160,0,1600,1200`; a synthesized double click returned to windowed mode (`glide_fullscreen_toggle=true`),
  both through the backend's real `PumpEvents`. The Task 768 checks and `--opengl-lfb` still pass.
* pumpit1 Debug, 15 s: `none` 413, 411 and 406 frames, `crt` 395, all exit 0 with a normal teardown.
  Lower than Task 768's 458, but CPU load was 42% during these runs and the frame count does not track
  the run's length (406 in 8.6 s, 411 in 10.7 s), so it is no speed comparison. By the code, a default
  window of the right ratio makes `ClearLetterboxBars` return without a GL call, so there is no added
  per-frame GL work; a controlled A/B was not run.

## WSL verification (Tasks 768 and 769 together)

The earlier note that WSL had no cmake or g++ was wrong: WSL Ubuntu-24.04 has cmake 3.28 and g++.

* Full `build/linux_x64_debug` build (`scripts/build_linux_x64.sh --config Debug`), exit 0. No warnings
  from new or changed files; the two warnings come from existing files (`execution_trampoline.cpp`,
  `fault_handler_arch.cpp`).
* `repiu_aot_probe` and `repiu_glide_render_probe` sit inside CMake's `if(WIN32)` and are not built on
  Linux. To check without touching the repository, the three probe sources with a small driver, and a
  copy of the render probe with only its `_WIN32` guards lifted, were linked against `librepiu_exe.a`
  ad hoc (no `-Wall -Wextra` warnings).
* `--glide-letterbox`, `--post-shader`, `--launcher`: all `all=true`.
* `--opengl-lfb`: pass under x11 and Wayland (d3d12).
* `--opengl-post-shader`: across x11 and Wayland × d3d12 (RTX 4090) and llvmpipe, scanlines 48/48/96,
  crt, none, state restore and fullscreen entry (3840×2160, mode kept, viewport `480,0,2880,2160`) pass;
  **`glide_letterbox_gl` was false in all four**.
  * The probe is the cause. Right after widening the window to 256×96, SDL reports a 256×96 drawable, but
    Mesa (GLX and EGL) resizes the back buffer only at the next swap, so only the left half (x 64..127) of
    viewport `64,0,128,96` was drawn and x ≥ 128 read back as zero, outside the buffer. WGL resizes at
    once, so Windows passes.
  * With one `SDL_GL_SwapWindow` after the resize in the copy, all four read white 3072/3072 and
    `glide_letterbox_gl=true`. The game swaps every frame, so this is no engine defect; on Linux, though, the
    one frame right after a resize can be drawn with the new viewport into the old-size buffer.
* **Leaving fullscreen on WSLg Wayland is slow and erratic.** In the copy with that swap, the flag was
  still set after the double click once `SDL_SyncWindow` returned; swapping while waiting cleared it
  after 13, 43, 50 and 368 swaps, and in one of five runs not within 600. The backend did call
  `SDL_SetWindowFullscreen(false)` (`windowed mode` logged), and a window that had never committed a
  buffer left at once. This looks like the WSLg compositor; a real Wayland desktop (GNOME etc.) was not
  checked.
* Real game: pumpit1 Debug, `SDL_VIDEO_DRIVER=x11`, `REPIU_POST_SHADER=crt`, 30 s, with real X input
  through XTest. `Alt+Enter` → 3840×2160 at 0,0; double click → 1280×960 at 1286,627; two more
  `Alt+Enter` round-tripped the same. A fullscreen screenshot showed the ANDAMIRO logo centred with the
  crt scanlines applied (the background is black, so the bar edges cannot be seen). 566 frames, no GL error,
  `reason=timeout`; exit 3 is the existing `immediate-exit` path that Linux x64 always takes at the time limit.
* Toggling in the real game under the Wayland driver was not done: XTest cannot reach a Wayland window and
  the engine's input script cannot send modifiers.

## Not done

* **By hand:** a real mouse double click, a real `Alt+Enter`, and a double click on an OSD slider not
  toggling were not watched (synthesized events covered the same path). Under Linux x11 the real X input
  path was checked with XTest (section above).
* **The Linux i386 build** and a real Wayland desktop (GNOME etc.).
* Hiding the cursor in fullscreen and a start-in-fullscreen setting were out of scope.
