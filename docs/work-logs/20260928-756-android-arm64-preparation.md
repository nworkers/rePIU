# Task 756 작업 로그: Android arm64 이식 준비

설계: [20260928-756](../design/20260928-756-android-arm64-execution.md) ·
작업 지시: [20260928-756](../work-orders/20260928-756-android-arm64-preparation.md) ·
frontier: [android-arm64-port-frontier](../analysis/android-arm64-port-frontier.md)

## 요약

**Android arm64에서 게임을 실행하려면 새 실행 backend가 필요하고, 그것은 웹 이식이 보류해 둔 Stage 3
(플랫폼 중립 인터프리터)과 같은 것입니다.** 현재 backend 둘은 모두 x86 호스트 위에 서 있고, Linux
x64가 썼던 "바이트 대부분 그대로, 일부 재인코딩" 지름길은 arm64에 없습니다.

한편 **코어와 실행 엔진의 소스는 arm64에서 거의 전부 컴파일됩니다.** aarch64-linux-gnu 크로스
빌드에서 `repiu_exe` 오브젝트 183개 가운데 178개가 컴파일되고, 실패한 다섯은 전부 호스트 CPU 전용
파일(ucontext 변환 1개, x86-64 어셈블리 4개)입니다. 코어 소스는 한 줄도 바꾸지 않았습니다. Linux
x64 이식이 x86 전용 부분을 `#if`로 갈라 둔 결과입니다.

코드는 바꾸지 않았습니다. 설계, frontier, kb, 색인, TODO를 썼습니다.

## 측정

### 크로스 컴파일

| 항목 | 값 |
|---|---:|
| configure (SDL3 3.4.10 headless, spdlog, libchdr, Zydis, imgui, minimp3) | 통과, 38초 |
| `repiu_exe` 오브젝트 시도 / 성공 / 실패 | 183 / **178** / 5 |
| 코어 소스 수정 | 0줄 |
| 경고 | 1건 (`g_repiu_active_thread_context` extern 초기화, 기존 것) |

실패 다섯: `src/platform/linux/guest_cpu_context.cpp`(AArch64 `mcontext_t`에 `gregs` 없음),
`aot_dbt_return_thunk_x64.S`, `aot_dbt_glide_gate_thunk_x64.S`, `guest_stack_recover_x64.S`,
`guest_entry_x64.S`(Intel 문법 x86-64 어셈블리). CMake가 `CMAKE_SIZEOF_VOID_P GREATER 4`만 보고 x64
어셈블리를 포함시킨 것이 후자의 원인이며, `CMAKE_SYSTEM_PROCESSOR` 검사는 저장소에 없습니다.

링크와 probe 실행은 하지 않았습니다. 링크하려면 x64 `.S`를 제외하는 CMake 변경이 필요하고, 그것은
Stage 1의 코드 변경입니다. `qemu-user-static` 설치도 시도했으나 이 컨테이너의 apt 미러에서 404가
났고, 링크 없이는 쓸 곳이 없어 더 가지 않았습니다.

### 코드 조사

| 항목 | 값 |
|---|---:|
| 아키텍처 매크로 분기 최다 파일 | `execution_trampoline.cpp` 36, `linux/guest_cpu_context.cpp` 13, `linux/fault_handler.cpp` 13 |
| `__aarch64__` / `__ANDROID__` | 1 (이름표) / 0 |
| 게스트 주소를 호스트 포인터로 쓰는 identity cast | 26 |
| 데스크톱 GL 고정 기능 호출 (`glide_opengl_backend.cpp`) | 41 |
| GLSL | `#version 110` 2, 런처 `#version 130` 1 |
| Android 검증이 필요한 호스트 호출 | `pthread_timedjoin_np`, `process_vm_readv`, `posix_spawn`, `perf_event_open`, `MAP_FIXED_NOREPLACE`, `SIGRTMIN`, `/proc/self/maps` |

x64 실행 모델(Task 546)이 확정한 "게스트 주소 `uint32_t`, 호스트 포인터 `uintptr_t`, 게스트 arena는
하위 4 GiB `0x00010000`" 계약은 arm64에도 그대로 필요합니다. HLE가 게스트 주소를 호스트 포인터로
그대로 쓰기 때문입니다.

