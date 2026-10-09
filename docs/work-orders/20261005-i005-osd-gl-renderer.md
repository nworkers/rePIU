# #5 작업 지시: 인게임 OSD에 OpenGL renderer 표시

Issue: [#5](https://github.com/reexec/rePIU/issues/5) · 설계: [20261005-i005](../design/20261005-i005-osd-gl-renderer.md)

## 절차

1. `gl_renderer_identity.{h,cpp}`: 구조체와 `IsSoftwareGlRenderer`. `repiu_exe`에 추가.
2. Glide OpenGL backend: GL context 생성 직후 구조체를 채우고 로그 한 줄, OSD 초기화 뒤 `SetRendererIdentity`.
3. `GlideOsd`: `SetRendererIdentity`, OSD 맨 위 Renderer 절(소프트웨어면 경고 색).
4. probe `gl_renderer_identity`(core probe, Win32 AOT probe `--gl-renderer-identity`).
5. `ARCHITECTURE.md`의 `GlideOsd` 항목, README의 OSD 설명.
6. Win32 Release 빌드·core probe·실행과 OSD 캡처, Linux x64 Debug(WSL) 빌드·core probe.
7. 작업 로그.

## 완료 조건

probe가 두 호스트에서 통과하고, Win32에서 OSD를 열면 renderer·vendor·버전·드라이버가 보이며, 기존 OSD 항목과 프레임에 변화가
없습니다.

---

# #5 Work Order: The OpenGL Renderer in the In-Game OSD

Issue: [#5](https://github.com/reexec/rePIU/issues/5) · Design: [20261005-i005](../design/20261005-i005-osd-gl-renderer.md)

## Steps

1. `gl_renderer_identity.{h,cpp}`: the structure and `IsSoftwareGlRenderer`, added to `repiu_exe`.
2. Glide OpenGL backend: fill the structure right after the GL context is created, log one line, and call
   `SetRendererIdentity` once the OSD is initialised.
3. `GlideOsd`: `SetRendererIdentity`, and a Renderer section at the top of the overlay (warning colour for software).
4. The `gl_renderer_identity` probe (core probe, and `--gl-renderer-identity` in the Win32 AOT probe).
5. The `GlideOsd` entry of `ARCHITECTURE.md`, and the OSD description in the README.
6. Win32 Release build, core probe, a run with an OSD capture; Linux x64 Debug (WSL) build and core probe.
7. The work log.

## Done when

The probe passes on both hosts; on Win32 the open OSD shows the renderer, vendor, version and driver; and the existing OSD
controls and the frame rate are unchanged.
