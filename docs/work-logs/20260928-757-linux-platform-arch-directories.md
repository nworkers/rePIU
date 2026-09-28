# Task 757 작업 로그 — Linux 플랫폼 계층의 공용·x86·x64 분리

설계: [20260928-757](../design/20260928-757-linux-platform-arch-directories.md)
작업 지시: [20260928-757](../work-orders/20260928-757-linux-platform-arch-directories.md)

## 한국어

### 요약

`src/platform/linux/`에는 i386과 x86-64가 함께 쓰는 코드만 남기고, 아키텍처에 따라 갈리는 코드는
`x86/`과 `x64/`로 옮겼습니다. 파일 단위로 전용인 8개는 이름을 그대로 두고 옮겼고, 한 파일 안에 분기가 섞여
있던 `guest_cpu_context.cpp`와 `fault_handler.cpp`는 공용 부분과 아키텍처 부분으로 나눴습니다. **동작은
바꾸지 않았습니다.** 변경 전후로 Linux i386·x64 `repiu_core_probe`의 결과 줄이 모두 같고, 처리되지 않은
fault의 보고 줄은 두 아키텍처에서 필드 순서까지 같습니다.

### 결과 구조

| 위치 | 파일 |
|---|---|
| `src/platform/linux/` | `host_environment`, `host_process`, `safe_memory_copy`, `virtual_memory`, `worker_signal`, `guest_cpu_context`(공용: `ReadGuestFaultInfo`), `fault_handler`(공용 흐름), `fault_handler_arch.h`(내부), `fault_report_writer.h/.cpp`(내부) |
| `src/platform/linux/x86/` | `guest_cpu_context.cpp`, `fault_handler_arch.cpp`, `aot_dbt_dispatch_thunks.S`, `guest_stack_switch.S`, `stack_bridge.inc.S` |
| `src/platform/linux/x64/` | `guest_cpu_context.cpp`, `fault_handler_arch.cpp`, `aot_dbt_dispatch_x64.cpp`, `aot_dbt_return_thunk_x64.S`, `aot_dbt_glide_gate_thunk_x64.S`, `guest_stack_recover_x64.S`, `guest_entry_x64.S` |

`src/host/linux/main.cpp`는 아키텍처 분기가 없어 그대로 두었습니다.

### 구현

* 8개 전용 파일은 `git mv`로 옮겼습니다(이력 유지). `stack_bridge_probe.S`의 상대 include만 `x86/`을
  가리키도록 고쳤습니다.
* `fault_handler.cpp`는 원본에서 **줄 범위를 그대로 떼어** 나눴습니다(분리 스크립트 사용). 원본의 모든 줄이 새
  파일 어딘가에 있는지 기계적으로 대조했고, 빠진 줄은 의도한 것뿐이었습니다: `#if`/`#endif`, 공개 API
  `ReadHostInstructionPointer`로 바꾼 내부 `HostInstructionPointer` 호출 5곳, x86 파일에 다시 쓴 i386 분기,
  `HandleArchDiagnosticTrap`으로 옮긴 data watch 호출.
* 아키텍처 파일은 첫머리에서 `#error`로 자기 아키텍처를 확인합니다. 이전의 `#else` 분기(`false` 반환)는
  아키텍처 파일에서 사라졌습니다.
* 원본의 `volatile` 증가 경고(`++g_data_watch_hits`, `-Wvolatile`)는 원본 줄을 그대로 옮긴 것이라 그대로
  남았습니다.

### 검증

