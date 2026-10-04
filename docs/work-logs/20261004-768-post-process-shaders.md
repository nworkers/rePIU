# Task 768 작업 로그: 화면 후처리 shader

설계: [20261004-768](../design/20261004-768-post-process-shaders.md) ·
작업 지시: [20261004-768](../work-orders/20261004-768-post-process-shaders.md) ·
가이드: [post-process-shaders](../guides/post-process-shaders.md) ·
kb: [libretro GLSL 후처리 shader 형식](../kb/libretro-glsl-post-shaders.md)

## 요약

Glide backend의 buffer swap 직전에 표시 단계 전용 후처리 pass를 넣었습니다. shader는 libretro
단일 pass GLSL 부분집합이고, 내장 `crt`·`scanline`과 `shaders/*.glsl` 사용자 파일을 런처(저장),
환경 변수 `REPIU_POST_SHADER`, 게임 중 `Tab` OSD(그 실행만, Reload와 매개변수 슬라이더)에서
고릅니다. 기본값 `none`은 GL 호출을 하나도 추가하지 않습니다.

## 바꾼 것

| 영역 | 파일 |
|---|---|
| 소스 조립 (GL 없음) | `include/repiu/engine/post_shader_source.h`, `src/engine/post_shader_source.cpp` |
| 목록 (GL 없음) | `include/repiu/engine/post_shader_catalog.h`, `src/engine/post_shader_catalog.cpp` |
| 내장 shader | `src/engine/post_shaders/crt.glsl`, `scanline.glsl`, `post_shader_builtins.inc.in` |
| GL pass | `include/repiu/engine/glide_post_process.h`, `src/engine/glide_post_process.cpp` |
| 배선 | `glide_opengl_backend.h/.cpp`(생성·`Apply`·해제), `glide_osd.h/.cpp`(메뉴) |
| 런처 | `launcher_settings.h/.cpp`(`[Video] post_shader`, 게시), `launcher_ui.cpp`(콤보), `host/loader/main.cpp`(세 번째 override 인자, 로그) |
| 빌드 | `CMakeLists.txt`: 새 소스 3개, `configure_file`로 생성 헤더, probe 소스 |
| probe | `post_shader_probe.*` 신설(`--post-shader`), `launcher_probe.cpp` 확장, `glide_render_probe --opengl-post-shader` 신설 |
| 문서 | 설계, 작업 지시, 가이드, kb와 색인, `ARCHITECTURE.md`, `README.md` |

## 검증 중 발견해 고친 것: 2배율에서 주사선이 사라짐

처음 작성한 두 shader는 줄 가운데에 대칭인 밝기 곡선(`scanline`은 사인, `crt`는 줄 중심에
놓인 가우시안 빔)을 썼습니다. 기본 창 배율 2x에서는 한 논리 줄의 두 출력 행 중심이 0.25와
0.75로 줄 가운데에 대칭이라 두 행이 같은 밝기가 되고, **기본 배율에서 주사선이 보이지 않습니다.**
GL probe를 설계하면서 계산으로 찾았고, 두 shader를 고친 뒤 probe로 확인했습니다.

* `scanline`: 줄의 뒤쪽 절반을 틈으로, 가장자리는 출력 1픽셀 폭으로 부드럽게. 1x에서는 꺼짐.
* `crt`: 빔 중심을 출력 반 픽셀 옮겨 정수 배율에서 픽셀 중심에 맞춤(`OutputSize` 사용).

일반 지식으로 [kb 5절](../kb/libretro-glsl-post-shaders.md)에 남겼습니다.

### 후속 조정: crt 곡률

