# TODO

## 2026-07-09 segment register store HLE 진행

`66 26 8C 1D` segment register store 중단 지점은 guest selector shadow state를 relocated runtime memory에 쓰는 HLE 요구사항으로 분류되었다.
이번 작업에서는 이 지점을 직접 처리하여 다음 중단 지점을 관찰한다.

## 2026-07-09 Segment Register Store HLE Progress

The `66 26 8C 1D` segment-register store stop is classified as an HLE requirement that writes guest selector shadow state into relocated runtime memory.
This task handles that stop directly and observes the next execution stop.

## 2026-07-09 segment register store HLE 완료

`66 26 8C 1D` segment register store는 처리되었고, `DS=0x0024`가 relocated destination `0x020F3AED`에 기록되는 것을 확인했다.
다음 중단 지점은 `0x020F39C8`의 `66 8E 05 E4 65 1A 02` memory-source segment register load이다.

## 2026-07-09 Segment Register Store HLE Complete

`66 26 8C 1D` segment-register store is handled, and `DS=0x0024` is written to relocated destination `0x020F3AED`.
The next stop is the memory-source segment-register load `66 8E 05 E4 65 1A 02` at `0x020F39C8`.

## 2026-07-09 현재 상태

이전 TODO/PLAN의 분석 및 기반 구조 작업은 완료되었다.
이번 단계에서 Win32 x86 guest ESP 전환 trampoline도 구현되었다.
privileged instruction 예외 위치를 HLE trap 후보와 CPU/DPMI 상태 초기화 후보로 분류하는 초기 classifier도 추가되었다.
`STI`는 첫 HLE trap으로 처리되었다.
`INT 21h AH=0x30`도 처리되어 다음 중단 지점인 `INT 21h AH=0xFF`까지 진행된다.
`INT 21h AH=0xFF`도 최소 응답으로 처리되어 다음 중단 지점인 segment register load 계열 명령 `8E D9`까지 진행된다.
segment register load는 guest selector shadow state로 처리되어 다음 중단 지점인 segment register store 계열 명령 `66 26 8C 1D`까지 진행된다.

남은 실제 구현 작업은 다음과 같다.

1. HLE dispatcher handler 호출 규약과 guest context 복귀 경로 구현
2. 실제 trace로 확인된 INT21/INT31 서비스 최소 구현
3. selector/descriptor 권한 검사와 DPMI descriptor API 연결
4. `66 26 8C 1D` segment register store 중단 지점을 selector/descriptor memory write HLE 요구사항으로 분류

## Status As Of 2026-07-09

The previous TODO/PLAN analysis and foundation work is complete.
This step also implements the Win32 x86 guest ESP-switching trampoline.
It also adds the initial classifier that separates privileged-instruction exceptions into HLE trap candidates and CPU/DPMI state initialization candidates.
`STI` is now handled as the first HLE trap.
`INT 21h AH=0x30` is also handled, allowing execution to proceed to the next stop at `INT 21h AH=0xFF`.
`INT 21h AH=0xFF` is also handled with a minimal response, allowing execution to proceed to the next stop at segment-register load instruction `8E D9`.
Segment-register loads are handled through guest selector shadow state, allowing execution to proceed to the next stop at segment-register store instruction `66 26 8C 1D`.

Remaining real implementation work:

1. Implement the HLE dispatcher handler calling convention and guest context return path.
2. Implement only the INT21/INT31 services confirmed by actual traces.
3. Connect selector/descriptor permission checks and DPMI descriptor APIs.
4. Extend segment-register load HLE for the `66 8E 05 E4 65 1A 02` memory-source form.

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
5. privileged instruction 초기 분류기와 loader 출력 연결: 완료.
6. `STI` HLE trap 처리와 다음 `INT 21h` 중단 지점 관찰: 완료.
7. `INT 21h AH=0x30` DOS version query 처리와 다음 `AH=0xFF` 중단 지점 관찰: 완료.
8. `INT 21h AH=0xFF` 최소 처리와 다음 `8E D9` 중단 지점 관찰: 완료.
9. `8E /r` register source segment load 처리와 다음 `66 26 8C 1D` 중단 지점 관찰: 완료.

## 남은 실제 구현 작업

1. HLE dispatcher handler 호출 규약과 guest context 복귀 경로 구현.
2. INT21/INT31 중 실제 trace로 확인된 서비스부터 최소 구현.
3. selector/descriptor 권한 검사와 DPMI descriptor API 연결.
4. `66 26 8C 1D` segment register store를 guest selector shadow state와 relocated memory write 정책으로 연결.

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
5. Initial privileged instruction classifier and loader output wiring: complete.
6. `STI` HLE trap handling and observation of the next `INT 21h` stop: complete.
7. `INT 21h AH=0x30` DOS version query handling and observation of the next `AH=0xFF` stop: complete.
8. `INT 21h AH=0xFF` minimal handling and observation of the next `8E D9` stop: complete.
9. `8E /r` register-source segment load handling and observation of the next `66 26 8C 1D` stop: complete.
10. `66 26 8C /r` absolute-destination segment store handling and observation of the next `66 8E 05` stop: complete.

## Remaining Real Implementation Work

1. Implement the HLE dispatcher handler calling convention and guest context return path.
2. Implement only the INT21/INT31 services confirmed by actual traces.
3. Connect selector/descriptor permission checks and DPMI descriptor APIs.
4. Extend `66 8E 05 E4 65 1A 02` memory-source segment-register load through relocated memory-read policy.
