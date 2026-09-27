# Android arm64 이식 frontier / Android arm64 port frontier

설계: [20260928-756](../design/20260928-756-android-arm64-execution.md) ·
작업 지시: [20260928-756](../work-orders/20260928-756-android-arm64-preparation.md) ·
작업 로그: [20260928-756](../work-logs/20260928-756-android-arm64-preparation.md)

이 문서는 **Android arm64 이식이 지금 어디까지 왔고 다음에 무엇이 필요한지**만 유지합니다.
단계별 증거는 작업 로그에, 단계 계획과 결정 항목은 설계에 있습니다. 표기는 이 디렉터리의
규칙을 따릅니다: **확인됨**, **추정**, **미확정**.

## 1. 상태: Stage 0 완료, 결정 대기 (2026-09-28)

**측정과 설계가 끝났고, 코드는 바뀌지 않았습니다.** Stage 1(빌드 구성)은 설계의 결정 항목 13개
가운데 1, 4, 5, 6, 8번에 답이 있어야 시작할 수 있습니다.

**Android에서 게임은 실행되지 않습니다.** 현재 backend 둘(`legacy`, `dynamic`)이 모두 x86 호스트
위에 서 있고, arm64에는 복사할 수 있는 게스트 바이트가 하나도 없습니다.

## 2. 확인됨: 지금 서 있는 것

측정 환경: x86-64 Ubuntu 24.04 컨테이너, `g++-aarch64-linux-gnu` 13.2, CMake 3.28, Ninja. 저장소
수정 없음.