사용자 확인에서 곡률이 과하다는 의견을 받아 `CRT_CURVATURE` 기본값을 0.08에서 0.03으로 낮췄습니다.
0.08은 화면 모서리를 가로·세로로 약 8% 밀어내 휨이 눈에 띄었고, 0.03은 그 3분의 1 남짓입니다. 슬라이더 범위(0~0.30)는
그대로입니다. 다시 빌드한 뒤 `--opengl-post-shader`(`crt=true`, 모서리 픽셀은 여전히 검정)와
`--post-shader`가 통과했습니다.
이어서 사용자 요청으로 0.01까지 더 낮췄습니다. 같은 probe가 다시 통과했습니다.

## 검증

* Win32 x86 Debug 전체 빌드 exit 0. 새로 쓰거나 고친 파일에서 경고 없음(기존 C4819 코드 페이지
  경고만).
* `repiu_aot_probe --post-shader`: `post_shader_assembly/parameters/builtins/catalog/variable/all`
  모두 `true`.
* `repiu_aot_probe --launcher`: `launcher_all=true` (shader 키 왕복, 게시 우선순위 포함).
* `repiu_glide_render_probe --opengl-post-shader` (Intel Arc 130T, 64×48 논리, 128×96 drawable):
  `state_restored=true`, `scanline_rows=48/48/96`(밝은 48행, 어두운 48행), `crt=true`,
  `none=true`, `glide_render_probe=pass`. `--opengl-lfb`도 pass.
* 실제 게임: pumpit1 Debug 15초 시간 제한 실행. `none`은 458프레임, `crt`는 455프레임,
  둘 다 exit 0, 로그 1,100줄로 같음, GL 오류 보고 없음, `[repiu-post] shader: crt (6 parameters)`.
  vsync가 켜진 Debug라 프레임 수 차이는 측정 잡음 범위입니다. 이 두 실행은 주사선 수정 전 shader였고,
  수정 후 다시 빌드해 `crt` 459프레임, `scanline` 453프레임(둘 다 exit 0, 정상 teardown)을 확인했습니다.
* shutdown 줄의 `attempts`는 실행마다 1, 5, 2로 달랐습니다. `recovered=1 failure=0`이고 shader 없는
  실행에서도 값이 흔들리는 시간 제한 teardown 지표라 이 작업과 무관하다고 판단했습니다.

## 하지 않은 것

* **Linux i386 빌드.** "WSL에 cmake·g++ 없음"이라는 처음 기록은 틀렸습니다. 이후 WSL에서 x64 Debug
  빌드와 probe, x11 실제 게임(crt)으로 확인했습니다. 결과는
  [Task 769 로그의 WSL 검증 절](20261004-769-fullscreen-toggle-and-aspect.md)에 있습니다.
* **`repiu_aot_probe` 전체 스위트:** DOS/4GW 실행 파일 인자가 필요하고, Task 761 로그에 적힌 기존
  회귀(`dbt_indirect_dispatch_*`)에서 멈춥니다. 그래서 새 probe와 런처 probe를 단독 플래그로
  실행했습니다.
* **화면을 눈으로 확인:** 게임 창이 주 모니터 밖에 열려 스크린샷에 잡히지 않았습니다. 그림 확인은
  GL probe의 픽셀 검사로 대신했고, `Tab` OSD 조작과 실제 화질은 사용자 확인이 필요합니다
  ([가이드 3절](../guides/post-process-shaders.md)).

---

# Task 768 Work Log: Post-Processing Shaders

Design: [20261004-768](../design/20261004-768-post-process-shaders.md) ·
Work order: [20261004-768](../work-orders/20261004-768-post-process-shaders.md) ·
Guide: [post-process-shaders](../guides/post-process-shaders.md) ·
kb: [libretro GLSL post-processing shader format](../kb/libretro-glsl-post-shaders.md)

## Summary

A presentation-only post-processing pass now runs just before the Glide backend's buffer swap.
Shaders are a subset of libretro single-pass GLSL; the built-in `crt` and `scanline` and user files in
`shaders/*.glsl` are chosen in the launcher (stored), through `REPIU_POST_SHADER`, or in game from the
`Tab` OSD (that run only, with Reload and parameter sliders). The default `none` adds no GL call.

