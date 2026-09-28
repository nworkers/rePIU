# Task 759: 엔진은 실행 모델로 나누고, OS 코드는 platform으로 옮긴다

## 한국어

### 배경

Task 758은 "모든 디렉터리 `D`에서 플랫폼 코드는 `D/<플랫폼>/`, 아키텍처 코드는 `D/<플랫폼>/<아키텍처>/`"라는
규칙으로 단계 1(플랫폼 계층)과 단계 2(엔진 지원 파일)를 마쳤다. 단계 3을 시작하기 전에 사용자가 방향을
확인하도록 했고, 확인 결과 규칙을 바꾸기로 했다.

플랫폼 디렉터리 밖에 남은 분기 162개(`src`·`include`, 도구 포함)를 모두 분류했다.

| 분류 | 개수 | 대략 줄 수 | 실제로 가르는 것 |
|---|---|---|---|
| 1. 실행 방식 | 35 | 1,650 | 게스트를 직접 실행하느냐, 번역 캐시로 실행하느냐 |
| 2. 기계어 thunk | 23 | 470 | CPU와 컴파일러(MSVC 인라인 어셈블리 대 `.S`) |
| 3. Win32 API | 24 | 590 | OS |
| 4. 한 호스트 전용 진단 | 5 | 400 | 그 호스트에서만 만든 추적 기능 |
| 5. 헤더 선택과 include | 9 | 20 | 어느 헤더를 고를지 |
| 도구와 probe | 69 | — | 약 30개는 probe 전체가 Win32에서만 도는 가드 |

엔진에서 아키텍처가 실제 이유인 것은 분류 1뿐이고, 그것은 "이 호스트는 게스트를 어떻게 실행하는가"라는 한
질문의 여러 면이다. 32비트 x86 호스트는 게스트의 바이트를 그대로 실행하고, x86-64 호스트는 long mode가 같은
바이트 일부를 다르게 읽으므로 번역한 코드 캐시를 실행한다. 단계 2가 아키텍처로 나눈 파일을 다시 보면 기능을
켤지 여부(`linexe_glide_boundary_arch`)나 값 하나(`host_architecture.h`)였다.

### 규칙

1. **엔진·런타임·HLE·도구 디렉터리에는 플랫폼·아키텍처 하위 디렉터리를 두지 않는다.**
2. **OS에 의존하는 코드는 플랫폼 계층에 둔다**: `src/platform/<OS>/[<아키텍처>/]`, 공개 계약은
   `include/repiu/platform/`. 플랫폼 계층은 엔진의 타입을 알지 못한다. 엔진은 주소·버퍼·콜백 같은 기본
   값으로 플랫폼 함수를 부르고, 엔진의 흐름과 엔진 타입을 다루는 코드는 엔진에 남는다.
3. **엔진의 아키텍처 차이는 실행 모델로 표현한다.** 실행 모델은 둘이다.
   * `direct`: 게스트의 바이트를 이 프로세스에서 그대로 실행한다(Win32, Linux i386).
   * `cache`: 번역한 long mode 코드 캐시를 실행한다(Linux x64).

   엔진은 내부 인터페이스 `include/repiu/runtime/execution_model.h`로 묻고, 구현은 공용 파일 옆에
   `<이름>_direct.cpp`, `<이름>_cache.cpp`로 둔다. CMake가 `REPIU_EXECUTION_MODEL`로 고른다(`REPIU_ARCH`가
   `x86`이면 `direct`, `x64`이면 `cache`). 모델 파일은 첫머리에서 `#error`로 자기 아키텍처를 확인한다.
4. **헤더의 선택 지점은 플랫폼 헤더에만 둔다**(Task 758의 예외를 그대로 쓴다).
5. **이 작업에서 건드리지 않는 것**: 한 호스트 전용 진단(분류 4)과 probe 전체 가드. 디렉터리로 나누면 "그
   호스트에서만 만들었다"는 임시 상태가 구조로 굳는다. 파일 전체가 한 호스트에서만 빌드되는 진단 파일은 원래
   자리에 두고 CMake가 고른다.

