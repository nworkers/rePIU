# 20260830-541 WSLg i386 3D 가속 경로 검토 작업 로그

## 한국어

### 결론

WSLg 자체의 3D 가속은 정상입니다. 같은 WSLg 세션에서 기본 GL 선택은
`llvmpipe`, `Accelerated: no`였지만, `GALLIUM_DRIVER=d3d12`를 명시하면
`Microsoft Corporation / D3D12 (NVIDIA GeForce RTX 4090) / Accelerated: yes`가
확인되었습니다.

따라서 기존 결론은 다음처럼 정정해야 합니다.

- 잘못된 일반화: Linux 또는 WSLg에서는 소프트웨어 OpenGL만 사용합니다.
- 현재까지의 정확한 결론: 기존 측정 세션의 기본 GL 경로가 `llvmpipe`로 폴백했고,
  32-bit 게임 프로세스는 i386 D3D12 DRI 모듈을 사용할 수 없는 상태일 가능성이 큽니다.

### 실행 증거

| 항목 | 결과 |
|---|---|
| 명시적 D3D12 renderer | `D3D12 (NVIDIA GeForce RTX 4090)` |
| 명시적 D3D12 가속 | `Accelerated: yes` |
| WSLg 장치 | `/dev/dxg`, `/dev/dri/renderD128` 존재 |
| 64-bit D3D12 DRI | `/usr/lib/x86_64-linux-gnu/dri/d3d12_dri.so` 존재 |
| 게임 실행 파일 | ELF 32-bit i386, `/lib/ld-linux.so.2` |
| 게임 GL ABI | `/lib/i386-linux-gnu/libGL.so.1`, `libGLX.so.0` |
| i386 D3D12 DRI | `/usr/lib/i386-linux-gnu/dri/d3d12_dri.so` 없음 |
| i386 Mesa 패키지 | `libgl1-mesa-dri:i386` 설치됨 |

`libgl1-mesa-dri:i386` 패키지는 설치되어 있지만 현재 파일 목록에는
`d3d12_dri.so`가 없습니다. 반면 64-bit Mesa 패키지에는 같은 모듈이 있습니다.
이 차이 때문에 64-bit `glxinfo`의 가속 성공과 32-bit 게임의 실제 renderer는
서로 다를 수 있습니다.

### 영향 해석

현재 Linux palette 경로의 기능 누락은 확인되지 않았습니다. P_8/AP_88은 공용
경로에서 RGBA8로 확장되므로, i386 프로세스가 `llvmpipe`로 실행되면 이 RGBA8
texture의 draw sampling/rasterization 비용이 하드웨어 가속 Win32와 크게 달라질
수 있습니다. 이는 palette 디코드 구현의 Linux 차이라기보다 renderer 선택 차이입니다.

다만 현재 결과만으로 실제 게임 프로세스가 반드시 `llvmpipe`를 사용한다고
확정하지는 않습니다. 최종 확인은 게임 시작 직후 같은 프로세스에서
`GL_VENDOR`, `GL_RENDERER`, `GL_VERSION`을 기록하거나, 동일 ABI의 i386 GL
probe로 수행해야 합니다.

### 확인됨·추정·미확정

**확인됨**

- WSLg 세션에서 D3D12 하드웨어 가속 GL 경로가 동작합니다.
- 기존 기본 renderer 조회는 `llvmpipe`였습니다.
- 현재 게임 바이너리는 32-bit i386입니다.
- i386 Mesa 패키지는 설치되어 있으나 i386 `d3d12_dri.so`는 보이지 않습니다.
- WSLg vGPU 장치와 64-bit D3D12 DRI 모듈은 존재합니다.

**추정**

- 현재 32-bit 게임이 i386 D3D12 DRI 부재로 `swrast/llvmpipe`에 폴백했을
  가능성이 높습니다.
- 이 폴백이 paletted texture 장면의 성능 저하를 크게 증폭했을 수 있습니다.

**미확정**

- 게임 프로세스 내부의 실제 `GL_RENDERER` 값
- Ubuntu 24.04 i386 Mesa에서 D3D12 DRI 모듈이 제공되지 않는 이유와 대체
  설치 경로
- 가속 경로에서 `pumpipx3`의 palette 장면이 Win32 대비 얼마나 느린지

### 다음 검증

1. 게임 GL 초기화 직후 renderer 문자열을 로그에 남깁니다.
2. 가능하면 i386 D3D12 Mesa 런타임을 제공하는 동일 환경에서 재실행합니다.
3. 가속 전·후에 같은 `pumpipx3` 장면의 frame time과 P_8 draw 비용을 비교합니다.
4. 그 뒤에도 차이가 남으면 `glGetError()` 동기화와 공용 RGBA8 texture 경로를
   별도 계측합니다.

### 참고

- [Microsoft WSLg](https://github.com/microsoft/wslg)
- [WSLg GPU selection](https://github.com/microsoft/wslg/wiki/GPU-selection-in-WSLg)

## English

### Conclusion

WSLg 3D acceleration itself is working. In the same WSLg session, the default GL
selection was `llvmpipe` with `Accelerated: no`, while explicitly selecting
`GALLIUM_DRIVER=d3d12` produced `Microsoft Corporation / D3D12 (NVIDIA GeForce RTX 4090) /
Accelerated: yes`.

The conclusion must therefore be corrected:

- Incorrect generalization: Linux or WSLg only uses software OpenGL.
- Current precise conclusion: the default GL path in the measured session fell back to
  `llvmpipe`, and the 32-bit game process likely cannot use an i386 D3D12 DRI module.

### Evidence

| Item | Result |
|---|---|
| Explicit D3D12 renderer | `D3D12 (NVIDIA GeForce RTX 4090)` |
| Explicit D3D12 acceleration | `Accelerated: yes` |
| WSLg devices | `/dev/dxg`, `/dev/dri/renderD128` present |
| 64-bit D3D12 DRI | `/usr/lib/x86_64-linux-gnu/dri/d3d12_dri.so` present |
| Game executable | ELF 32-bit i386, `/lib/ld-linux.so.2` |
| Game GL ABI | `/lib/i386-linux-gnu/libGL.so.1`, `libGLX.so.0` |
| i386 D3D12 DRI | `/usr/lib/i386-linux-gnu/dri/d3d12_dri.so` absent |
| i386 Mesa package | `libgl1-mesa-dri:i386` installed |

The installed `libgl1-mesa-dri:i386` package does not currently expose
`d3d12_dri.so`, while the 64-bit Mesa package does. Consequently, a successful
64-bit `glxinfo` acceleration check does not prove the renderer used by the 32-bit game.

### Interpretation

No missing Linux palette functionality was found. P_8/AP_88 use the shared path and are
expanded to RGBA8, so an i386 process running through `llvmpipe` can have very different
draw sampling and rasterization costs from hardware-accelerated Win32. This is a renderer
selection difference, not a Linux-specific palette decoder.

The game process's actual renderer is not yet proven by this check. The final check must
record `GL_VENDOR`, `GL_RENDERER`, and `GL_VERSION` inside the game process, or use an
i386 GL probe with the same runtime.

### Next verification

1. Log the renderer strings immediately after the game's GL initialization.
2. Re-run in an environment that provides an i386 D3D12 Mesa runtime, if available.
3. Compare frame time and per-frame P_8 draw cost before and after acceleration.
4. If a gap remains, measure `glGetError()` synchronization and the shared RGBA8 texture
   path separately.

### References

- [Microsoft WSLg](https://github.com/microsoft/wslg)
- [WSLg GPU selection](https://github.com/microsoft/wslg/wiki/GPU-selection-in-WSLg)
