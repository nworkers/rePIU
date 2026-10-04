# Task 769 작업 지시: 전체화면 전환과 화면 비율 유지

설계: [20261004-769](../design/20261004-769-fullscreen-toggle-and-aspect.md)

1. **letterbox 계산.** `include/repiu/engine/glide_letterbox.h`, `src/engine/glide_letterbox.cpp`에
   순수 함수 `ComputeGlideLetterboxRect`.
2. **backend.** content rect 멤버, `ApplyDrawableViewport`·readback·진단·띠 clear를 rect 기준으로,
   `ToggleFullscreen`, `Alt+Enter`와 더블클릭 처리, 전체화면 중 `Alt+1~4` 무시.
3. **OSD.** `WantsMouse()`.
4. **후처리.** `GlidePostProcess::Apply`가 rect를 받음.
5. **probe.** `glide_letterbox_probe`(`--glide-letterbox`), render probe 인자 갱신.
6. **문서.** `ARCHITECTURE.md`, README 조작 설명, 작업 로그.

완료 기준: Win32 Debug 빌드, 새 probe와 render probe 통과.

# Task 769 Work Order: Fullscreen Toggle and Aspect-Preserving Scaling

Design: [20261004-769](../design/20261004-769-fullscreen-toggle-and-aspect.md)

1. **Letterbox.** The pure function `ComputeGlideLetterboxRect` in `glide_letterbox.h/.cpp`.
2. **Backend.** A content-rect member; `ApplyDrawableViewport`, readback, diagnostics and bar clearing
   on that rect; `ToggleFullscreen`; `Alt+Enter` and double-click handling; `Alt+1` to `Alt+4` ignored
   while fullscreen.
3. **OSD.** `WantsMouse()`.
4. **Post-process.** `GlidePostProcess::Apply` takes the rect.
5. **Probes.** `glide_letterbox_probe` (`--glide-letterbox`); the render probe's call updated.
6. **Documents.** `ARCHITECTURE.md`, README controls, the work log.

Done when the Win32 Debug build, the new probe and the render probe pass.
