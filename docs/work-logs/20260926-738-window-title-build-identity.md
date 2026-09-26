# Task 738 작업 로그 — 창 제목에 플랫폼·아키텍처·빌드 구성 표시

설계: [20260926-738](../design/20260926-738-window-title-build-identity.md)
작업 지시: [20260926-738](../work-orders/20260926-738-window-title-build-identity.md)

## 결과

창 제목이 버전 뒤에 빌드 identity를 붙입니다.

| host | 실행 중 실제 창 제목 |
|---|---|
| Win32 x86 Debug | `rePIU v0.0.190 (Win/x86 Debug) - Build Sep 26 2026 - Glide 2 OpenGL [dynamic] - FPS : …` |
| WSL Linux x64 Debug | `rePIU v0.0.190 (Linux/x64 Debug) - Build Sep 26 2026 - Glide 2 OpenGL [dynamic] - FPS : …` |

* `include/repiu/platform/build_identity.h`, `src/platform/build_identity.cpp`: 플랫폼·아키텍처는
  컴파일러 predefined 매크로로, 빌드 구성은 CMake가 넣는 `REPIU_BUILD_CONFIG="$<CONFIG>"`로 정합니다
  (빈 문자열이면 `NDEBUG`). 매크로 판정은 이 파일에만 있습니다.
* `GlideOpenGlBackend::BuildWindowTitle`은 `(<label>)`만 끼워 넣고 나머지는 그대로입니다. 요청 예시에는
  버전이 없었지만 "여기에 추가하자"였으므로 버전을 유지했습니다. 빼기를 원하시면 한 줄입니다.
* core probe `build_identity`가 라벨을 probe 자신의 매크로·포인터 폭·`NDEBUG`와 대조합니다.
  `repiu_aot_probe --build-identity`로도 실행됩니다.

## 검증

| 검증 | 결과 |
|---|---|
| Win32 x86 Debug 전체 빌드, core probe | 31/31, `build_identity=true,label="Win/x86 Debug"` |
| WSL Linux x64 Debug 빌드, core probe | 33/33, `label="Linux/x64 Debug"` |
| 실행 중 창 제목 | Win32는 `MainWindowTitle`, Linux는 X11 `XFetchName`으로 12초·10초 시점에 위 표대로 확인 |

Release·Web 빌드는 하지 않았습니다. Release 라벨은 `$<CONFIG>`에서, Web은 `__EMSCRIPTEN__`/`__wasm32__`에서
나오며 probe가 같은 규칙으로 검사합니다.


## 정정 (Task 739)

이 작업의 Linux x64 Release 빌드 때 "기존 트리가 headless로 구성돼 있었다"고 보고했습니다. 캐시에
`SDL_UNIX_CONSOLE_BUILD=ON`이 남아 있던 것은 맞지만, 그 스위치는 X11/Wayland 개발 패키지가 없을 때
configure의 오류를 막을 뿐이라 데스크톱 드라이버는 그대로 빌드됩니다. 같은 값이 남은
`build/linux_x64_debug`도 창을 띄웁니다. 그 트리에서 `repiu`가 없던 이유는 headless가 아니라 아직
빌드된 적이 없어서입니다. 자세한 것은 [Task 739](20260926-739-linux-build-scripts-manual-use.md)에
있습니다.

---

# English

# Task 738 work log — platform, architecture and build configuration in the window title

Design: [20260926-738](../design/20260926-738-window-title-build-identity.md)
Work order: [20260926-738](../work-orders/20260926-738-window-title-build-identity.md)

## Result

The window title now carries the build identity after the version: `rePIU v0.0.190 (Win/x86 Debug)
- Build Sep 26 2026 - Glide 2 OpenGL [dynamic] - FPS : …` on Win32 x86 Debug and `(Linux/x64 Debug)`
on WSL Linux x64 Debug, both read from the live window during a run.

* `include/repiu/platform/build_identity.h` and `src/platform/build_identity.cpp` decide the platform
  and architecture from the compiler's predefined macros and the configuration from CMake's
  `REPIU_BUILD_CONFIG="$<CONFIG>"` (falling back to `NDEBUG` when empty). The macro tests live only
  there.
* `GlideOpenGlBackend::BuildWindowTitle` inserts `(<label>)` and keeps everything else. The request's
  examples omitted the version, but it asked to *add* to the version, so the version stays; dropping
  it is a one-line change.
* The `build_identity` core probe checks the label against the probe's own macros, pointer width and
  `NDEBUG`; `repiu_aot_probe --build-identity` runs the same check.

## Verification

Win32 x86 Debug full build and core probe 31/31 (`label="Win/x86 Debug"`); WSL Linux x64 Debug build
and core probe 33/33 (`label="Linux/x64 Debug"`); live titles read at 12 s (Win32 `MainWindowTitle`)
and 10 s (Linux X11 `XFetchName`) as in the table. Release and Web builds were not made: the Release
label comes from `$<CONFIG>` and Web from `__EMSCRIPTEN__`/`__wasm32__`, and the probe checks both by
the same rule.

## Correction (Task 739)

When building the Linux x64 Release here, the earlier tree was reported as "configured headless".
Its cache did hold `SDL_UNIX_CONSOLE_BUILD=ON`, but that switch only suppresses the configure error
when the X11/Wayland development packages are missing; the desktop drivers are built regardless, and
`build/linux_x64_debug`, which holds the same value, opens windows. That tree had no `repiu` simply
because it had never been built. See [Task 739](20260926-739-linux-build-scripts-manual-use.md).
