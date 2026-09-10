# 20260910-651 작업 로그: Linux x64 legacy stack run 완결

설계: [20260910-651 설계](../design/20260910-651-linux-x64-aot-boundary-stack-drain.md)  
작업 지시: [20260910-651 작업 지시](../work-orders/20260910-651-linux-x64-aot-boundary-stack-drain.md)  
분석: [linux-port-frontier 3.88](../analysis/linux-port-frontier.md)

## 한국어

### 결과

1. x64 legacy stack-run helper에 일반 및 segment PUSH/POP에 이어지는 직접형 `SUB ESP, imm8` (0x83 0xEC) 및 `SUB ESP, imm32` (0x81 0xEC) 지원을 추가했습니다. 32-bit 모듈로 연산으로 guest ESP를 갱신하고 `SetCompareFlags`를 통해 산술 flag를 유지합니다.
2. segment HLE 이후 후속 stack 명령 drain을 AOT 상태 flag(`aot_legacy_fallback`) 조건과 무관하게 허용하고, opcode `0F` directed dispatch에 POP FS/GS를 포함하여 epilogue의 segment 및 general POP run이 단일 HLE 경계에서 완결되도록 했습니다.
3. Win32 호스트 빌드 시 `GuestCpuContext` 전방선언으로 인해 발생하던 CONTEXT alias 불완전 형식 충돌 문제를 해결하기 위해 `guest_write_trace.h`와 `guest_address_watch.h`에서 `repiu/platform/guest_cpu_context.h`를 직접 include하도록 정리했습니다.

### 검증

1. synthetic general stack probe(`RunGeneralStackProbe`)에 PUSH run 뒤 `SUB ESP, 4` 처리 및 플래그 검증, 그리고 상태 flag에 무관한 segment/general POP epilogue drain 검증을 추가하여 정상 동작을 확인했습니다.
2. Debug 빌드 및 `repiu_core_probe`를 실행하여 전체 25개 core probe가 실패 없이 통과함을 확인했습니다.

## English

### Result

1. Added support for direct `SUB ESP, imm8` (0x83 0xEC) and `SUB ESP, imm32` (0x81 0xEC) following general and segment PUSH/POP sequences to the x64 legacy stack-run helper. Updates guest ESP via 32-bit modular arithmetic and synchronizes arithmetic flags using `SetCompareFlags`.
2. Relaxed the post-segment-HLE stack drain to run independently of AOT state flags (`aot_legacy_fallback`), and extended opcode `0F` directed dispatch to cover POP FS/GS so epilogue segment and general POP runs complete within a single HLE boundary.
3. Fixed Win32 build compilation issues caused by forward declarations of `GuestCpuContext` conflicting with the CONTEXT alias by directly including `repiu/platform/guest_cpu_context.h` in `guest_write_trace.h` and `guest_address_watch.h`.

### Verification

1. Extended `RunGeneralStackProbe` to verify bounded general/segment PUSH followed by `SUB ESP, 4` with flag validation, as well as flag-independent segment/general POP epilogue draining.
2. Verified Debug build and executed `repiu_core_probe`, confirming all 25 core probes pass with 0 failures.
