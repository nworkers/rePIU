# #5 작업 로그: 인게임 OSD에 OpenGL renderer 표시

Issue: [#5](https://github.com/reexec/rePIU/issues/5) · 설계: [20261005-i005](../design/20261005-i005-osd-gl-renderer.md) ·
작업 지시: [20261005-i005](../work-orders/20261005-i005-osd-gl-renderer.md)

## 요약

`Tab` OSD 맨 위에 Renderer 절이 생겼습니다. renderer, vendor, GL 버전, SDL 비디오 드라이버를 보여 주고, 소프트웨어 렌더러면
빨간 글씨와 "Software rendering: no 3D acceleration"으로 경고합니다. OSD와 backend는 공용 코드라 Win32와 Linux에서 같은 코드로
동작하며, 두 호스트에서 화면으로 확인했습니다.

issue #5는 2026-10-05 Linux 세션에서 등록만 되고 구현되지 않은 채 남아 있었습니다(브랜치·PR 모두 없음). 이 작업이 그 구현입니다.

## 바꾼 것

* `include/repiu/engine/gl_renderer_identity.h`, `src/engine/gl_renderer_identity.cpp`(신규): `GlRendererIdentity`,
  `IsSoftwareGlRenderer`, `MakeGlRendererIdentity`. GL·SDL 헤더 없음.
* `src/engine/glide_opengl_backend.cpp`: 기존 `GL renderer:` 로그 옆에서 vendor·version·`SDL_GetCurrentVideoDriver()`를 읽어
  구조체를 만들고 `GL vendor/version/video driver/software: …` 한 줄을 더 찍음. OSD 초기화가 성공하면 `SetRendererIdentity`.
  기존 로그 줄은 그대로.
* `GlideOsd`: `SetRendererIdentity`, OSD 맨 위 `DrawRendererSection`.
* probe `gl_renderer_identity`(core probe, Win32 AOT probe `--gl-renderer-identity`): 소프트웨어 이름 7개는 true, GPU 이름 4개와
  `unknown`·빈 문자열은 false, 빈 값·null은 `unknown`.
* `ARCHITECTURE.md`의 `GlideOsd` 항목, README의 OSD 설명.

## 검증

| 검증 | 결과 |
|---|---|
| Win32 Release 빌드 | exit 0, 바뀐 파일에 새 경고 없음 |
| Win32 core probe | `gl_renderer_identity_all=true`. `core_probe_all=false`는 기존 [#8](https://github.com/reexec/rePIU/issues/8)(`stack_bridge_contract`) 하나 |
| Win32 AOT probe `--gl-renderer-identity` | exit 0 |
| Win32 pumpit1 22초, OSD 열고 `PrintWindow` 캡처 | Renderer 절: `NVIDIA GeForce RTX 4090/PCIe/SSE2`, `Vendor: NVIDIA Corporation`, `OpenGL: 4.6.0 NVIDIA 616.56`, `Video driver: windows`. 기존 LFB·shader 항목 그대로. 1,023프레임, 실패 0 |
| Linux x64 Debug(WSL) 빌드 | exit 0 |
| Linux x64 core probe | `gl_renderer_identity_all=true`, `core_probe_all=true` |
| Linux x64 pumpit1, `GALLIUM_DRIVER=llvmpipe`, `SDL_VIDEO_DRIVER=x11`, 입력 스크립트로 6초에 Tab | 로그 `software: true`. OSD에 `llvmpipe (LLVM 20.1.2, 256 bits)`와 경고가 빨간색, `Video driver: x11`. 2,585프레임, 실패 0 |

* WSL 캡처는 Windows 쪽에서 WSLg 창(`msrdc`)을 `CopyFromScreen`으로 찍었습니다. 처음에는 창 제목 검색이 제목에 "rePIU"가 든 VS Code
  창에 걸려 잘못 찍었고, 그 캡처는 지웠습니다.
* Linux i386과 Wayland는 실행하지 않았습니다. 같은 코드이고, 드라이버 이름은 SDL이 돌려주는 값을 그대로 씁니다.

## 남은 것

* 소프트웨어 판정은 이름 목록입니다. 목록에 없는 소프트웨어 렌더러는 경고 없이 이름만 보입니다. 새 이름을 보면 probe와 함께 더합니다.

---

# #5 Work Log: The OpenGL Renderer in the In-Game OSD

Issue: [#5](https://github.com/reexec/rePIU/issues/5) · Design: [20261005-i005](../design/20261005-i005-osd-gl-renderer.md) ·
Work order: [20261005-i005](../work-orders/20261005-i005-osd-gl-renderer.md)

## Summary

The `Tab` OSD now opens with a Renderer section: the renderer, vendor, GL version and SDL video driver, with a software
renderer shown in red and "Software rendering: no 3D acceleration". The OSD and the backend are shared code, so Win32 and
Linux run the same code; both were checked on screen.

Issue #5 was filed in the Linux session on 2026-10-05 and left unimplemented (no branch, no pull request). This task is the
implementation.

## Changes

* `include/repiu/engine/gl_renderer_identity.h`, `src/engine/gl_renderer_identity.cpp` (new): `GlRendererIdentity`,
  `IsSoftwareGlRenderer`, `MakeGlRendererIdentity`. No GL or SDL header.
* `src/engine/glide_opengl_backend.cpp`: next to the existing `GL renderer:` log line, reads the vendor, the version and
  `SDL_GetCurrentVideoDriver()`, builds the structure, and logs one more line `GL vendor/version/video driver/software: …`.
  When the OSD initialises, `SetRendererIdentity`. The existing log line is unchanged.
* `GlideOsd`: `SetRendererIdentity`, and `DrawRendererSection` at the top of the overlay.
* The `gl_renderer_identity` probe (core probe, and `--gl-renderer-identity` in the Win32 AOT probe): seven software names
  are true; four GPU names, `unknown` and the empty string are false; empty and null values read `unknown`.
* The `GlideOsd` entry of `ARCHITECTURE.md`, and the OSD description in the README.

## Verification

| Check | Result |
|---|---|
| Win32 Release build | exit 0, no new warnings in the changed files |
| Win32 core probe | `gl_renderer_identity_all=true`. `core_probe_all=false` comes only from the existing [#8](https://github.com/reexec/rePIU/issues/8) (`stack_bridge_contract`) |
| Win32 AOT probe `--gl-renderer-identity` | exit 0 |
| Win32 pumpit1 22 s, OSD opened and captured with `PrintWindow` | Renderer section: `NVIDIA GeForce RTX 4090/PCIe/SSE2`, `Vendor: NVIDIA Corporation`, `OpenGL: 4.6.0 NVIDIA 616.56`, `Video driver: windows`. The LFB and shader controls unchanged. 1,023 frames, no failure |
| Linux x64 Debug (WSL) build | exit 0 |
| Linux x64 core probe | `gl_renderer_identity_all=true`, `core_probe_all=true` |
| Linux x64 pumpit1, `GALLIUM_DRIVER=llvmpipe`, `SDL_VIDEO_DRIVER=x11`, Tab at 6 s from an input script | log `software: true`. The OSD shows `llvmpipe (LLVM 20.1.2, 256 bits)` and the warning in red, `Video driver: x11`. 2,585 frames, no failure |

* The WSL capture took the WSLg window (`msrdc`) from the Windows side with `CopyFromScreen`. At first the title search
  matched the VS Code window, whose title contains "rePIU", and captured the wrong window; that capture was deleted.
* Linux i386 and Wayland were not run. The code is the same, and the driver name is whatever SDL returns.

## Left

* The software check is a name list. A software renderer not on the list shows its name without the warning; new names go
  into the list together with the probe.
