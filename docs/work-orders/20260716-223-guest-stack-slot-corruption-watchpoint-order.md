# 게스트 스택 슬롯 비동기 손상 writer 포착 작업 지시서
# Work Order: Catch the Async Writer Corrupting the Guest Stack Slot

## 1. 배경 (Background)

Task 222는 종료 지점 `0x0302208C`(`mov [edi], al` → wild `0xDD1523B1`) 분석에서 다음을
확정했다:

* 문제 함수 entry는 guest `0x03021DF8`(Watcom 레지스터 규약, 프레임 0x190).
* EDI는 caller 인자가 아니라 지역변수 `[esp+0x154] = ESI+0xC`(0x03021F41에서 설정,
  0x03021F71에서 적재).
* 구조체 필드 store(`[ESI]/[ESI+4]/[ESI+8]`)는 성공 → ESI(=`0x0325E208`)는 유효.
* EDI(`0xDD1523B1`)는 **홀수**라 짝수 `ESI+0xC`(=`0x0325E214`)일 수 없음 → 목적지 지역만 손상.
* **AOT 정적·동적 번역 모두 바이트 단위 정확**(런타임 캐시 프로브로 확인).
* store(0x03021F41)와 load(0x03021F71) 사이엔 call/push/pop/boundary/int3 없음.

결론: 게스트 스택 슬롯 `0x035D6B14`(=이 프레임의 `[esp+0x154]`, 실측·결정론적)가 **게스트
명령 스트림 외부에서 비동기로 덮어써진다**. 값 `0xDD1523B1`은 fault 이전 어떤 HLE 트레이스/
레지스터에도 등장하지 않는다.

## 2. 목적 (Objective)

이 스택 슬롯을 손상시키는 **쓰기의 출처(EIP/호출 경로)**를 확정한다. 후보 기제:

1. AOT 디스패치/VEH 기구(반환 인라인 캐시, 경계 처리, RecoverToHost 등)의 게스트 스택 쓰기.
2. HLE 핸들러(타이머 tick, DPMI 프레임, 세그먼트 스위치, MSCDEX 등)의 잘못된 오프셋 쓰기.
3. **메모리 레이아웃 겹침** — 게스트 스택 영역(`0x035Dxxxx`)이 다른 매핑/합성 구조체와
   중첩되어 다른 곳으로의 쓰기가 이 슬롯을 오염(Task 220/221의 arena/LINEXE private-data
   충돌 계열).

## 3. 세부 작업 (Tasks)

1. **하드웨어 워치포인트 설계·구현.** 게스트 주소(우선 `0x035D6B14`, env로 지정 가능)에
   DR0 + DR7(write, len=4)을 걸어 쓰기 시 debug 예외를 받고, 쓰기 명령의 host/guest EIP와
   호출 스택을 로깅한다.
   * **주의(핵심 리스크):** aot-dynamic 백엔드는 TF(trap flag)/int3를 디스패치에 광범위하게
     사용한다. DR 기반 debug 예외(`EXCEPTION_SINGLE_STEP`/debug)는 기존 `GuestStackVectored
     ExceptionHandler`의 TF/int3 처리와 충돌하지 않도록, DR 이벤트를 별도로 식별해 로깅 후
     계속 실행시켜야 한다. 먼저 이 공존 방식을 설계 문서로 정리한다.
   * 슬롯 주소가 실행마다 고정인지(3회 구동 모두 `0x035D6B14`) 재확인하고, 아니면 프레임
     활성화 시점에 동적으로 DR을 설정하는 방식으로 대체.
2. **손상 시점 좁히기(대안/보완).** DR 구현 전, 저비용 실험으로 후보 기제를 배제:
   * 메모리 레이아웃 겹침(후보 3) 확인 — 게스트 스택 영역과 arena/합성 구조체/HLE 버퍼의
     주소 범위를 실측 비교(`0x035D6B14`가 다른 매핑 안에 드는지).
   * 의심 HLE(타이머 tick 등)를 토글로 끄고 재현 여부 확인(후보 2 이분 탐색).
3. 확정된 출처에 맞는 수정을 설계·구현하고 aot-dynamic 60초 + trap 30초로 검증한다.

## 4. 검증 범위 (Verification Scope)

각 단계는 aot-dynamic 60초 구동으로 확인하고, 코드 수정이 포함되면 trap 백엔드 30초 회귀를
함께 확인한다. 근인 확정 전에는 HLE 추측 수정을 하지 않는다(AGENTS.md).

## 5. 참고 (References)

* `docs/work-logs/20260716-222-string-copy-store-frontier-log.md`
* `docs/analysis/current-execution-frontier.md` (Task 222 항목)
* `docs/analysis/aot-return-stack-divergence.md`(AOT call/return 프레임 계열)

---

# Work Order (English)

## Background

Task 222 established that the `0x0302208C` terminal (`mov [edi], al` to wild `0xDD1523B1`) is a
corrupted destination-pointer local, not a caller argument: EDI is `[esp+0x154] = ESI+0xC` in
function `0x03021DF8` (Watcom register convention, 0x190 frame); the struct-field stores to
`[ESI]/[ESI+4]/[ESI+8]` succeed (ESI = `0x0325E208`, valid); EDI is odd and cannot be the even
`ESI+0xC`; and both the static and the runtime dynamic AOT translations are byte-identical and
correct, with no call/push/pop/boundary between the store (`0x03021F41`) and the load
(`0x03021F71`). So the guest stack slot `0x035D6B14` (this frame's `[esp+0x154]`, measured and
deterministic) is overwritten asynchronously from outside the guest instruction stream, and the
value `0xDD1523B1` appears in no pre-fault HLE trace.

## Objective

Pin down the source (EIP / call path) of the write that corrupts this slot. Candidates:
(1) the AOT dispatch/VEH machinery (return inline cache, boundary handling, RecoverToHost);
(2) an HLE handler (timer tick, DPMI frame, segment switch, MSCDEX) writing at a wrong offset;
(3) a memory-layout overlap where the guest stack region (`0x035Dxxxx`) collides with another
mapping/synthesized structure (the Task 220/221 arena / LINEXE private-data collision family).

## Tasks

1. Design and implement a hardware watchpoint: DR0 + DR7 (write, len 4) on the guest address
   (start with `0x035D6B14`, env-configurable), logging the writing instruction's host/guest EIP
   and call stack on the debug exception. **Key risk:** the aot-dynamic backend uses TF/int3
   extensively; DR debug exceptions must be distinguished from and coexist with the existing
   `GuestStackVectoredExceptionHandler` TF/int3 handling — write a design note for this
   coexistence first. Re-confirm the slot address is stable across runs (it was `0x035D6B14` in
   all three), else set DR dynamically when the frame activates.
2. Cheap alternatives to narrow the mechanism before DR: compare the guest stack address range
   against the arena / synthesized structures / HLE buffers to test the memory-overlap candidate;
   toggle suspect HLEs (e.g., the timer tick) off and check reproduction to bisect candidate (2).
3. Implement the fix matching the confirmed source and verify with aot-dynamic 60 s + trap 30 s.
   No speculative HLE change until the corruption source is confirmed.
