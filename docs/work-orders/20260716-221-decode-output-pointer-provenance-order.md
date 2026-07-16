# 디코드 출력 포인터 0x045D3EB0 provenance 재조사 작업 지시서
# Work Order: Re-investigating the Provenance of Decode Output Pointer 0x045D3EB0

## 1. 목적 (Objective)

Task 220의 처리량 수정 후 실행이 기지의 종료 지점(guest `0x030873F4`가 `0x045D3EB0`에 쓰는
`0xC0000005`)에 재도달했다. Task 213의 resize 상한 모델링에도 같은 주소가 재현되므로, 게스트
allocator가 heap top을 얻는 실제 경로를 재조사하고 수정 방향을 확정한다.

## 2. 1차 조사에서 확인된 사실 (Findings So Far)

1. **Task 213의 resize 상한은 실제로 한 번도 작동하지 않았다.** Task 220 검증 구동(정상 종료)의
   최종 요약: `handled DOS resize count: 150`, `last DOS resize selector: 0x002B`,
   `last DOS resize paragraphs: 0x1500`, `requested end: 0x00015000`, `result: success`.
   `HandleDosResizeMemoryBlock`(`execution_trampoline.cpp:3743`)은 `context->guest_es`를
   그대로 사용하는데 이 값이 게스트 selector가 아니라 **호스트 진입 ES(`0x2B`)**여서
   `FindDescriptor`가 실패하고 `selector_base = 0`이 된다. 그 결과 `requested_end`(작은 값)와
   `dynamic_allocator_end`(0x045C6000)의 비교가 항상 통과해 150건 전부 무조건 성공으로
   보고됐다 — Task 213 검증 당시의 "해결" 판정은 다른 요인(당시 예외 루프 소멸)의 부수 효과로
   보인다.
2. DPMI `INT 31h`는 0x0500(free memory info)/0x0501(allocate)을 처리하지 않고(unsupported
   경로), `INT 21h`는 AH=48h(할당)를 처리하지 않는다. 합성 client/private data 페이지에는
   메모리 풀 경계 값이 기록되지 않는다. 즉 **게스트가 heap top(0x045D7000 = arena_end)을 얻는
   경로는 아직 미확정**이다.
3. 산술적 단서: `0x045D3EB0 + 0x3150 = 0x045D7000 = arena_end = arena base 0x03000000 +
   reserve 0x015D7000`. 게스트의 heap top 믿음은 arena 예약 전체 크기와 정확히 일치한다.

## 3. 세부 작업 (Tasks)

1. `HandleDosResizeMemoryBlock`이 `ReadGuestSegmentSelector`로 ES를 해석하도록 수정하고,
   게스트가 넘긴 **EBX 전체(32비트)**와 해석된 selector/base를 텔레메트리로 기록한다
   (DOS/4G 확장 API는 16비트 BX가 아니라 EBX 크기 인자를 쓸 수 있다 — 현재 16비트 절단).
2. 재구동으로 150건 resize의 실제 요청 크기 분포와 base를 확인해, heap top이 resize 응답에서
   오는지 재판정한다.
3. resize가 아니라면 디코드 구조체 `[ESI+0x34]`에 `0x045D3EB0`을 쓰는 게스트 명령을
   write-watch/트랩으로 포착해 출처를 역추적한다.
4. 결과에 따라 (a) resize/할당 HLE의 상한을 실제로 작동하게 수정, 또는 (b) heap top을 노출하는
   미지의 경로를 모델링. `docs/analysis/current-execution-frontier.md`에 반영한다.

## 4. 검증 범위 (Verification Scope)

각 단계는 aot-dynamic 40초 구동으로 검증한다(현재 21초에 종료 지점 도달 가능). 최종 수정은
trap 백엔드 30초 회귀 확인을 포함한다.
