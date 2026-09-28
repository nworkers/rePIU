# Task 760: WSL의 D3D12 드라이버를 프로세스의 아키텍처로 찾는다

## 한국어

### 배경

Task 759의 검증으로 Linux i386 빌드를 실행하니 창을 열지 못하고 끝났다.

```
glx: failed to create drisw screen
X Error of failed request:  BadValue (integer parameter out of range for operation)
  Major opcode of failed request:  148 (GLX)
  Minor opcode of failed request:  3 (X_GLXCreateContext)
```

`REPIU_WSL_D3D12=0`으로는 실행됐다. Task 752의 드라이버 선택(`SelectWslD3d12Driver`)은 드라이버 파일을 네
경로에서 찾는데, 그 목록이 아키텍처를 가리지 않았다. 이 머신에는 `/usr/lib/x86_64-linux-gnu/dri/d3d12_dri.so`만
있고 i386용은 없다. i386 프로세스가 x86-64 드라이버 파일을 보고 `GALLIUM_DRIVER=d3d12`를 설정했고, 32비트
Mesa는 그 드라이버를 읽을 수 없어 컨텍스트를 만들지 못했다.

### 설계

드라이버 경로 목록을 아키텍처별로 둔다. 플랫폼 계층의 규칙대로 `src/platform/linux/x64/`와
`src/platform/linux/x86/`의 `host_gpu_driver_arch.cpp`가 목록을 주고, 공용 `host_gpu_driver.cpp`가 그것을 쓴다.

| 아키텍처 | 경로 |
|---|---|
| x64 | `/usr/lib/x86_64-linux-gnu/dri/`, `/usr/lib64/dri/`, `/usr/lib/dri/` |
| x86 | `/usr/lib/i386-linux-gnu/dri/`, `/usr/lib32/dri/` |

x86 목록에서 `/usr/lib/dri/`를 뺀다. multiarch 시스템에서 그 디렉터리는 x86-64 드라이버를 담고, 잘못 고르면
창이 열리지 않는다(고르지 않으면 소프트웨어 렌더러로 그린다).

### 검증 전략

두 아키텍처를 빌드해 core probe를 확인하고, 각각 한 번 실행해 최종 보고의 `Glide GL renderer/
wsl-d3d12-chosen`을 본다.

## English

### Background

Running the Linux i386 build for Task 759's verification ended without a window.

```
glx: failed to create drisw screen
X Error of failed request:  BadValue (integer parameter out of range for operation)
  Major opcode of failed request:  148 (GLX)
  Minor opcode of failed request:  3 (X_GLXCreateContext)
```

It ran with `REPIU_WSL_D3D12=0`. Task 752's driver choice (`SelectWslD3d12Driver`) looks for the driver
file in four paths, and that list did not tell architectures apart. This machine has
`/usr/lib/x86_64-linux-gnu/dri/d3d12_dri.so` and no i386 one. The i386 process saw the x86-64 driver file
and set `GALLIUM_DRIVER=d3d12`; the 32-bit Mesa cannot load that driver and no context was made.

### Design

The list of driver paths is kept per architecture. By the platform layer's rule,
`host_gpu_driver_arch.cpp` in `src/platform/linux/x64/` and `src/platform/linux/x86/` gives the list and
the shared `host_gpu_driver.cpp` uses it.

| Architecture | Paths |
|---|---|
| x64 | `/usr/lib/x86_64-linux-gnu/dri/`, `/usr/lib64/dri/`, `/usr/lib/dri/` |
| x86 | `/usr/lib/i386-linux-gnu/dri/`, `/usr/lib32/dri/` |

`/usr/lib/dri/` is left out of the x86 list. On a multiarch system that directory holds the x86-64
drivers, and a wrong choice leaves no window (no choice draws with the software renderer).

### Verification strategy

Both architectures are built and their core probes checked, and each is run once to read the final
report's `Glide GL renderer/wsl-d3d12-chosen`.
