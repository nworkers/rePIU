# 20260831-547 Linux x64 frame/ABI probe 작업 지시서

## 한국어

### 목적

Task 546의 x64 실행 모델을 첫 코드 단위로 고정합니다. guest의 32비트 register
state와 host pointer를 이름 있는 frame으로 표현하고, 실제 guest를 실행하지 않는
synthetic SysV AMD64 probe로 resolver 호출 계약을 검증합니다.

### 작업

- Linux x64 전용 dispatch frame header와 오프셋 assertion 추가
- guest word는 32비트, host pointer는 64비트인 구조 검증
- callee-saved register, 16-byte call-site stack alignment, XMM state 복원을
  검사하는 synthetic assembly probe 추가
- `repiu_core_probe`에서 Linux x64에만 해당 probe를 포함

### 비범위

- production AOT/DBT thunk
- 원본 guest block 실행
- x64 emitter 또는 signal fault resume
- i386/Win32 ABI 변경

### 검증

Linux x64에서 `repiu_core_probe`를 빌드·실행하고 frame/ABI 결과를 기록합니다.
Linux i386과 Win32에서는 새 x64 probe가 수집되지 않는지 확인합니다.

## English

### Objective

Make Task 546's x64 execution model concrete as the first code unit. Represent 32-bit
guest register state and host pointers in a named frame, then validate the resolver
call contract with a synthetic SysV AMD64 probe that never executes guest code.

### Work items

- Add a Linux x64 dispatch-frame header and offset assertions.
- Verify 32-bit guest words and 64-bit host pointers in the structure.
- Add a synthetic assembly probe for callee-saved registers, 16-byte call-site stack
  alignment, and XMM state restoration.
- Include the probe only in the Linux x64 `repiu_core_probe` target.

### Out of scope

- Production AOT/DBT thunks
- Original guest-block execution
- The x64 emitter or signal fault resumption
- i386 or Win32 ABI changes

### Verification

Build and run `repiu_core_probe` on Linux x64 and record the frame/ABI result. Confirm
that Linux i386 and Win32 do not collect the new x64 probe.
