# #15 작업 지시: re2DJ 형태의 OSD와 창 크기에 비례하는 UI 글자

Issue: [#15](https://github.com/reexec/rePIU/issues/15) · 설계: [20261006-i015](../design/20261006-i015-osd-re2dj-layout.md)

## 절차

1. `imgui_ui_scale.{h,cpp}`: `UiScaleRule`, `UiScaleForHeight`, `ApplyImGuiUiScale`. `repiu_exe`에 추가.
2. `session_identity.{h,cpp}`: `SetSessionTargetProfile`, `SessionTargetProfile`. loader가 프로필 확정 직후 호출.
3. `GlideOsd`: 정보 줄(`SetInfoLines`), 맨 위 가로 전체 폭 창, 매 프레임 배율 적용.
4. Glide backend: OSD 생성 시 첫 줄(창 제목과 같은 표기)과 Target Profile 줄을 넘김.
5. 런처: `SDL_WINDOW_RESIZABLE`, 매 프레임 배율 적용, 고정 픽셀 값에 배율.
6. probe `imgui_ui_scale`(core probe, Win32 AOT probe `--imgui-ui-scale`).
7. `ARCHITECTURE.md`(`GlideOsd`, 런처), README OSD 설명.
8. Win32 Release 빌드·probe·OSD와 런처 캡처, Linux x64 Debug(WSL) 빌드·core probe. 작업 로그.

## 완료 조건

probe가 두 호스트에서 통과하고, OSD가 화면 맨 위 가로 전체 폭에 이름·버전·빌드 날짜와 Target Profile을 보이며, OSD와 런처의
글자가 창을 키우면 함께 커집니다.

---

# #15 Work Order: An OSD in the re2DJ Layout, and UI Text That Scales with the Window

Issue: [#15](https://github.com/reexec/rePIU/issues/15) · Design: [20261006-i015](../design/20261006-i015-osd-re2dj-layout.md)

## Steps

1. `imgui_ui_scale.{h,cpp}`: `UiScaleRule`, `UiScaleForHeight`, `ApplyImGuiUiScale`, added to `repiu_exe`.
2. `session_identity.{h,cpp}`: `SetSessionTargetProfile` and `SessionTargetProfile`; the loader calls it right after
   settling the profile.
3. `GlideOsd`: information lines (`SetInfoLines`), a full-width window at the top, the scale applied each frame.
4. Glide backend: when creating the OSD, hand over the first line (written as in the window title) and the Target Profile
   line.
5. Launcher: `SDL_WINDOW_RESIZABLE`, the scale applied each frame, fixed pixel values multiplied by it.
6. The `imgui_ui_scale` probe (core probe, and `--imgui-ui-scale` in the Win32 AOT probe).
7. `ARCHITECTURE.md` (`GlideOsd`, the launcher) and the README's OSD description.
8. Win32 Release build, probes, OSD and launcher captures; Linux x64 Debug (WSL) build and core probe. The work log.

## Done when

The probe passes on both hosts; the OSD spans the full width at the top with the name, version, build date and Target
Profile; and OSD and launcher text grows when the window is enlarged.