Task 757(플랫폼 계층 안의 `x86/`, `x64/`)과 Task 758 단계 1(플랫폼 계층의 OS별 분리)은 이 규칙과 맞으므로
그대로 둔다.

### 단계

| 단계 | 내용 |
|---|---|
| 1 | 규칙 문서(`AGENTS.md`, `docs/CODING_STYLE.md`, `ARCHITECTURE.md`)를 고치고, Task 758 단계 2가 엔진 안에 만든 디렉터리를 없앤다 |
| 2 | 분류 2: MSVC 인라인 어셈블리 thunk를 `src/platform/win32/`로 옮긴다(Linux i386 쪽은 이미 `src/platform/linux/x86/`에 있다) |
| 3 | 분류 1: `execution_model.h`를 채우고 엔진 핵심의 아키텍처 분기를 그 호출로 바꾼다. 파일 하나씩 |
| 4 | 분류 3: 엔진 핵심의 Win32 API 호출을 플랫폼 함수로 떼어 낸다 |

단계마다 빌드·검증·커밋한다.

#### 단계 1의 파일별 처리

| Task 758 단계 2의 결과 | 처리 |
|---|---|
| `include/repiu/platform/host_architecture.h`와 `x86/`, `x64/` 헤더 | 지우고 `execution_model::RunsGuestBytesDirectly()`로 바꾼다 |
| `boundary/x86/`·`boundary/linux/x64/`의 `linexe_glide_boundary_arch.cpp` | 지운다. "swap 대기 중 tick을 넣을 수 있는가"는 `execution_model::InjectsTicksDuringSwapWait()`가 답하고, 환경 변수 확인은 공용 파일로 돌아간다 |
| `x86/`·`linux/x64/`의 `native_phase_sampler_arch.cpp` | `src/engine/native_phase_sampler_direct.cpp`, `native_phase_sampler_cache.cpp`로 옮긴다 |
| `telemetry/linux/x64/linux_x64_native_write_trace.cpp` | 원래 자리 `src/engine/telemetry/`로 되돌린다(분류 4) |
| `win32/x87_context_win32.cpp`, `include/repiu/engine/win32/x87_context_win32.h` | 원래 자리로 되돌리고 헤더는 `GuestCpuContext`를 받게 한다. 이 함수는 호출하는 곳이 없다 |
| `telemetry/{win32,linux}/guest_write_trace_io*` | `platform::WriteHostTraceDescriptor`로 옮긴다 |
| `telemetry/{win32,linux}/live_telemetry_snapshot*`의 `ReadLoaderModuleRange`, `ReadHostThreadTimes` | `platform::ReadHostImageRange`, `platform::ReadHostThreadTimes`로 옮긴다 |
| `telemetry/win32/live_telemetry_snapshot_win32.cpp`의 나머지(공유 매핑, 정지한 스레드의 스냅샷) | 엔진 타입을 쓰는 엔진 코드다. OS 호출을 플랫폼 함수로 떼어 내고 흐름은 `live_telemetry_snapshot.cpp`에 둔다 |
| `telemetry/{win32,linux}/guest_position_census_symbols*` | 주소 하나를 이름으로 바꾸는 `platform::ResolveHostAddressSymbol`로 떼어 내고, 스냅샷을 도는 코드는 엔진에 둔다 |
| `exception/{win32,linux}/host_crash_report*` | 엔진 타입을 쓰지 않는다. `platform::InstallHostCrashReporter`로 통째로 옮긴다 |
| `{win32,linux}/glide_opengl_backend_wsl*` | 엔진 타입을 쓰지 않는다. `platform::SelectWslD3d12Driver`로 옮긴다 |
| `exception/win32/exception_rescue_win32.cpp` | 엔진의 `DispatchGuestException`을 부르는 Win32 진입점이다. 원래 자리로 되돌리고 단계 4에서 다룬다 |