| 항목 | 결과 |
|---|---|
| 변경 전 기준 | main(`3ebc35f`)을 WSL 로컬 디스크에 clone, Linux i386·x64 Debug로 `repiu_core_probe`와 `repiu` 빌드(각 827 s, 797 s), probe 종료 코드 0, 두 구성 모두 `core_probe_all=true` |
| 변경 후 | 같은 clone을 작업 브랜치로 바꿔 같은 빌드 디렉터리에서 증분 빌드(121 s, 129 s), probe 종료 코드 0 |
| probe 결과 줄 비교 | `=true`/`=false` 줄을 16진·10진 값만 가리고 diff: **i386·x64 모두 동일**. `guest_cpu_context_all`, `fault_handler_all`, `stack_bridge_all`, `guest_stack_switch_all`(i386), `linux_x64_glide_gate_thunk … x87_survived=true`(x64), `core_probe_all=true` |
| 문법 검사 | 새 파일 7개를 `-m32`와 `-m64`로 `-fsyntax-only -Wall -Wextra`: 새 경고 없음 |
| `#error` 가드 | `x86/guest_cpu_context.cpp`를 `-m64`로, `x64/fault_handler_arch.cpp`를 `-m32`로 컴파일하면 각 `#error`에서 멈춤 |
| 처리되지 않은 fault 보고 | 변경 전후 fault handler 소스를 단독 테스트(`int3` 두 번 재개 후 `0x10`에 쓰기)로 각각 빌드해 비교: 보고 줄 필드 이름 순서가 i386 24개, x64 46개로 **동일**. 두 쪽 모두 `breaks=2`, x64에서 `REPIU_LINUX_X64_SIGNAL_BOUNDARY_TRACE=1`일 때 `last_signal=0x5 last_kind=0x2`로 같음 |
| x64 data watch | 전역 변수에 `REPIU_LINUX_X64_DATA_WATCH`를 걸고 두 번 쓰기: 변경 전후 모두 `armed`, `hit=0x1`, `hit=0x2`, 정상 종료 |
| Win32 x86 Debug | `build_win32_x86.ps1 -Configuration Debug` 증분 빌드 성공(종료 코드 0, CMake 재구성과 주석이 바뀐 엔진 파일 5개 포함) |

값을 가린 이유: 주소·스택 값은 ASLR과 코드 배치 때문에 실행마다 달라집니다.

### 결정 4 — 공개 헤더 (사용자 요구로 추가)

사용자가 `include/`에도 같은 규칙을 적용하도록 정해, x64 공개 헤더 4개(`linux_x64_aot_dispatch.h`,
`linux_x64_aot_frame.h`, `linux_x64_guest_entry.h`, `linux_x64_guest_registers.h`)를
`include/repiu/platform/linux/x64/`로 `git mv`했습니다. 이름과 include guard는 그대로이고, include 줄 16곳(소스
12개 파일, 헤더 1개, 분석 문서 4곳)을 새 경로로 고쳤습니다. `AGENTS.md`, `docs/CODING_STYLE.md`,
`ARCHITECTURE.md`의 규칙 문장이 공개 헤더도 다루도록 넓혔습니다. 엔진 계층의
`include/repiu/engine/linux_x64_transfer_failure_provenance.h`는 `src/engine/`의 짝과 함께 옮기지 않았습니다.

| 항목 | 결과 |
|---|---|
| Linux i386·x64 Debug | 증분 빌드(각 46 s), probe 종료 코드 0, `=true`/`=false` 결과 줄이 **변경 전 기준과 동일**, 두 구성 모두 `core_probe_all=true` |
| Win32 x86 Debug | 옮긴 헤더를 include하는 엔진 공용 소스까지 다시 컴파일해 성공(종료 코드 0). 로그의 C4819(코드 페이지) 경고는 이 작업과 무관한 기존 헤더에서 나는 것 |

### 확인하지 못한 것

* 게임 실행. 이 작업은 코드 위치와 파일 경계만 바꿨고, 위 probe와 단독 테스트가 옮긴 경로(machine context 변환,
  x87 태그, 재개, 보고, data watch)를 모두 지나갑니다.

## English

### Summary

`src/platform/linux/` now keeps only code shared by i386 and x86-64, and code that differs by architecture
moved to `x86/` and `x64/`. The eight architecture-only files moved with their names unchanged, and the two
files with mixed branches, `guest_cpu_context.cpp` and `fault_handler.cpp`, were split into a shared part
and per-architecture parts. **Behaviour is unchanged.** The Linux i386 and x64 `repiu_core_probe` result
lines are identical before and after, and the unhandled-fault report line keeps the same fields in the same
order on both architectures.

### Resulting layout

| Location | Files |
|---|---|
| `src/platform/linux/` | `host_environment`, `host_process`, `safe_memory_copy`, `virtual_memory`, `worker_signal`, `guest_cpu_context` (shared: `ReadGuestFaultInfo`), `fault_handler` (shared flow), `fault_handler_arch.h` (internal), `fault_report_writer.h/.cpp` (internal) |
| `src/platform/linux/x86/` | `guest_cpu_context.cpp`, `fault_handler_arch.cpp`, `aot_dbt_dispatch_thunks.S`, `guest_stack_switch.S`, `stack_bridge.inc.S` |
| `src/platform/linux/x64/` | `guest_cpu_context.cpp`, `fault_handler_arch.cpp`, `aot_dbt_dispatch_x64.cpp`, `aot_dbt_return_thunk_x64.S`, `aot_dbt_glide_gate_thunk_x64.S`, `guest_stack_recover_x64.S`, `guest_entry_x64.S` |

