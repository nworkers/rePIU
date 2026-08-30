# 20260831-548 Linux x64 ucontext adapter 작업 지시서

## 한국어

### 목적

Linux x64 `ucontext_t`를 현재 32비트 guest context 계약으로 변환하여 POSIX
context probe가 x64에서 컴파일·실행되도록 합니다.

### 작업

- x64 GPR/RIP/RSP/EFLAGS 매핑 추가
- packed CS/GS/FS 읽기 추가 및 segment write-back 금지
- x64 FXSAVE x87 register bytes/tag 변환 추가
- guest context probe의 i386/x64 stack register 분기 추가
- x64 native guest 실행은 계속 unsupported로 유지

### 검증

Linux x64 `repiu_core_probe`를 빌드·실행하고 `guest_cpu_context_all=true`와
`linux_x64_aot_frame_all=true`를 확인합니다. Linux i386 `repiu_exe`와
`repiu_core_probe`도 회귀 빌드합니다. 단, 공통 core probe가 `pit_timer`에서
멈추면 해당 hang을 먼저 분리하고 x64 전용 probe를 독립 실행합니다.

## English

### Objective

Convert Linux x64 `ucontext_t` into the existing 32-bit guest-context contract so the
POSIX context probe compiles and runs on x64.

### Work items

- Add x64 GPR/RIP/RSP/EFLAGS mapping.
- Read packed CS/GS/FS and keep segment write-back disabled.
- Add x64 FXSAVE x87 register-byte/tag conversion.
- Add an i386/x64 stack-register branch to the guest-context probe.
- Keep native x64 guest execution unsupported.

### Verification

Build and run the Linux x64 `repiu_core_probe` and confirm
`guest_cpu_context_all=true` and `linux_x64_aot_frame_all=true`. Rebuild the Linux
i386 executable and core-probe targets as regressions. If the shared core probe hangs
at `pit_timer`, isolate that hang and run the x64-only probes independently.