### 외부 사실

* SDL3 3.4.10: Android 최소 API 21, 템플릿은 NDK 28.2, compileSdk 35, `arm64-v8a`, CMake
  externalNativeBuild (configure가 받아온 소스에서 읽음).
* Android 16 KB 페이지: Android 15부터, NDK r28부터 기본, Play 요구는 2027-02-01부터 API 35 이상
  대상 앱 ([Android 문서](https://developer.android.com/guide/practices/page-sizes)).
* bionic 소스(android.googlesource.com)는 이 컨테이너의 네트워크 정책이 막아 읽지 못했습니다. API
  수준 확인은 미확정으로 남겼습니다.

## 결정 (설계에 상세)

1. Android arm64는 Linux 이식이 아니라 웹과 같은 부류이며, 인터프리터 먼저, arm64 DBT는 측정 뒤
   결정을 권고합니다.
2. 인터프리터는 웹 Stage 3과 하나입니다. 두 소비자를 전제로 설계합니다.
3. 게스트 주소 = 호스트 주소를 유지하고, Android 앱 프로세스의 하위 4 GiB 배치를 Stage 1 probe로
   먼저 잽니다.
4. 단계는 0(이 작업), 1(빌드 구성, Linux aarch64 먼저), 2(인터프리터), 3(Android 호스트: GLES,
   오디오, 입력, 자산), 4(arm64 DBT, 측정 뒤).
5. 사용자 결정 항목 13개를 설계에 표로 두었습니다. Stage 1 시작에는 1, 4, 5, 6, 8번이 필요합니다.
6. 사용자 질문에 따라 기존 x86 CPU 라이브러리(Unicorn, Bochs, DOSBox 계열, FEX, Box64 등)를 검토했고,
   라이선스와 경계 계약 때문에 본체는 자체 작성하고 SoftFloat 같은 부품만 받는다는 결론을 설계에
   보충했습니다. Linux aarch64 실행 환경 후보 표도 함께 넣었습니다.

## 만든 문서

* `docs/design/20260928-756-android-arm64-execution.md`
* `docs/work-orders/20260928-756-android-arm64-preparation.md`
* `docs/analysis/android-arm64-port-frontier.md` (색인 갱신)
* `docs/kb/aarch64-and-android-host-constraints.md` (색인 갱신)
* `docs/TODO.md` 활성 항목 추가
* 이 로그

## 검증

코드 변경이 없어 빌드 검증 대상이 없습니다. 크로스 컴파일 측정은 frontier의 재현 절대로 다시 낼
수 있습니다. `build/linux_aarch64_probe/`는 `.gitignore`의 `build/` 아래라 커밋에 들어가지 않습니다.

## 다음

사용자가 설계의 결정 항목에 답하면 Stage 1 설계와 작업 지시(Task 757)를 씁니다.

---

# Task 756 work log: preparing the Android arm64 port

Design: [20260928-756](../design/20260928-756-android-arm64-execution.md) ·
Work order: [20260928-756](../work-orders/20260928-756-android-arm64-preparation.md) ·
Frontier: [android-arm64-port-frontier](../analysis/android-arm64-port-frontier.md)

## Summary

**Running the game on Android arm64 needs a new execution backend, and it is the same thing the web
port left on hold as Stage 3, the platform-neutral interpreter.** Both present backends stand on an x86
host, and the shortcut Linux x64 used, "most bytes verbatim, a few re-encoded", does not exist on
arm64.

At the same time, **the core and the execution engine almost entirely compile for arm64.** In an
aarch64-linux-gnu cross build, 178 of `repiu_exe`'s 183 objects compile, and all five failures are
host-CPU-only files (one ucontext conversion, four x86-64 assembly files). Not one core source line
was changed. This is the result of the Linux x64 port fencing the x86-only parts behind `#if`.

No code was changed. The design, the frontier, the kb, the indexes and TODO were written.

## Measurements

### Cross compilation

| Item | Value |
|---|---:|
| configure (SDL3 3.4.10 headless, spdlog, libchdr, Zydis, imgui, minimp3) | passes, 38 s |
| `repiu_exe` objects attempted / compiled / failed | 183 / **178** / 5 |
| Core sources modified | 0 |
| Warnings | 1 (`g_repiu_active_thread_context` extern initialisation, pre-existing) |

The five failures: `src/platform/linux/guest_cpu_context.cpp` (AArch64 `mcontext_t` has no `gregs`),
`aot_dbt_return_thunk_x64.S`, `aot_dbt_glide_gate_thunk_x64.S`, `guest_stack_recover_x64.S`,
`guest_entry_x64.S` (Intel-syntax x86-64 assembly). The latter are included because CMake tests only
`CMAKE_SIZEOF_VOID_P GREATER 4`; no `CMAKE_SYSTEM_PROCESSOR` check exists in the repository.

Linking and running the probes were not attempted. Linking needs a CMake change that excludes the x64
`.S` files, which is Stage 1's code change. Installing `qemu-user-static` was tried too, but this
container's apt mirror returned 404, and without a link there was nothing to run under it.

### Code survey

| Item | Value |
|---|---:|
| Files with the most architecture-macro branches | `execution_trampoline.cpp` 36, `linux/guest_cpu_context.cpp` 13, `linux/fault_handler.cpp` 13 |
| `__aarch64__` / `__ANDROID__` | 1 (a label) / 0 |
| Identity casts using a guest address as a host pointer | 26 |
| Desktop GL fixed-function calls (`glide_opengl_backend.cpp`) | 41 |
| GLSL | 2 × `#version 110`, launcher 1 × `#version 130` |
| Host calls needing Android verification | `pthread_timedjoin_np`, `process_vm_readv`, `posix_spawn`, `perf_event_open`, `MAP_FIXED_NOREPLACE`, `SIGRTMIN`, `/proc/self/maps` |

The contract the x64 execution model (Task 546) settled, "guest addresses `uint32_t`, host pointers
`uintptr_t`, the guest arena in the low 4 GiB from `0x00010000`", is needed on arm64 unchanged,
because the HLE uses guest addresses as host pointers.

### External facts

* SDL3 3.4.10: minimum Android API 21; the template uses NDK 28.2, compileSdk 35, `arm64-v8a` and
  CMake externalNativeBuild (read from the source the configure fetched).
* Android 16 KB pages: from Android 15, default from NDK r28, required by Play from 2027-02-01 for apps
  targeting API 35 or higher ([Android docs](https://developer.android.com/guide/practices/page-sizes)).
* The bionic source (android.googlesource.com) was blocked by this container's network policy, so the
  API-level checks stay unresolved.

## Decisions (detailed in the design)

1. Android arm64 is the web's kind of problem, not the Linux port's: interpreter first, arm64 DBT
   decided after measuring, is the recommendation.
2. The interpreter is one with web Stage 3, designed for two consumers.
3. Guest address = host address stays, and low-4 GiB placement in an Android app process is measured
   first by a Stage 1 probe.
4. Stages: 0 (this task), 1 (build configuration, Linux aarch64 first), 2 (interpreter), 3 (Android
   host: GLES, audio, input, assets), 4 (arm64 DBT, after measuring).
5. Thirteen items for the user are tabled in the design. Items 1, 4, 5, 6 and 8 gate Stage 1.
6. At the user's question, existing x86 CPU libraries (Unicorn, Bochs, the DOSBox family, FEX, Box64 and
   others) were reviewed; the design gained a supplement concluding that the body is written in-house
   for license and boundary-contract reasons and only parts such as SoftFloat are taken, plus a table
   of Linux aarch64 environment candidates.

## Documents created

* `docs/design/20260928-756-android-arm64-execution.md`
* `docs/work-orders/20260928-756-android-arm64-preparation.md`
* `docs/analysis/android-arm64-port-frontier.md` (index updated)
* `docs/kb/aarch64-and-android-host-constraints.md` (index updated)
* An active item in `docs/TODO.md`
* This log

## Verification

No code changed, so there is nothing to build-verify. The cross-compile measurement can be reproduced
from the frontier's reproduction section. `build/linux_aarch64_probe/` sits under `.gitignore`'s
`build/` and is not committed.

## Next

Once the user answers the design's decision items, the Stage 1 design and work order (Task 757)
follow.