`src/host/linux/main.cpp` has no architecture branch and stays where it is.

### Implementation

* The eight architecture-only files moved with `git mv` (history kept). Only the relative include in
  `stack_bridge_probe.S` was pointed at `x86/`.
* `fault_handler.cpp` was split by **copying line ranges from the original** with a split script. Every
  original line was checked mechanically against the new files; the only missing lines were intended:
  `#if`/`#endif`, five calls to the internal `HostInstructionPointer` replaced by the public
  `ReadHostInstructionPointer`, the i386 branches rewritten in the x86 file, and the data-watch call moved
  into `HandleArchDiagnosticTrap`.
* Architecture files check their architecture with `#error` at the top. The old `#else` branches (returning
  `false`) are gone from the architecture files.
* The original `volatile` increment warning (`++g_data_watch_hits`, `-Wvolatile`) remains, because the line
  moved as it was.

### Verification

| Item | Result |
|---|---|
| Baseline | main (`3ebc35f`) cloned onto WSL's local disk; Linux i386 and x64 Debug builds of `repiu_core_probe` and `repiu` (827 s and 797 s); probes exited 0, both `core_probe_all=true` |
| After | The same clone switched to the task branch and rebuilt incrementally in the same build directories (121 s, 129 s); probes exited 0 |
| Probe result lines | `=true`/`=false` lines diffed with hex and decimal values masked: **identical on i386 and x64**. `guest_cpu_context_all`, `fault_handler_all`, `stack_bridge_all`, `guest_stack_switch_all` (i386), `linux_x64_glide_gate_thunk … x87_survived=true` (x64), `core_probe_all=true` |
| Syntax check | The seven new files with `-m32` and `-m64`, `-fsyntax-only -Wall -Wextra`: no new warnings |
| `#error` guard | Compiling `x86/guest_cpu_context.cpp` with `-m64` and `x64/fault_handler_arch.cpp` with `-m32` stops at each `#error` |
| Unhandled-fault report | The fault handler sources before and after, each built into a standalone test (resume two `int3`s, then write to `0x10`): the report's field names are **identical** in order, 24 on i386 and 46 on x64. Both give `breaks=2`, and on x64 with `REPIU_LINUX_X64_SIGNAL_BOUNDARY_TRACE=1` both give `last_signal=0x5 last_kind=0x2` |
| x64 data watch | `REPIU_LINUX_X64_DATA_WATCH` on a global written twice: before and after both show `armed`, `hit=0x1`, `hit=0x2` and a normal exit |
| Win32 x86 Debug | `build_win32_x86.ps1 -Configuration Debug` incremental build succeeded (exit 0, including the CMake reconfigure and the five engine files whose comments changed) |

Values were masked because addresses and stack contents change from run to run with ASLR and code layout.

### Decision 4 — Public headers (added at the user's request)

The user decided that `include/` follows the same rule, so the four x64 public headers
(`linux_x64_aot_dispatch.h`, `linux_x64_aot_frame.h`, `linux_x64_guest_entry.h`,
`linux_x64_guest_registers.h`) moved with `git mv` to `include/repiu/platform/linux/x64/`. Names and include
guards are unchanged, and 16 include lines (12 source files, one header, four analysis-document paths) now
use the new path. The rule text in `AGENTS.md`, `docs/CODING_STYLE.md` and `ARCHITECTURE.md` now covers
public headers. The engine-layer `include/repiu/engine/linux_x64_transfer_failure_provenance.h` stays with its
`src/engine/` counterpart.

| Item | Result |
|---|---|
| Linux i386 and x64 Debug | Incremental builds (46 s each), probes exited 0, `=true`/`=false` result lines **identical to the pre-change baseline**, both `core_probe_all=true` |
| Win32 x86 Debug | Rebuilt through the shared engine sources that include the moved headers, succeeded (exit 0). The C4819 (code page) warnings in the log come from existing headers unrelated to this task |

### Not confirmed

* A game run. This task changed only where code lives and file boundaries, and the probes and standalone
  tests above pass through every moved path (machine-context conversion, x87 tags, resume, report, data
  watch).
