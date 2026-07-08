# TODO/PLAN 잔여 작업 정리 설계

## 배경

`docs/TODO.md`에는 최소 실행 trampoline 이후의 후속 작업 6개가 남아 있었다.
이번 작업은 해당 항목을 한 번에 정리하되, 원본 게임 로직을 재구현하지 않고 원본 32-bit x86 코드 실행 경로를 보존하는 기존 원칙을 유지한다.

## 설계 결정

1. skipped relocation 10개는 요약만이 아니라 개별 레코드 목록을 확인할 수 있어야 하므로 relocation dry-run 결과에 상세 목록을 보존한다.
2. guest stack 전환은 장기 실행 trampoline의 전제 조건으로 문서 설계를 확정한다. 현재 minimal trampoline은 관찰용으로 유지한다.
3. INT/DPMI/HLE trap은 원본 코드 패치보다 호스트 제어 지점으로 진입하는 방식으로 설계한다.
4. `0x020F3890` privileged instruction 예외는 DOS/4G 런타임 초기화 또는 특권 명령 경로로 분류하고, trap/HLE dispatcher 설계의 첫 후보로 둔다.
5. Win32 object protection은 LE object flags 기반 정책을 유지하되, 비 Win32 빌드에서는 Win32 API 호출을 명확히 unsupported로 처리한다.
6. relocated image 배치는 장기 runtime memory manager의 입력 구조로 유지하고, object/entry/stack/HLE reserve 정보를 결과 문서에 고정한다.

## 비목표

* 원본 executable 코드 수정
* 게임 로직 C++ 재작성
* DOSBox 코드 통합
* 실제 DPMI 전체 구현

## 검증 전략

* Linux CMake 빌드가 Win32 API 부재 때문에 실패하지 않아야 한다.
* exe analyzer가 skipped relocation 상세 목록을 출력할 수 있어야 한다.
* 작업 결과는 별도 Markdown 결과 문서에 한국어 우선, 영어 번역 순서로 기록한다.

# TODO/PLAN Remaining Work Closure Design

## Background

`docs/TODO.md` still had six follow-up tasks after the minimal execution trampoline.
This task closes those items together while preserving the existing principle: keep the original 32-bit x86 execution path and do not reimplement game logic.

## Design Decisions

1. The 10 skipped relocations must be inspectable as individual records, not only as summary counts, so the relocation dry-run keeps a detailed skipped-record list.
2. Guest stack switching is fixed as a prerequisite for a long-running trampoline. The current minimal trampoline remains observation-only.
3. INT/DPMI/HLE traps are designed to enter host-controlled dispatch points instead of patching original gameplay code.
4. The privileged-instruction exception at `0x020F3890` is classified as a DOS/4G runtime initialization or privileged instruction path and becomes the first trap/HLE dispatcher candidate.
5. Win32 object protection keeps the LE object flag policy, while non-Win32 builds now report Win32 API paths as unsupported explicitly.
6. Relocated image placement remains the input structure for the long-running runtime memory manager, and object/entry/stack/HLE reserve facts are fixed in the result document.

## Non-Goals

* Modifying the original executable code.
* Rewriting gameplay logic in C++.
* Integrating DOSBox code.
* Implementing all DPMI services now.

## Verification Strategy

* Linux CMake builds must not fail only because Win32 APIs are absent.
* The exe analyzer must be able to print detailed skipped relocation records.
* The task result must be recorded in a separate Markdown result document with Korean first and English translation immediately after it.
