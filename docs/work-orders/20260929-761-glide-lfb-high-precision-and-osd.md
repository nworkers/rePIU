# Task 761 작업 지시: Glide LFB 고정밀 표시 경로와 in-game OSD 토글

설계: `docs/design/20260929-761-glide-lfb-high-precision-and-osd.md`

## 작업 항목

1. **HLE 함수.** `include/repiu/hle/glide_lfb_region.h`,
   `src/hle/glide_lfb_region.cpp`에 `WriteGlideLfbRegionRgba8`를 추가한다.
   클리핑·stride 규칙은 `WriteGlideLfbRegion`과 공유하고, 888/8888은 채널 그대로,
   16-bit는 565 pack → decode 왕복과 동일한 값을 쓴다.
2. **ThreadContext.** `src/engine/execution/thread_context.h`에 RGBA8 shadow와
   valid 플래그, 고정밀 present 계수를 추가한다.
3. **Backend 토글.** `GlideOpenGlBackend`에 `std::atomic<bool>` 토글과 접근자를
   추가하고, 창 열기 시 `REPIU_GLIDE_LFB_HIGH_PRECISION`을
   `runtime::ResolvePromotedToggle`로 읽어 초기화한다(기본 on).
4. **경계 배선.** `linexe_glide_boundary.cpp`의 seed/write/flush 세 지점을
   설계대로 바꾼다.
5. **OSD.** `include/repiu/engine/glide_osd.h`, `src/engine/glide_osd.cpp`를
   신설하고 백엔드의 창 열기/닫기/이벤트/스왑에 배선한다. `Tab`으로 표시를 토글한다.
6. **CMake.** `glide_osd.cpp`를 엔진 소스 블록에 추가하고 `repiu_exe`에
   `repiu_imgui`를 연결한다.
7. **Probe.** `glide_lfb_region_probe`에 새 함수의 바이트 보존·왕복 일치·클리핑
   케이스를 추가한다.
8. **문서.** `ARCHITECTURE.md`에 고정밀 경로와 OSD 절을 추가하고 작업 로그를
   남긴다.

## 완료 기준

* Debug 빌드와 `repiu_aot_probe` 전체 통과.
* 토글 off 기본값에서 기존 경로와 동작 차이 없음(코드 검토 + probe).

# Task 761 Work Order: Glide LFB High-Precision Path and In-Game OSD Toggle

Design: `docs/design/20260929-761-glide-lfb-high-precision-and-osd.md`

1. Add `WriteGlideLfbRegionRgba8` to the HLE region module, sharing the plan
   with the 565 writer; full channels for 888/8888, pack→decode parity for
   16-bit sources.
2. Add the RGBA8 shadow, valid flag, and present counter to `ThreadContext`.
3. Add the atomic toggle to `GlideOpenGlBackend`, initialized from
   `REPIU_GLIDE_LFB_HIGH_PRECISION` through `runtime::ResolvePromotedToggle`
   (on by default).
4. Wire the boundary's seed/write/flush sites per the design.
5. Create the `GlideOsd` subsystem and wire it into the backend's open, close,
   event pump, and swap; `Tab` toggles visibility.
6. Add `glide_osd.cpp` to the engine source block and link `repiu_imgui` into
   `repiu_exe`.
7. Extend `glide_lfb_region_probe` with byte-preservation, parity, and clipping
   cases.
8. Update `ARCHITECTURE.md` and leave the work log.

Done when the Debug build and the full `repiu_aot_probe` pass, and the
default-off path is behaviorally unchanged.
