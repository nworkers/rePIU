# TODO

## 현재 우선순위

1. Skipped relocation 10개 상세 분석
2. guest stack 전환 trampoline 설계
3. INT/DPMI/HLE trap 진입 방식 설계
4. 원본 entry 최소 실행 시도 결과 분석: `0x020F3890`에서 `0xC0000096` privileged instruction 예외 발생
5. Win32 object protection 정책 정밀화
6. relocated image를 장기 실행 가능한 runtime memory manager로 승격

## Current Priorities

1. Analyze the 10 skipped relocations in detail.
2. Design a guest stack switching trampoline.
3. Design INT/DPMI/HLE trap entry.
4. Analyze the minimal original entry execution result: privileged instruction exception `0xC0000096` at `0x020F3890`.
5. Refine Win32 object protection policy.
6. Promote relocated image placement into a long-running runtime memory manager.
