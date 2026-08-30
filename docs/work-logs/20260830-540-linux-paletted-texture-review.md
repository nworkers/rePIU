# 20260830-540 Linux paletted texture 경로 검토 작업 로그

## 한국어

### 결과 요약

Linux 포트에 paletted texture 구현 누락은 확인되지 않았습니다. `glide_opengl_backend.cpp`
와 `glide_texture_decode.cpp`가 Win32와 Linux에서 공용으로 빌드되며, P_8/AP_88을 CPU에서
RGBA8로 확장한 뒤 OpenGL texture로 업로드합니다.

다만 현재 측정한 Linux WSLg 환경의 renderer는 Mesa `llvmpipe`이고 `Accelerated: no`였습니다.
따라서 Linux에서는 확장된 RGBA texture의 실제 샘플링과 rasterization까지 소프트웨어로
수행될 수 있어, paletted texture를 사용하는 장면이 하드웨어 가속 Win32 환경보다 불리할
가능성이 확인됐습니다.

### 실행 증거

기존 Linux i386 Release `pumpipx3` 실행의 최종 texture census는 다음과 같습니다.

| 항목 | 값 |
|---|---:|
| texture uploads / distinct | 54 / 48 |
| P_8 (`format 5`) | 42 |
| ARGB_4444 (`format 12`) | 12 |
| palette downloads / changed / identical | 266 / 266 / 0 |
| lazy refresh / failure | 224 / 0 |
| refresh source / RGBA bytes | 14,680,064 / 58,720,256 |
| refresh decode / upload | 25.845 ms / 7.021 ms |
| decode failures / palette missing | 0 / 0 |
| GL debug errors | 0 |

P_8 refresh 하나는 256×256 indexed source 65,536바이트를 262,144바이트 RGBA8로
확장하는 형태입니다. 기록된 refresh 호출의 decode+upload 합계는 32.866 ms이며,
평균은 약 0.147 ms + 0.031 ms입니다. 이 누계만으로는 약 200 ms/frame인 지속적인
5 fps 구간 전체를 설명할 수 없습니다. 단, upload 호출 이후의 소프트웨어 texture
sampling/rasterization 비용은 이 refresh 누계에 포함되지 않으므로 완전히 배제하지는
않습니다.

### 코드 대조

- Linux CMake도 공용 `src/engine/glide_opengl_backend.cpp`를 빌드합니다.
- `grTexDownloadTable`은 공용 boundary에서 palette를 RGBA8로 변환하고 backend의
  generation을 갱신합니다.
- `grTexDownloadMipMapLevel`은 현재 palette를 사용해 P_8/AP_88을 디코드합니다.
- palette 변경 후에는 모든 texture를 즉시 재생성하지 않고, 현재 draw에 사용되는
  stale texture만 `glTexSubImage2D`로 지연 갱신합니다.
- Linux 전용 palette 처리, Linux 전용 색상 변환, palette 누락 또는 decode 실패는
  확인되지 않았습니다.

### 위험도별 판정

**확인됨**

- Linux에서도 P_8 경로가 실제로 사용됩니다.
- Linux는 Win32와 다른 paletted texture 구현을 사용하지 않습니다.
- 현재 WSLg GL은 하드웨어 가속이 아닌 `llvmpipe`입니다.
- palette refresh 자체는 성공했고 palette 누락·decode 실패·GL debug error가 없습니다.

**추정**

- Linux WSLg에서는 P_8을 4바이트 RGBA8로 확장한 texture를 소프트웨어 rasterizer가
  샘플링하므로, fragment 비용이 커질 수 있습니다.
- `StoreTexture`와 `RefreshCurrentPalettizedTexture`는 다른 setter와 달리 upload 직후
  `glGetError()`를 무조건 호출합니다. Linux GL 구현에서 이 호출이 동기화 비용을 만들
  가능성은 남아 있지만, 이번 refresh 계측 합계는 작았습니다.

**미확정**

- palette texture의 fragment sampling 비용이 `pumpipx3` 후반 5 fps 절벽에서 차지하는 비율
- 실제 하드웨어 가속 Linux desktop에서 같은 장면의 성능
- palette 변경 시점과 특정 frame의 draw texture format별 비용

### 결론

Linux 포트가 paletted texture 기능을 빠뜨린 것이 원인은 아닙니다. 현재 Linux 측의
현실적인 추가 문제는 WSLg가 `llvmpipe` 소프트웨어 OpenGL이라는 점이며, 이는 팔레트
texture를 RGBA8로 확장하는 공용 설계와 결합될 때 Win32 하드웨어 가속 환경보다 불리할
수 있습니다. 다만 현재 로그의 refresh 비용만으로 5 fps 절벽을 단정할 수 없으므로,
다음 검토는 하드웨어 가속 Linux와의 비교 및 frame별 P_8 draw 비용 계측이어야 합니다.

