# Task 768 작업 지시: 화면 후처리 shader

설계: [20261004-768](../design/20261004-768-post-process-shaders.md)

## 작업 항목

1. **소스 조립.** `post_shader_source`(GL 없음): `#pragma parameter` 해석, 단계별 define
   삽입(`#version` 뒤), 매개변수 목록.
2. **목록.** `post_shader_catalog`(GL 없음): 내장 `crt`/`scanline` + `shaders/*.glsl`,
   디렉터리 결정(작업 디렉터리 → 실행 파일 옆), id 조회, 텍스트 읽기.
3. **내장 shader.** `src/engine/post_shaders/crt.glsl`, `scanline.glsl`을 새로 작성하고
   CMake가 생성 헤더로 포함한다.
4. **GL 모듈.** `glide_post_process`: 함수 해석, 컴파일·링크, scene 텍스처, `Apply`,
   상태 보존, 실패 시 `none`.
5. **Backend 배선.** 창 열기에서 생성하고 `REPIU_POST_SHADER`로 초기 선택, swap 직전
   `Apply`, 닫기에서 해제.
6. **OSD.** shader 콤보, Reload, 매개변수 슬라이더.
7. **런처.** `[Video] post_shader` 저장·읽기, `REPIU_POST_SHADER` 게시(환경 변수 우선),
   Options 콤보.
8. **Probe.** `post_shader_probe` 신설, `launcher_probe` 확장.
9. **문서.** `ARCHITECTURE.md` 절, 가이드 `docs/guides/post-process-shaders.md`, README 사용법,
   작업 로그.

## 완료 기준

* Win32 Debug 빌드와 `repiu_aot_probe` 전체 통과.
* `none`(기본)에서 swap 경로의 GL 호출이 바뀌지 않음(코드 검토).

# Task 768 Work Order: Post-Processing Shaders

Design: [20261004-768](../design/20261004-768-post-process-shaders.md)

1. **Source assembly.** `post_shader_source` (no GL): `#pragma parameter` parsing, per-stage
   defines inserted after `#version`, the parameter list.
2. **Catalog.** `post_shader_catalog` (no GL): built-in `crt`/`scanline` plus `shaders/*.glsl`,
   directory resolution (working directory, then next to the executable), id lookup, text loading.
3. **Built-in shaders.** Write `src/engine/post_shaders/crt.glsl` and `scanline.glsl`; CMake embeds
   them through a generated header.
4. **GL module.** `glide_post_process`: function resolution, compile and link, the scene texture,
   `Apply`, state preservation, fallback to `none` on failure.
5. **Backend wiring.** Create on window open with the initial selection from `REPIU_POST_SHADER`,
   `Apply` just before the swap, release on close.
6. **OSD.** Shader combo, Reload, parameter sliders.
7. **Launcher.** Store and load `[Video] post_shader`, publish `REPIU_POST_SHADER` (the environment
   wins), an Options combo.
8. **Probes.** New `post_shader_probe`; extend `launcher_probe`.
9. **Documents.** An `ARCHITECTURE.md` section, the guide `docs/guides/post-process-shaders.md`,
   README usage, and the work log.

Done when the Win32 Debug build and the full `repiu_aot_probe` pass, and the default `none` leaves
the swap path's GL calls unchanged (code review).