## Changes

Two GL-free modules (`post_shader_source`, `post_shader_catalog`), the built-in `.glsl` sources embedded
by `configure_file`, the GL module `glide_post_process`, wiring in the backend, OSD, launcher settings,
launcher UI and loader host, the new `post_shader_probe` (`--post-shader`), an extended
`launcher_probe`, a new `glide_render_probe --opengl-post-shader`, and the design, work order, guide, kb
topic and index, `ARCHITECTURE.md` and `README.md`.

## Found and fixed during verification: no scanlines at 2x

Both shaders were first written with brightness profiles symmetric about a line's middle (a sine for
`scanline`, a Gaussian beam centred on the line for `crt`). At the default 2x window scale a logical
line's two output rows have centres at 0.25 and 0.75, symmetric about the middle, so both read equally
bright and **no scanline showed at the default scale**. This was found by working out the expected
values while designing the GL probe, and the probe confirmed the fix:

* `scanline`: the back half of each line is the gap, with a one-output-pixel soft edge; off at 1x.
* `crt`: the beam centre moves by half an output pixel so it lands on a pixel centre at integer scales
  (uses `OutputSize`).

The general lesson is in [kb section 5](../kb/libretro-glsl-post-shaders.md).

### Follow-up: crt curvature

The user found the curvature excessive, so the `CRT_CURVATURE` default went from 0.08 to 0.03. At 0.08 the
corners are pushed out by about 8% on each axis and the bend is obvious; 0.03 is a little over a third of that. The slider range (0 to 0.30) is
unchanged. After a rebuild, `--opengl-post-shader` (`crt=true`, the corner pixel still black) and
`--post-shader` pass.
The user then asked for 0.01, and the same probes pass again.

## Verification

* Full Win32 x86 Debug build, exit 0; no warnings from new or changed files (only the existing C4819
  code-page warnings).
* `repiu_aot_probe --post-shader`: `post_shader_assembly/parameters/builtins/catalog/variable/all` all
  `true`.
* `repiu_aot_probe --launcher`: `launcher_all=true`, including the shader key round trip and publish
  precedence.
* `repiu_glide_render_probe --opengl-post-shader` (Intel Arc 130T, 64×48 logical, 128×96 drawable):
  `state_restored=true`, `scanline_rows=48/48/96` (48 bright and 48 dark rows), `crt=true`,
  `none=true`, `glide_render_probe=pass`. `--opengl-lfb` also passes.
* Real game: pumpit1 Debug with a 15-second limit. `none` presented 458 frames and `crt` 455, both exit
  0 with the same 1,100 log lines, no GL error reported, and `[repiu-post] shader: crt (6 parameters)`.
  With vsync on in Debug, the frame difference is measurement noise. Those two runs used the shaders
  before the scanline fix; after rebuilding, `crt` presented 459 frames and `scanline` 453, both exit 0
  with a normal teardown.
* The shutdown line's `attempts` read 1, 5 and 2 across runs. With `recovered=1 failure=0`, and being a
  time-limit teardown metric that varies without a shader too, it is judged unrelated.

## Not done

* **The Linux i386 build.** The first note that WSL had no cmake or g++ was wrong; the x64 Debug build,
  the probes and a real x11 game run with `crt` were checked in WSL afterwards, recorded in the
  [Task 769 log's WSL verification section](20261004-769-fullscreen-toggle-and-aspect.md).
* **The full `repiu_aot_probe` suite:** it needs a DOS/4GW executable argument and stops at the
  existing regression recorded in the Task 761 log (`dbt_indirect_dispatch_*`), so the new probe and
  the launcher probe were run through their standalone flags.
* **Seeing the screen:** the game window opened off the primary monitor and was not in the screenshot.
  The picture was checked through the GL probe's pixel tests instead; the `Tab` OSD and the look on
  real content need a user check ([guide section 3](../guides/post-process-shaders.md)).