### 동작 보존

코드는 옮기기만 하고 바꾸지 않는다. 함수 경계를 새로 만들 때 생기는 차이는 인라인이던 코드가 호출이 되는
것뿐이다. 예외 처리기 안에서 도는 코드에 새로 생기는 함수는 할당과 락을 더하지 않고, 조기 반환은 호출한
쪽에 그대로 전달한다.

### 검증 전략

사용자 지시에 따라 기본 검증은 probe다. 기준은 이 작업을 시작한 커밋(`a7ad129`)에서 모은다.

| 구성 | 확인 |
|---|---|
| Linux x64 Release·Debug, Linux i386 | 빌드, `repiu_core_probe`의 결과 줄(값은 가림)을 기준과 비교 |
| Win32 x86 Debug | 빌드, `repiu_core_probe`, `repiu_aot_probe`의 이미지 없는 모드 |
| 실행 | 엔진 핵심을 고친 단계(3, 4)에서만, Linux x64와 Win32에서 한 롬셋을 짧게 |

Web 빌드는 이 머신에 도구가 없으면 확인하지 못한다. 그 경우 작업 로그에 확인하지 못했다고 적는다.

## English

### Background

Task 758 finished its phase 1 (the platform layer) and phase 2 (engine support files) under the rule "in
any directory `D`, platform code goes in `D/<platform>/` and architecture code in
`D/<platform>/<arch>/`". Before phase 3 the user asked for the direction to be checked, and the rule is
changed as a result.

All 162 branches remaining outside the platform directories (`src` and `include`, tools included) were
classified.

| Class | Count | Lines, roughly | What really decides |
|---|---|---|---|
| 1. How the guest runs | 35 | 1,650 | running the guest directly, or running a translated cache |
| 2. Machine-code thunks | 23 | 470 | the CPU and the compiler (MSVC inline assembly against `.S`) |
| 3. Win32 API | 24 | 590 | the OS |
| 4. Diagnostics of one host | 5 | 400 | tracing built on that host alone |
| 5. Header selection and includes | 9 | 20 | which header to take |
| Tools and probes | 69 | — | about 30 are guards around a probe that runs on Win32 only |

In the engine, architecture is the real reason in class 1 alone, and that class is the several faces of
one question: how does this host run the guest? A 32-bit x86 host runs the guest's bytes as they are; an
x86-64 host runs a translated code cache, because long mode reads some of those bytes differently. The
files phase 2 split by architecture turn out to be whether a feature is on
(`linexe_glide_boundary_arch`) or a single value (`host_architecture.h`).

### Rule

1. **The engine, runtime, HLE and tool directories have no platform or architecture subdirectories.**
2. **Code that depends on the OS lives in the platform layer**: `src/platform/<OS>/[<arch>/]`, with its
   public contract in `include/repiu/platform/`. The platform layer knows none of the engine's types. The
   engine calls platform functions with plain values (addresses, buffers, callbacks), and code that holds
   the engine's flow or its types stays in the engine.
3. **The engine's difference by architecture is expressed as an execution model.** There are two.
   * `direct`: the guest's bytes run in this process as they are (Win32, Linux i386).
   * `cache`: the translated long-mode code cache runs (Linux x64).

   The engine asks through the internal interface `include/repiu/runtime/execution_model.h`, and
   implementations sit next to their shared file as `<name>_direct.cpp` and `<name>_cache.cpp`. CMake
   picks by `REPIU_EXECUTION_MODEL` (`direct` when `REPIU_ARCH` is `x86`, `cache` when `x64`). A model file
   checks its own architecture with `#error` at the top.
4. **Selection points in headers exist in platform headers only** (Task 758's exception, as it was).
5. **Left alone by this task**: diagnostics of one host (class 4) and guards around whole probes. Putting
   them in directories would turn "built on that host only", a temporary state, into structure. A
   diagnostic file built on one host only stays where it was and CMake picks it.

Task 757 (`x86/` and `x64/` inside the platform layer) and Task 758's phase 1 (the platform layer split by
OS) agree with this rule and stay.

