# 20260831-546 Linux x64 AOT/DBT 실행 모델 작업 지시서

## 한국어

### 목적

Linux x64 실행 경로의 guest 상태, host pointer, dispatch frame, stack, emitter,
fault resume 계약을 분리하여 실제 구현이 기존 i386 경로를 손상하지 않도록 합니다.

### 작업 범위

- 기존 i386 전용 계약과 x64에서 새로 필요한 계약을 문서화합니다.
- x64 guest state와 host pointer의 폭을 구분하는 타입 방향을 정합니다.
- SysV AMD64 dispatch frame/bridge와 host RSP 유지 정책을 정합니다.
- raw `kCopy`와 x64 semantic re-encode의 경계를 정합니다.
- x64 fault 복원과 단계별 검증 순서를 정합니다.

### 구현 순서

1. x64 frame/type header 및 static assertion
2. synthetic x64 ABI bridge probe
3. 제한된 x64 AOT emitter subset
4. dispatch/fault 연결
5. DOS/4GW sample 상태 비교

### 비범위

- 이번 단위에서 x64 실행 코드를 바로 추가하지 않습니다.
- 원본 실행 파일과 기존 Linux i386 경로를 변경하지 않습니다.
- WSLg 그래픽 스택을 변경하지 않습니다.

### 완료 기준

설계 문서가 guest/host 주소와 stack/frame 계약을 명확히 구분하고, 다음 구현 단위가
독립적인 compile/probe 단계로 나뉘어야 합니다.

## English

### Objective

Separate guest state, host pointers, dispatch frames, stacks, emitters, and fault
resumption for Linux x64 so the implementation cannot silently damage the existing
i386 path.

### Scope

- Document current i386 contracts and the contracts required on x64.
- Define the width separation between x64 guest state and host pointers.
- Define the SysV AMD64 dispatch frame/bridge and host-RSP policy.
- Define the boundary between raw `kCopy` and x64 semantic re-encoding.
- Define x64 fault recovery and staged verification.

### Implementation order

1. x64 frame/type header and static assertions
2. Synthetic x64 ABI bridge probe
3. Restricted x64 AOT emitter subset
4. Dispatch/fault integration
5. DOS/4GW sample state comparison

### Out of scope

This unit adds no x64 execution code, changes neither the original executable nor the
existing Linux i386 path, and does not modify the WSLg graphics stack.

### Completion criteria

The design must distinguish guest/host addresses and stack/frame contracts clearly, and
the next implementation units must be independently buildable and testable.
