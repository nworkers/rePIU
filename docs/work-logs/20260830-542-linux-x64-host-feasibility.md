# 20260830-542 Linux x64 host 전환 타당성 검토 작업 로그

## 한국어

### 조사 결과

현재 Linux 빌드 스크립트는 `-m32`를 C/C++/ASM 및 linker flag에 적용합니다.
실행 엔진은 `IsDirectX86ExecutionSupported()`와 `IsGuestStackSwitchSupported()`에서
i386만 지원하고, Linux signal adapter도 `__i386__`의 `REG_EIP`, `REG_ESP` 형태만
읽고 씁니다.

Linux GAS trampoline은 EAX/ESP/EBP, 32-bit stack frame, 32-bit cdecl/stdcall bridge를
직접 사용합니다. AOT code cache는 thunk·counter·cache pointer를 32-bit immediate로
patch하며, host allocation이 4 GiB를 넘으면 배치를 거부합니다. Linux link 정책도
non-PIE와 고정 code/guest address range를 사용합니다.

따라서 x64 host 전환은 graphics backend만 교체하는 작업이 아닙니다. 원본 guest를
같은 프로세스에서 직접 실행하려면 x86-64 host와 32-bit guest 사이의 진입·stack·fault
resume 경계를 새로 설계해야 합니다. 이 경계를 단순히 현재 i386 assembly를 유지한
채 x64로 빌드하는 방식은 성립하지 않습니다.

### 결정

64-bit Linux 전환은 장기 후보로 유지하되, 첫 구현 목표로 확정하지 않습니다.
먼저 다음 순서로 검증합니다.

1. x64 graphics-only GL probe로 WSLg D3D12 가속을 확인합니다.
2. `-m64` compile probe로 architecture barrier를 실제 compiler output으로 수집합니다.
3. native guest 보존이 필요한지와 x86-64 AOT/DBT 전환 비용을 비교합니다.
4. 그 결과에 따라 x64 AOT/DBT 또는 32-bit guest+x64 renderer IPC를 선택합니다.

현재 WSLg의 i386 D3D12 stack을 제공하는 경로가 발견되면 x64 전환의 긴급성은
낮아지지만, 현 환경에서는 해당 stack이 확인되지 않았으므로 x64 feasibility study는
계속 진행합니다.

### x64 compile probe

`build/linux_x64_probe`를 x64 CMake profile로 구성하고 `repiu_exe`를 `--parallel 2`로
빌드했습니다. SDL3·libchdr·Zydis 및 프로젝트의 선행 C++ 소스는 x86-64 compiler로
진행되었지만, 첫 프로젝트 소유 오류는 다음과 같았습니다.

```text
include/repiu/engine/live_telemetry.h:207:28: error: static assertion failed
static_assert(sizeof(long) == 4);
note: the comparison reduces to ‘(8 == 4)’
```

이는 `SharedLiveTelemetry`의 고정 shared-memory layout이 Linux x64에서 깨지는
첫 번째 ABI 장벽입니다. probe를 위해 source code는 변경하지 않았습니다.

### 변경 사항

- 소스 코드와 실행 파일은 변경하지 않았습니다.
- Linux x64 전환 설계, 작업 지시서, 작업 로그를 추가했습니다.
- x64 configure와 compile probe를 수행하고 첫 번째 ABI 오류를 기록했습니다.

## English

### Findings

The current Linux build script applies `-m32` to C, C++, assembly, and linker flags. The
execution engine accepts only i386 in `IsDirectX86ExecutionSupported()` and
`IsGuestStackSwitchSupported()`, while the Linux signal adapter reads and writes only the
`__i386__` `REG_EIP` and `REG_ESP` shape.

The Linux GAS trampoline directly uses EAX/ESP/EBP, 32-bit stack frames, and 32-bit
cdecl/stdcall bridges. The AOT code cache patches thunk, counter, and cache pointers as
32-bit immediates and rejects placements above 4 GiB. Linux linking also relies on non-PIE
and fixed code/guest address ranges.

An x64 host port is therefore not a graphics-backend-only change. To execute the original
guest directly in the same process, the entry, stack, and fault-resume boundaries between
an x86-64 host and a 32-bit guest would need a new design. Keeping the current i386
assembly and merely building it as x64 cannot work.

### Decision

Keep a 64-bit Linux port as a long-term candidate, but do not make it the first
implementation target yet. Verify, in order:

1. WSLg D3D12 acceleration with an x64 graphics-only GL probe.
2. Architecture barriers from an `-m64` compile probe.
3. The cost of preserving native guest execution versus moving to x86-64 AOT/DBT.
4. Either x64 AOT/DBT or a 32-bit guest plus x64 renderer IPC based on those results.

If a usable i386 WSLg D3D12 stack is found, the urgency of the x64 port decreases. The
stack is not present in the current environment, so the x64 feasibility study continues.

### x64 compile probe

`build/linux_x64_probe` was configured with an x64 CMake profile and `repiu_exe` was
built with `--parallel 2`. SDL3, libchdr, Zydis, and the preceding project C++ sources
ran through the x86-64 compiler, but the first project-owned error was:

```text
include/repiu/engine/live_telemetry.h:207:28: error: static assertion failed
static_assert(sizeof(long) == 4);
note: the comparison reduces to ‘(8 == 4)’
```

This is the first ABI barrier: Linux x64 changes the width of fields in the fixed
shared-memory `SharedLiveTelemetry` layout. No source code was changed for the probe.

### Changes

- No source code or executable was changed.
- Added the Linux x64 feasibility design, work order, and work log.
- Ran the x64 configure and compile probe and recorded the first ABI error.