### 변경 사항

소스 코드와 실행 파일은 변경하지 않았습니다. 기존 측정 로그와 OpenGL renderer 확인만
수행했습니다.

근거 로그: [task539_pumpipx3_dummy.err](../../build/task539_pumpipx3_dummy.err),
[task539_pumpipx3_default.err](../../build/task539_pumpipx3_default.err).
Renderer 확인 결과는 Mesa `llvmpipe (LLVM 20.1.2, 256 bits)`, OpenGL 4.5
Compatibility Profile, `Accelerated: no`였습니다.

## English

### Summary

No missing paletted-texture implementation was found in the Linux port. Linux and Win32
build the shared `glide_opengl_backend.cpp` and `glide_texture_decode.cpp`; P_8/AP_88 are
expanded on the CPU to RGBA8 and uploaded as OpenGL textures.

The measured Linux WSLg renderer is Mesa `llvmpipe` with `Accelerated: no`. Linux can
therefore perform the sampling and rasterization of the expanded RGBA textures in software,
which can make a paletted-texture scene less favourable than a hardware-accelerated Win32
environment.

### Evidence

The existing Linux i386 Release `pumpipx3` run reported:

| Item | Value |
|---|---:|
| texture uploads / distinct | 54 / 48 |
| P_8 (`format 5`) | 42 |
| ARGB_4444 (`format 12`) | 12 |
| palette downloads / changed / identical | 266 / 266 / 0 |
| lazy refresh / failure | 224 / 0 |
| refresh source / RGBA bytes | 14,680,064 / 58,720,256 |
| refresh decode / upload | 25.845 ms / 7.021 ms |
| decode failures / missing palette | 0 / 0 |
| GL debug errors | 0 |

One P_8 refresh expands a 65,536-byte 256×256 indexed source into 262,144 RGBA8 bytes.
The measured decode plus upload total is 32.866 ms, or approximately 0.147 ms plus
0.031 ms per refresh. That total cannot explain an entire sustained 5 FPS interval by
itself. Software texture sampling and rasterization after the upload are not included in
that refresh total, so they are not completely ruled out.

### Code comparison

- The Linux CMake target also builds the shared `src/engine/glide_opengl_backend.cpp`.
- `grTexDownloadTable` converts the palette to RGBA8 in the shared boundary and advances
  the backend palette generation.
- `grTexDownloadMipMapLevel` decodes P_8/AP_88 with the current palette.
- After a palette change, only the stale texture used by the current draw is lazily updated
  through `glTexSubImage2D`; all retained textures are not rebuilt immediately.
- No Linux-only palette path, Linux-only colour conversion, missing palette, or decode
  failure was found.

### Findings by confidence

**Confirmed**

- Linux actually uses the P_8 path.
- Linux does not use a different paletted-texture implementation from Win32.
- The current WSLg GL renderer is unaccelerated `llvmpipe`.
- Palette refreshes succeed, with no missing palettes, decode failures, or GL debug errors.

**Inferred**

- On Linux WSLg, the software rasterizer samples the 4-byte RGBA8 expansion of P_8
  textures, so fragment cost can rise in a scene that draws many paletted textures.
- `StoreTexture` and `RefreshCurrentPalettizedTexture` unconditionally call `glGetError()`
  after uploads, unlike the other setter paths. That could add synchronization cost on a
  Linux GL implementation, although the measured refresh total was small.

**Unresolved**

- The share of the `pumpipx3` late 5 FPS cliff attributable to paletted-texture sampling
- Performance of the same scene on an accelerated native Linux desktop
- Per-frame cost by palette-change timing and source texture format

### Conclusion

The Linux port did not omit paletted-texture functionality. The concrete additional Linux
risk in the current measurements is that WSLg uses unaccelerated `llvmpipe`; combined with
the shared RGBA8 expansion design, that can be worse than hardware-accelerated Win32 for
paletted scenes. The refresh totals alone do not prove that it causes the 5 FPS cliff, so
the next check should compare an accelerated Linux desktop and measure P_8 draw cost per
frame.

### Changes

No source code or executable was changed. Only existing measurements and the OpenGL renderer
were inspected.

Evidence logs: [task539_pumpipx3_dummy.err](../../build/task539_pumpipx3_dummy.err) and
[task539_pumpipx3_default.err](../../build/task539_pumpipx3_default.err). The renderer check
reported Mesa `llvmpipe (LLVM 20.1.2, 256 bits)`, OpenGL 4.5 Compatibility Profile, and
`Accelerated: no`.
