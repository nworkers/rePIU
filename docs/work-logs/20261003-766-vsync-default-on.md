# 작업 로그 766: vsync 기본값 켜기

설계: [20261003-766-vsync-default-on.md](../design/20261003-766-vsync-default-on.md) ·
작업 지시: [20261003-766-vsync-default-on.md](../work-orders/20261003-766-vsync-default-on.md)

## 배경

Linux x64 빌드를 Ubuntu GNOME(Wayland)에서 실행하면 vsync가 꺼진 채로 돌았다. 엔진은
`REPIU_GLIDE_SWAP_INTERVAL`이 없으면 swap interval을 요청하지 않았고(Task 371), SDL의 Wayland
backend는 요청이 없으면 EGL interval 0으로 그린다. WSL(X11/GLX)에서는 드라이버 기본값 덕분에
켜져 있었다.

## 수행 내용

* `glide_swap_interval_policy`: `kDefaultGlideSwapInterval = 1`,
  `ResolveGlideSwapInterval(const char*, std::int32_t*)`(순수 함수, 항상 값을 채우고 override
  여부를 반환), `ReadGlideSwapInterval`. `TryReadGlideSwapIntervalOverride`는 제거했다.
  스냅샷에 `interval_requested`를 추가했다(GL 창이 있으면 true).
* `glide_opengl_backend.cpp`: 조건 분기를 없애고 interval을 항상 요청한다. Task 745의 거부 대응은
  그대로 적용된다.
* `main.cpp`: 페이싱·대기 tick·시계 보고 줄의 조건을 `override_requested`에서
  `interval_requested`로 바꿨다.
* `launcher_ui.cpp`: 저장된 `swap_interval`이 없으면 "Wait for vertical sync"가 체크된 상태로 보인다.
* probe: 기본값 해석 검사(`glide_swap_interval_default`)를 추가했다.
* `README.md`, `ARCHITECTURE.md`에 기본값과 끄는 방법을 적었다.

## 검증

* `cmake --build build/linux_x64` 성공. 새 경고 없음(`execution_trampoline.cpp:158` 경고는 기존 것).
* `repiu_aot_probe`는 Win32 전용 타깃이라 Linux에서는 probe 소스와 정책 소스만 따로 컴파일해
  실행했다. `glide_swap_interval_all=true`.
* Wayland(GNOME)에서 `./build/linux_x64/repiu pumpit1`, `REPIU_EXECUTION_TIMEOUT_MS=20000`:

| 조건 | override requested/value/applied/effective | frames / span_ms |
|---|---|---|
| 환경 변수 없음 | `false/1/true/1` | 629 / 15,240 |
| `REPIU_GLIDE_SWAP_INTERVAL=0` | `true/0/true/0` | 10,696 / 15,383 |

  기본값에서는 interval 1이 적용됐고 프레임이 디스플레이에 묶였다. `=0`이면 이전과 같이 상한이
  없다. 기본값 실행의 평균(약 41 fps)이 60에 못 미치는 것은 시작 구간의 자산 로딩이 span에
  포함되기 때문으로 보인다. 이 작업에서는 확인하지 않았다.
* Win32 빌드와 런처 UI는 이 환경에서 실행하지 않았다.

---

# Work Log 766: Vertical sync on by default

Design: [20261003-766-vsync-default-on.md](../design/20261003-766-vsync-default-on.md) ·
Work order: [20261003-766-vsync-default-on.md](../work-orders/20261003-766-vsync-default-on.md)

## Background

The Linux x64 build ran with vsync off under Ubuntu GNOME (Wayland). Without
`REPIU_GLIDE_SWAP_INTERVAL` the engine never requested an interval (Task 371), and SDL's Wayland
backend draws at EGL interval 0 unless asked. Under WSL (X11/GLX) the driver default had kept it on.

## Changes

* `glide_swap_interval_policy`: `kDefaultGlideSwapInterval = 1`,
  `ResolveGlideSwapInterval(const char*, std::int32_t*)` (pure; always fills the value and returns
  whether it was an override) and `ReadGlideSwapInterval`; `TryReadGlideSwapIntervalOverride` is
  gone. The snapshot gains `interval_requested` (true once a GL window exists).
* `glide_opengl_backend.cpp`: the branch is gone and the interval is always requested; Task 745's
  refusal handling applies unchanged.
* `main.cpp`: the pacing, wait-tick and clock report lines key on `interval_requested` rather than
  `override_requested`.
* `launcher_ui.cpp`: with no stored `swap_interval`, "Wait for vertical sync" reads as checked.
* Probe: a default-resolution check (`glide_swap_interval_default`).
* `README.md` and `ARCHITECTURE.md` record the default and how to turn it off.

## Verification

* `cmake --build build/linux_x64` succeeds with no new warnings (the one at
  `execution_trampoline.cpp:158` predates this task).
* `repiu_aot_probe` is a Win32-only target, so on Linux the probe source was compiled alone with
  the policy source and run: `glide_swap_interval_all=true`.
* Under Wayland (GNOME), `./build/linux_x64/repiu pumpit1` with `REPIU_EXECUTION_TIMEOUT_MS=20000`:

| Condition | override requested/value/applied/effective | frames / span_ms |
|---|---|---|
| no variable | `false/1/true/1` | 629 / 15,240 |
| `REPIU_GLIDE_SWAP_INTERVAL=0` | `true/0/true/0` | 10,696 / 15,383 |

  The default applied interval 1 and the frames were bound to the display; `=0` is uncapped as
  before. The default run's average (about 41 fps) falls short of 60, most likely because the
  start-up asset loading sits inside the span; this task did not confirm that.
* The Win32 build and the launcher UI were not run in this environment.
