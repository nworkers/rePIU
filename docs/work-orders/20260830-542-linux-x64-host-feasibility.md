# 20260830-542 Linux x64 host 전환 타당성 검토 작업 지시서

## 한국어

### 목적

WSLg 3D 가속을 활용하기 위해 Linux host를 x86-64로 전환할 수 있는지 검토합니다.
원본 32-bit DOS/4G guest 계약을 보존하면서, 현재 i386 native 실행 경로가 x64에서
어떤 방식으로 대체되어야 하는지 결정하는 것이 목적입니다.

### 작업 항목

1. 현재 build, signal context, assembly trampoline, AOT code-cache의 i386 의존성을
   목록화합니다.
2. x64 GL-only probe와 x64 compile probe의 최소 검증 범위를 정의합니다.
3. x64 AOT/DBT, 32-bit guest+x64 renderer IPC, i386 WSLg stack의 후보를 비교합니다.
4. guest 32-bit address와 host 64-bit pointer의 분리 원칙을 설계합니다.
5. 실제 코드 포팅 전에 다음 단계와 중단 기준을 기록합니다.

### 범위

- 이번 단위에서는 소스 코드와 실행 파일을 변경하지 않습니다.
- i386 Mesa/WSLg 패키지 설치는 하지 않습니다.
- 원본 guest를 64-bit instruction으로 변환하지 않습니다.

### 완료 기준

- x64 전환이 단순 `-m64` 빌드가 아님을 근거와 함께 기록합니다.
- 후보 아키텍처와 비용을 비교하고 권장 순서를 정합니다.
- 첫 구현 전에 필요한 graphics-only probe와 compile probe가 정의됩니다.

## English

### Objective

Evaluate whether the Linux host can move to x86-64 to use WSLg 3D acceleration. Preserve
the original 32-bit DOS/4G guest contract and determine how the current i386 native
execution path would have to be replaced on x64.

### Work items

1. Inventory i386 dependencies in the build, signal context, assembly trampoline, and AOT
   code cache.
2. Define the minimum checks for an x64 GL-only probe and an x64 compile probe.
3. Compare x64 AOT/DBT, a 32-bit guest plus x64 renderer IPC, and the i386 WSLg stack.
4. Design the separation between 32-bit guest addresses and 64-bit host pointers.
5. Record the next step and stop criteria before changing code.

### Scope

- Do not change source code or executables in this unit.
- Do not install i386 Mesa/WSLg packages.
- Do not translate the original guest into 64-bit instructions.

### Completion criteria

- Record, with evidence, why this is not a simple `-m64` rebuild.
- Compare candidate architectures and choose an ordering.
- Define the graphics-only probe and compile probe required before implementation.

## Probe result

The x64 CMake configuration succeeded and the build compiled the third-party
dependencies plus project sources until `include/repiu/engine/live_telemetry.h`.
It stopped at `static_assert(sizeof(long) == 4)` because Linux x86-64 uses an
8-byte `long`. This confirms the first ABI barrier and starts the next work unit:
normalize the fixed-layout telemetry fields to an explicit 32-bit type, then rerun
the x64 compile probe.
