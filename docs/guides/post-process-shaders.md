# 화면 후처리 shader / Post-processing shaders

설계: [20261004-768](../design/20261004-768-post-process-shaders.md) ·
작업 로그: [20261004-768](../work-logs/20261004-768-post-process-shaders.md)

## 한국어

### 1. 고르는 방법

| 방법 | 지속 | 예 |
|---|---|---|
| 런처 Options의 "Screen shader" | `cfg/repiu.ini`의 `[Video] post_shader`에 저장 | `post_shader = crt` |
| 환경 변수 | 그 실행 (런처 값보다 우선) | `REPIU_POST_SHADER=scanline` |
| 게임 중 `Tab` OSD | 그 실행만 | 콤보, Reload, 매개변수 슬라이더 |

id는 `none`, 내장 `crt`·`scanline`, 또는 `shaders/` 안 파일 이름(`my_crt.glsl`)입니다.
`shaders/`는 작업 디렉터리를 먼저 보고, 없으면 실행 파일 옆을 봅니다(`cfg/`와 같은 규칙).

### 2. 직접 shader 쓰기

libretro 단일 pass GLSL 형식의 부분집합입니다. 한 파일에 두 단계를 `VERTEX`/`FRAGMENT`로
나눕니다. 내장 `src/engine/post_shaders/scanline.glsl`이 가장 짧은 예입니다.

```glsl
#pragma parameter STRENGTH "Strength" 0.5 0.0 1.0 0.05

#if defined(VERTEX)
attribute vec4 VertexCoord;
attribute vec4 TexCoord;
uniform mat4 MVPMatrix;
varying vec2 tex_coord;
void main()
{
    gl_Position = MVPMatrix * VertexCoord;
    tex_coord = TexCoord.xy;
}
#elif defined(FRAGMENT)
uniform sampler2D Texture;
uniform vec2 TextureSize;   // 게임 논리 해상도 (예: 640x480)
uniform vec2 OutputSize;    // 창의 픽셀 크기
varying vec2 tex_coord;
#ifdef PARAMETER_UNIFORM
uniform float STRENGTH;
#else
#define STRENGTH 0.5
#endif
void main()
{
    gl_FragColor = texture2D(Texture, tex_coord) * (1.0 - STRENGTH * 0.5);
}
#endif
```

쓸 수 있는 이름: attribute `VertexCoord`, `TexCoord`, `COLOR`; uniform `MVPMatrix`, `Texture`,
`InputSize`, `TextureSize`, `OutputSize`, `FrameCount`, `FrameDirection`; `#pragma parameter`.
다단계 preset(`.glslp`), `PrevTexture`, LUT는 아직 지원하지 않습니다.

작성 중에는 게임을 켠 채 파일을 고치고 OSD의 **Reload**를 누르면 됩니다. 컴파일 오류는 OSD에
빨간 글씨로, 로그에 `[repiu-post] shader '…' not applied: …`로 나오며 그동안 화면은 `none`으로
그려집니다.

### 3. 확인 절차

1. `REPIU_POST_SHADER=crt`로 롬셋을 실행합니다. 로그에 `[repiu-post] shader: crt (6 parameters)`.
2. 화면이 휘고 주사선과 마스크가 보이는지 봅니다. 창 배율 2x 이상(기본 2x, `Alt+1`~`Alt+4`)에서
   주사선이 뚜렷합니다.
3. `Tab` → Screen shader에서 `scanline`, `none`으로 바꿔 즉시 바뀌는지 확인합니다. `none`이면
   후처리 없는 원래 화면입니다.
4. `shaders/test.glsl`에 위 예제를 넣고 Reload → 목록에 `test.glsl`이 나오고 고를 수 있는지
   확인합니다. 파일에 일부러 오타를 넣고 Reload하면 오류가 보이고 화면은 `none`이어야 합니다.
5. 런처에서 Screen shader를 고르고 실행한 뒤 다시 런처로 돌아와 값이 유지되는지 확인합니다.

## English

### 1. Choosing a shader

| Way | Lasts | Example |
|---|---|---|
| "Screen shader" under the launcher's Options | stored as `[Video] post_shader` in `cfg/repiu.ini` | `post_shader = crt` |
| Environment variable | that run (wins over the launcher's value) | `REPIU_POST_SHADER=scanline` |
| The in-game `Tab` OSD | that run only | combo, Reload, parameter sliders |

Ids are `none`, the built-in `crt` and `scanline`, or a file name in `shaders/` (`my_crt.glsl`).
`shaders/` is looked up in the working directory first and next to the executable otherwise, the
rule `cfg/` follows.

### 2. Writing a shader

The format is a subset of libretro single-pass GLSL: one file, two stages guarded by `VERTEX` and
`FRAGMENT`. The built-in `src/engine/post_shaders/scanline.glsl` is the shortest example, and the
Korean section above has a minimal one. Available names: attributes `VertexCoord`, `TexCoord`,
`COLOR`; uniforms `MVPMatrix`, `Texture`, `InputSize`, `TextureSize` (both the game's logical
resolution, such as 640x480), `OutputSize` (the window's pixel size), `FrameCount`,
`FrameDirection`; and `#pragma parameter`. Multi-pass presets (`.glslp`), `PrevTexture` and LUTs are
not supported yet.

While writing, keep the game running, edit the file and press **Reload** in the OSD. A compile
error appears in red in the OSD and in the log as `[repiu-post] shader '…' not applied: …`, and the
screen is drawn as `none` meanwhile.

### 3. Check procedure

1. Run a ROM set with `REPIU_POST_SHADER=crt`. The log shows `[repiu-post] shader: crt (6 parameters)`.
2. Check for curvature, scanlines and the mask. Scanlines are clear at a 2x window scale or more
   (2x by default, `Alt+1` to `Alt+4`).
3. `Tab` → Screen shader: switch to `scanline` and `none` and check the change is immediate. `none`
   is the unprocessed picture.
4. Put the example in `shaders/test.glsl` and press Reload: `test.glsl` appears and can be chosen.
   Introduce a typo and Reload: the error shows and the screen is `none`.
5. Pick a Screen shader in the launcher, play, return to the launcher, and check the value stayed.
