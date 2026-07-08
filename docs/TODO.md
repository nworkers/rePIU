# TODO

## 현재 우선순위 상태

2026-07-08 기준으로 이전 TODO/PLAN 잔여 작업은 `docs/20260708-todo-plan-results.md`에 정리했다.

1. Skipped relocation 10개 상세 분석: 완료. dry-run 상세 목록과 analyzer 출력 추가.
2. guest stack 전환 trampoline 설계: 완료. 결과 문서에 장기 trampoline 순서 정리.
3. INT/DPMI/HLE trap 진입 방식 설계: 완료. 결과 문서에 SEH/HLE dispatcher 방향 정리.
4. 원본 entry 최소 실행 예외 분석: 완료. `0x020F3890` / `0xC0000096`을 privileged instruction trap 후보로 분류.
5. Win32 object protection 정책 정밀화: 완료. LE flags 기반 정책 유지 및 비 Win32 unsupported stub 추가.
6. relocated image runtime memory manager 승격: 완료. 장기 manager 입력 데이터와 다음 확장 범위 정리.

## 구현 보완 완료 상태

1. 예외 EIP 주변 relocated image byte window 기능: 완료.
2. guest context 구조체와 guest stack switch plan 구조: 완료.
3. HLE dispatcher table 초안: 완료.
4. selector/descriptor table 최소 모델: 완료.

## 남은 실제 구현 작업

1. Win32 x86 assembly 기반 실제 ESP 전환 trampoline 구현.
2. HLE dispatcher handler 호출 규약과 guest context 복귀 경로 구현.
3. INT21/INT31 중 실제 trace로 확인된 서비스부터 최소 구현.
4. selector/descriptor 권한 검사와 DPMI descriptor API 연결.

## Current Priority Status

As of 2026-07-08, the previous TODO/PLAN remaining work is summarized in `docs/20260708-todo-plan-results.md`.

1. Detailed analysis of the 10 skipped relocations: complete. Added dry-run detail list and analyzer output.
2. Guest stack switching trampoline design: complete. Recorded the long-running trampoline sequence in the result document.
3. INT/DPMI/HLE trap entry design: complete. Recorded the SEH/HLE dispatcher direction in the result document.
4. Minimal original entry exception analysis: complete. Classified `0x020F3890` / `0xC0000096` as a privileged-instruction trap candidate.
5. Win32 object protection policy refinement: complete. Kept LE flags policy and added non-Win32 unsupported stubs.
6. Relocated image runtime memory manager promotion: complete. Recorded long-running manager input data and next extension scope.

## Implementation Follow-up Completion Status

1. Relocated image byte window around exception EIP: complete.
2. Guest context and guest stack switch plan structures: complete.
3. HLE dispatcher table draft: complete.
4. Minimal selector/descriptor table model: complete.

## Remaining Real Implementation Work

1. Implement the actual Win32 x86 assembly ESP-switching trampoline.
2. Implement the HLE dispatcher handler calling convention and guest context return path.
3. Implement only the INT21/INT31 services confirmed by actual traces.
4. Connect selector/descriptor permission checks and DPMI descriptor APIs.
