# #15 작업 로그: re2DJ 형태의 OSD와 창 크기에 비례하는 UI 글자

Issue: [#15](https://github.com/nworkers/rePIU/issues/15) · 설계: [20261006-i015](../design/20261006-i015-osd-re2dj-layout.md) ·
작업 지시: [20261006-i015](../work-orders/20261006-i015-osd-re2dj-layout.md)

## 요약

`Tab` OSD를 re2DJ OSD와 같은 형태로 바꿨습니다. 화면 맨 위에 가로 전체 폭으로 붙고, 첫 줄에 `rePIU v0.0.204 (Win/x86 Release) -
Build Oct  6 2026`, 둘째 줄에 `Target Profile : pumpit1`이 나옵니다. 글자와 여백은 창 높이에 비례합니다(1배 0.75, 2배 1.5, 4K
전체화면 3.375). 인자 없이 실행했을 때의 런처도 크기 조절·최대화가 되고 글자가 함께 커집니다. 폰트는 re2DJ와 같은 ImGui 1.92.9의
ProggyForever입니다.

## 바꾼 것

* `include/repiu/engine/imgui_ui_scale.h`, `src/engine/imgui_ui_scale.cpp`(신규): `UiScaleRule`, `kOsdUiScale`(480·0.75·0.75),
  `kLauncherUiScale`(640·1·1), `UiScaleForHeight`, `ApplyImGuiUiScale`(기본 스타일에서 `ScaleAllSizes` + `FontScaleMain`),
  `AddImGuiUiFont`(`AddFontDefaultVector`).
* `include/repiu/engine/session_identity.h`, `src/engine/session_identity.cpp`(신규): `SetSessionTargetProfile`,
  `SessionTargetProfile`. loader가 `loader target:` 로그 직후 한 번 씀.
* `GlideOsd`: `SetInfoLines`, 기본 스타일 보관, 매 프레임 배율(바뀐 프레임에만 적용), (0,0) 가로 전체 폭·높이 자동·제목 표시줄
  없음.
* Glide backend: OSD 초기화 뒤 정보 두 줄(창 제목 앞부분과 같은 표기, Target Profile).
* 런처: `SDL_WINDOW_RESIZABLE`, 매 프레임 배율, 열 폭·버튼에 배율. 표 아래 공간은 고정 132 대신 직전 프레임에 잰 옵션·버튼
  높이.
* ImGui `v1.92.1` → v1.92.9(re2DJ와 같은 커밋 `01380c5`). `THIRD_PARTY_NOTICES.md` 갱신.
* probe `imgui_ui_scale`(core probe, Win32 AOT probe `--imgui-ui-scale`).
* `ARCHITECTURE.md`(`GlideOsd`, 런처), README.

## 경과

1. 처음 구현(ImGui 1.92.1, ProggyClean)으로 1배·2배·전체화면 OSD와 런처를 캡처해 배치와 배율을 확인했습니다.
2. 사용자 요청("폰트가 다르면 re2DJ에 맞춰 달라")으로 re2DJ를 보니 ImGui v1.92.9였고, 그 `AddFontDefault`는 15 px 이상에서
   ProggyForever를 고릅니다(re2DJ OSD는 보통 19.5 px 이상). ImGui를 같은 커밋으로 올리고 ProggyForever를 명시했습니다.
3. 런처 캡처에서 기본 크기에도 세로 스크롤바가 보였습니다. 고정 여백 132가 실제 아래쪽 높이보다 작았던 기존 문제라, 잰 높이를
   쓰도록 고쳤습니다.

## 검증

| 검증 | 결과 |
|---|---|
| Win32 Release 빌드(ImGui 1.92.9) | exit 0, 바뀐 파일에 새 경고 없음 |
| Win32 core probe | `imgui_ui_scale_all=true`. `core_probe_all=false`는 기존 [#8](https://github.com/nworkers/rePIU/issues/8) 하나 |
| Win32 AOT probe `--imgui-ui-scale` | true |
| Win32 pumpit1 OSD 캡처 2배(1280×960)·1배(640×480)·전체화면(3840×2160) | 세 크기 모두 맨 위 가로 전체 폭, 첫 두 줄 정상, 글자가 창에 비례, ProggyForever. 1,922프레임, 실패 0 |
| Win32 런처 캡처 기본(960×640)·최대화(3840×2054) | 글자·열·버튼이 함께 커짐, 두 크기 모두 스크롤바 없음 |
| Linux x64 Debug(WSL) 빌드(ImGui 1.92.9) | exit 0 |
| Linux x64 core probe | `imgui_ui_scale_all=true`, `gl_renderer_identity_all=true`, `core_probe_all=true` |

* Linux의 새 OSD 화면은 캡처하지 못했습니다. 창 검색이 제목에 "rePIU"가 든 OBS 창에 한 번, WSLg 창이 앞에 오지 않아 VS Code
  화면이 찍힌 것이 한 번이었고, 두 캡처 모두 열어 보지 않거나 바로 지웠습니다. OSD 코드는 공용이고, #5에서 같은 경로의 Linux
  OSD를 화면으로 확인했습니다.
* 전체화면 캡처 오른쪽 위의 FPS/GPU/CPU 글자는 NVIDIA 오버레이입니다.

## 남은 것

* 런처 기본 크기에서 긴 제목이 Status 열 경계까지 닿습니다(이전과 같음). 창을 키우면 풀립니다.

---

# #15 Work Log: An OSD in the re2DJ Layout, and UI Text That Scales with the Window

Issue: [#15](https://github.com/nworkers/rePIU/issues/15) · Design: [20261006-i015](../design/20261006-i015-osd-re2dj-layout.md) ·
Work order: [20261006-i015](../work-orders/20261006-i015-osd-re2dj-layout.md)

## Summary

The `Tab` OSD now has re2DJ's layout: across the full width at the top of the screen, opening with `rePIU v0.0.204 (Win/x86
Release) - Build Oct  6 2026` and `Target Profile : pumpit1`. Text and spacing follow the window height (0.75 at 1x, 1.5 at
2x, 3.375 in 4K fullscreen). The launcher shown without arguments can be resized and maximised, and its text grows with it.
The font is ProggyForever from ImGui 1.92.9, as in re2DJ.

## Changes

* `include/repiu/engine/imgui_ui_scale.h`, `src/engine/imgui_ui_scale.cpp` (new): `UiScaleRule`, `kOsdUiScale` (480, 0.75,
  0.75), `kLauncherUiScale` (640, 1, 1), `UiScaleForHeight`, `ApplyImGuiUiScale` (`ScaleAllSizes` plus `FontScaleMain` from
  the base style), `AddImGuiUiFont` (`AddFontDefaultVector`).
* `include/repiu/engine/session_identity.h`, `src/engine/session_identity.cpp` (new): `SetSessionTargetProfile` and
  `SessionTargetProfile`, written once by the loader right after its `loader target:` line.
* `GlideOsd`: `SetInfoLines`, the base style kept, the scale each frame (applied only when it changes), and a title-less
  window at (0,0), full width, height following the content.
* Glide backend: the two information lines after the OSD initialises (written as the start of the window title, and the
  Target Profile).
* Launcher: `SDL_WINDOW_RESIZABLE`, the scale each frame, column widths and buttons scaled. Below the table it leaves the
  height the options and buttons measured on the previous frame instead of a fixed 132.
* ImGui `v1.92.1` → v1.92.9 (the commit re2DJ uses, `01380c5`); `THIRD_PARTY_NOTICES.md` updated.
* The `imgui_ui_scale` probe (core probe, and `--imgui-ui-scale` in the Win32 AOT probe).
* `ARCHITECTURE.md` (`GlideOsd`, the launcher), the README.

## How it went

1. The first implementation (ImGui 1.92.1, ProggyClean) was captured at 1x, 2x and fullscreen, and in the launcher, to check
   the layout and the scale.
2. At the user's request ("if the font differs, match re2DJ's"), re2DJ turned out to use ImGui v1.92.9, whose
   `AddFontDefault` picks ProggyForever from 15 px (re2DJ's OSD is usually 19.5 px or more). ImGui moved to the same commit and
   ProggyForever is named outright.
3. The launcher captures showed a vertical scroll bar even at the default size. The fixed 132 below the table was less than
   the real height below it, an existing problem; it now uses the measured height.

## Verification

| Check | Result |
|---|---|
| Win32 Release build (ImGui 1.92.9) | exit 0, no new warnings in the changed files |
| Win32 core probe | `imgui_ui_scale_all=true`. `core_probe_all=false` comes only from the existing [#8](https://github.com/nworkers/rePIU/issues/8) |
| Win32 AOT probe `--imgui-ui-scale` | true |
| Win32 pumpit1 OSD captures at 2x (1280×960), 1x (640×480), fullscreen (3840×2160) | full width at the top at all three, the first two lines right, text proportional to the window, ProggyForever. 1,922 frames, no failure |
| Win32 launcher captures, default (960×640) and maximised (3840×2054) | text, columns and buttons grow together; no scroll bar at either size |
| Linux x64 Debug (WSL) build (ImGui 1.92.9) | exit 0 |
| Linux x64 core probe | `imgui_ui_scale_all=true`, `gl_renderer_identity_all=true`, `core_probe_all=true` |

* The new OSD was not captured on Linux. The window search matched an OBS window whose title contains "rePIU" once, and
  once the WSLg window did not come to the front and the VS Code screen was captured; neither capture was viewed or kept.
  The OSD code is shared, and #5 showed the Linux OSD through the same path on screen.
* The FPS/GPU/CPU text at the top right of the fullscreen capture is the NVIDIA overlay.

## Left

* At the launcher's default size, long titles reach the Status column's edge (as before); enlarging the window clears it.