### Phases

| Phase | What |
|---|---|
| 1 | Correct the rule documents (`AGENTS.md`, `docs/CODING_STYLE.md`, `ARCHITECTURE.md`) and remove the directories Task 758's phase 2 made inside the engine |
| 2 | Class 2: move the MSVC inline-assembly thunks to `src/platform/win32/` (the Linux i386 ones are already in `src/platform/linux/x86/`) |
| 3 | Class 1: fill `execution_model.h` and replace the engine core's architecture branches with calls to it, one file at a time |
| 4 | Class 3: take the engine core's Win32 API calls out into platform functions |

Each phase is built, verified and committed.

#### Phase 1, file by file

| Result of Task 758's phase 2 | What happens |
|---|---|
| `include/repiu/platform/host_architecture.h` and the `x86/`, `x64/` headers | removed; `execution_model::RunsGuestBytesDirectly()` instead |
| `linexe_glide_boundary_arch.cpp` in `boundary/x86/` and `boundary/linux/x64/` | removed. "Can ticks be injected during the swap wait" is answered by `execution_model::InjectsTicksDuringSwapWait()`, and the environment check returns to the shared file |
| `native_phase_sampler_arch.cpp` in `x86/` and `linux/x64/` | moved to `src/engine/native_phase_sampler_direct.cpp` and `native_phase_sampler_cache.cpp` |
| `telemetry/linux/x64/linux_x64_native_write_trace.cpp` | back where it was, `src/engine/telemetry/` (class 4) |
| `win32/x87_context_win32.cpp`, `include/repiu/engine/win32/x87_context_win32.h` | back where they were, the header taking `GuestCpuContext`. Nothing calls this function |
| `telemetry/{win32,linux}/guest_write_trace_io*` | becomes `platform::WriteHostTraceDescriptor` |
| `ReadLoaderModuleRange` and `ReadHostThreadTimes` in `telemetry/{win32,linux}/live_telemetry_snapshot*` | become `platform::ReadHostImageRange` and `platform::ReadHostThreadTimes` |
| The rest of `telemetry/win32/live_telemetry_snapshot_win32.cpp` (the shared mapping, the snapshot of a suspended thread) | engine code using engine types. Its OS calls are taken out into platform functions and the flow stays in `live_telemetry_snapshot.cpp` |
| `telemetry/{win32,linux}/guest_position_census_symbols*` | the step that names one address becomes `platform::ResolveHostAddressSymbol`; the loop over the snapshot stays in the engine |
| `exception/{win32,linux}/host_crash_report*` | uses no engine type. Moves whole as `platform::InstallHostCrashReporter` |
| `{win32,linux}/glide_opengl_backend_wsl*` | uses no engine type. Moves as `platform::SelectWslD3d12Driver` |
| `exception/win32/exception_rescue_win32.cpp` | the Win32 entry that calls the engine's `DispatchGuestException`. Back where it was, and handled in phase 4 |

### Preserving behaviour

Code moves; it does not change. A new function boundary adds nothing beyond inline code becoming a call.
A function introduced on a path that runs inside a fault handler adds no allocation or lock, and an early
return is handed to the caller as it was.

### Verification strategy

By the user's instruction the default check is the probes. The baseline is collected at the commit this
task starts from (`a7ad129`).

| Configuration | Check |
|---|---|
| Linux x64 Release and Debug, Linux i386 | build; `repiu_core_probe` result lines (values masked) against the baseline |
| Win32 x86 Debug | build; `repiu_core_probe`; the image-free modes of `repiu_aot_probe` |
| Runs | only in the phases that change the engine core (3 and 4): one ROM set, briefly, on Linux x64 and Win32 |

The web build cannot be checked if this machine has no tools for it; the work log then says so.