| 항목 | 값 | 근거 |
|---|---|---|
| aarch64-linux-gnu 크로스 configure | **통과** (SDL3 3.4.10 headless, spdlog 1.14.1, libchdr, Zydis, imgui, minimp3) | Task 756 |
| `repiu_exe` 오브젝트 | **178 / 183 컴파일** | Task 756 |
| 실패 원인 | ucontext 변환 1개, x86-64 `.S` 4개. 전부 호스트 CPU 전용 | Task 756 |
| 실행 엔진(`src/engine/`) 컴파일 | **전부 통과**. x86 전용 부분은 `#if` 뒤의 "지원하지 않음" 가지 | Task 756 |
| aarch64에서의 실행 결과 | `IsDirectX86ExecutionSupported()`와 `IsCodeCacheEntrySupported()`가 모두 거짓이라 `RunExecutionThread`가 "requires a 32-bit host"로 끝남 (코드 읽기) | Task 756 |
| CMake의 아키텍처 판정 | `CMAKE_SIZEOF_VOID_P`만 봄. `CMAKE_SYSTEM_PROCESSOR` 검사 0곳 | Task 756 |
| `__aarch64__` 분기 | 1곳 (`src/platform/build_identity.cpp`) | Task 756 |
| `__ANDROID__` 분기 | 0곳 | Task 756 |
| 게스트 주소를 호스트 포인터로 쓰는 identity cast | 26곳 (`src/hle`, `src/runtime`, `src/engine`) | Task 756 |
| 데스크톱 GL 고정 기능 호출 | 41곳 (`glide_opengl_backend.cpp`) | Task 756 |
| GLSL | `#version 110` 2개, 런처 `#version 130` | Task 756 |
| SDL3 Android 최소 API | 21 | SDL3 3.4.10 `docs/README-android.md` |
| SDL3 `android-project` 템플릿 | NDK 28.2, compileSdk 35, `arm64-v8a`, CMake externalNativeBuild | 같은 소스 |
| Android 16 KB 페이지 | Android 15부터, NDK r28부터 기본 정렬, Play 요구 2027-02-01 (API 35 이상 대상) | [Android 문서](https://developer.android.com/guide/practices/page-sizes) |
| 인터프리터가 구현해야 하는 크기 | 명령 형태 320개, 상위 17 mnemonic 87.89% | Task 514 |
| Linux x64의 대조 기준 | 디스크가 있는 16개 롬셋 실행 | v0.0.194 |

컴파일 실패의 실제 메시지입니다.

```
src/platform/linux/guest_cpu_context.cpp:26:47: error: 'const struct mcontext_t'
    has no member named 'gregs'; did you mean 'regs'?
src/platform/linux/aot_dbt_return_thunk_x64.S:18: Error: unknown pseudo-op: `.intel_syntax'
src/platform/linux/aot_dbt_return_thunk_x64.S:24: Error: expected a register or
    register list at operand 1 -- `mov qword ptr[rip+repiu_linux_x64_return_thunk_rsp],rsp'
```

## 3. 추정

* **인터프리터로 플레이 가능한 속도가 나올 수 있습니다.** 게스트는 1999년 DOS 게임이고 현대
  arm64 코어는 그 시대 CPU보다 수십 배 빠릅니다. 측정 전이므로 추정입니다.
* **Android 앱 프로세스에서 `0x00010000`부터 134 MB 배치가 성립할 것입니다.** `mmap_min_addr`가
  보통 `0x8000`이고 64비트 프로세스의 실행 파일과 라이브러리는 훨씬 위에 놓이기 때문입니다. zygote와
  ART의 선점 구간은 기기마다 다르므로 Stage 1 probe로 확정합니다.

## 4. 미확정

| 항목 | 확인 방법 |
|---|---|
| Android 앱 프로세스의 하위 4 GiB 배치 | Stage 1 probe 앱, 4 KB와 16 KB 페이지 기기 |
| bionic의 `pthread_timedjoin_np`, `process_vm_readv`, `posix_spawn` API 수준 | NDK 헤더의 `__INTRODUCED_IN` 대조. 이 컨테이너에서는 bionic 소스에 닿지 못했습니다 |
| 앱 프로세스의 RX 익명 메모리(`execmem`) | Stage 4에만 필요 |
| ART `libsigchain`과 자체 signal 핸들러 | 인터프리터 경로에는 폴트 전달이 불필요 |
| 인터프리터 속도 | Stage 2 뒤 |
| aarch64에서 링크와 probe 실행 | 이번에는 컴파일까지만. 링크는 x86-64 `.S` 제외가 필요하고, 그것은 Stage 1의 CMake 변경입니다 |

## 5. 다음에 필요한 것

1. 설계의 결정 항목 1, 4, 5, 6, 8번에 대한 사용자 답.
2. Stage 1 설계와 작업 지시: CMake `CMAKE_SYSTEM_PROCESSOR` 술어, aarch64 실패 stub,
   `scripts/build_linux_aarch64.sh`, aarch64에서 `repiu_core_probe`와 `repiu_instruction_census` 실행,
   Android Gradle 골격, 주소 공간 probe.
3. 결정 3(x87 표현)은 Stage 2 이전까지.

## 6. 재현

toolchain 파일을 하나 만들고 configure합니다. 저장소를 수정하지 않습니다.

```cmake
# aarch64-linux-gnu.cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_ASM_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
```

```bash
sudo apt-get install -y g++-aarch64-linux-gnu
cmake -S . -B build/linux_aarch64_probe -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/path/to/aarch64-linux-gnu.cmake \
    -DSDL_UNIX_CONSOLE_BUILD=ON -DSDL_X11=OFF -DSDL_WAYLAND=OFF \
    -DSDL_PULSEAUDIO=OFF -DSDL_ALSA=OFF -DSDL_X11_XSCRNSAVER=OFF -DSDL_X11_XTEST=OFF
cmake --build build/linux_aarch64_probe --target repiu_exe --parallel 4 -- -k 0
```

`-k 0`이 실패 뒤에도 계속 빌드하므로, 실패한 다섯 개가 한 번에 이름으로 나옵니다.

---

# Android arm64 port frontier

Design: [20260928-756](../design/20260928-756-android-arm64-execution.md) ·
Work order: [20260928-756](../work-orders/20260928-756-android-arm64-preparation.md) ·
Work log: [20260928-756](../work-logs/20260928-756-android-arm64-preparation.md)

This document keeps only **how far the Android arm64 port has come and what it needs next**. The
evidence per stage is in the work logs, and the stage plan and decision items are in the design.
Labels follow this directory's rule: **confirmed**, **inferred**, **unresolved**.

## 1. State: Stage 0 done, awaiting decisions (2026-09-28)

**Measurement and design are done, and no code changed.** Stage 1 (the build configuration) can start
once decision items 1, 4, 5, 6 and 8 of the design's thirteen have answers.

**The game does not run on Android.** Both present backends (`legacy`, `dynamic`) stand on an x86
host, and on arm64 not one guest byte can be copied.

## 2. Confirmed: what stands today

Measurement environment: an x86-64 Ubuntu 24.04 container, `g++-aarch64-linux-gnu` 13.2, CMake 3.28,
Ninja. No repository changes.

| Item | Value | Evidence |
|---|---|---|
| aarch64-linux-gnu cross configure | **passes** (SDL3 3.4.10 headless, spdlog 1.14.1, libchdr, Zydis, imgui, minimp3) | Task 756 |
| `repiu_exe` objects | **178 of 183 compile** | Task 756 |
| Failure causes | 1 ucontext conversion, 4 x86-64 `.S` files. All host-CPU-only | Task 756 |
| Execution engine (`src/engine/`) compiles | **all of it**. x86-only parts sit behind `#if` with an "unsupported" branch | Task 756 |
| What would run on aarch64 | `IsDirectX86ExecutionSupported()` and `IsCodeCacheEntrySupported()` both false, so `RunExecutionThread` ends with "requires a 32-bit host" (from reading the code) | Task 756 |
| CMake's architecture test | `CMAKE_SIZEOF_VOID_P` only; `CMAKE_SYSTEM_PROCESSOR` is checked nowhere | Task 756 |
| `__aarch64__` branches | 1 (`src/platform/build_identity.cpp`) | Task 756 |
| `__ANDROID__` branches | 0 | Task 756 |
| Identity casts using a guest address as a host pointer | 26 (`src/hle`, `src/runtime`, `src/engine`) | Task 756 |
| Desktop GL fixed-function calls | 41 (`glide_opengl_backend.cpp`) | Task 756 |
| GLSL | 2 × `#version 110`, launcher `#version 130` | Task 756 |
| SDL3 minimum Android API | 21 | SDL3 3.4.10 `docs/README-android.md` |
| SDL3 `android-project` template | NDK 28.2, compileSdk 35, `arm64-v8a`, CMake externalNativeBuild | same source |
| Android 16 KB pages | from Android 15, default alignment from NDK r28, Play requirement 2027-02-01 for API 35+ targets | [Android docs](https://developer.android.com/guide/practices/page-sizes) |
| Size the interpreter must implement | 320 instruction forms, top 17 mnemonics 87.89% | Task 514 |
| Reference on Linux x64 | all 16 ROM sets with a disc run | v0.0.194 |

The actual compile failures:

```
src/platform/linux/guest_cpu_context.cpp:26:47: error: 'const struct mcontext_t'
    has no member named 'gregs'; did you mean 'regs'?
src/platform/linux/aot_dbt_return_thunk_x64.S:18: Error: unknown pseudo-op: `.intel_syntax'
src/platform/linux/aot_dbt_return_thunk_x64.S:24: Error: expected a register or
    register list at operand 1 -- `mov qword ptr[rip+repiu_linux_x64_return_thunk_rsp],rsp'
```

## 3. Inferred

* **An interpreter may reach playable speed.** The guest is a 1999 DOS game and a modern arm64 core
  is tens of times faster than that era's CPUs. Unmeasured, so inferred.
* **Placing 134 MB from `0x00010000` should hold in an Android app process.** `mmap_min_addr` is
  usually `0x8000` and a 64-bit process's executable and libraries sit far higher. The zygote's and
  ART's claims vary by device, so the Stage 1 probe settles it.

## 4. Unresolved

| Item | How to confirm |
|---|---|
| Low-4 GiB placement in an Android app process | Stage 1 probe app, on 4 KB and 16 KB page devices |
| API levels of bionic's `pthread_timedjoin_np`, `process_vm_readv`, `posix_spawn` | Compare against `__INTRODUCED_IN` in the NDK headers. This container could not reach the bionic source |
| RX anonymous memory (`execmem`) in an app process | Needed for Stage 4 only |
| ART `libsigchain` versus our own signal handlers | The interpreter path needs no fault delivery |
| Interpreter speed | After Stage 2 |
| Linking and running the probes on aarch64 | This task stopped at compilation. Linking needs the x86-64 `.S` files excluded, which is Stage 1's CMake change |

## 5. What is needed next

1. The user's answers to decision items 1, 4, 5, 6 and 8 in the design.
2. A Stage 1 design and work order: the CMake `CMAKE_SYSTEM_PROCESSOR` predicate, the aarch64 failing
   stubs, `scripts/build_linux_aarch64.sh`, `repiu_core_probe` and `repiu_instruction_census` running
   on aarch64, the Android Gradle skeleton, the address-space probe.
3. Decision 3 (the x87 representation) before Stage 2.

## 6. Reproducing

Write one toolchain file and configure. The repository is not modified.

```cmake
# aarch64-linux-gnu.cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_ASM_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
```

```bash
sudo apt-get install -y g++-aarch64-linux-gnu
cmake -S . -B build/linux_aarch64_probe -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/path/to/aarch64-linux-gnu.cmake \
    -DSDL_UNIX_CONSOLE_BUILD=ON -DSDL_X11=OFF -DSDL_WAYLAND=OFF \
    -DSDL_PULSEAUDIO=OFF -DSDL_ALSA=OFF -DSDL_X11_XSCRNSAVER=OFF -DSDL_X11_XTEST=OFF
cmake --build build/linux_aarch64_probe --target repiu_exe --parallel 4 -- -k 0
```

`-k 0` keeps building past failures, so all five failing units are named in one run.
