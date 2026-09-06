# Task 618 작업 지시서: Linux x64 direct-call 게스트 스택 provenance 추적

## 한국어

### 배경

Task 617은 `guest_source=0`이 indirect target이 아니라 direct `RET`가
스택에서 읽은 0임을 확인했고, producer site를 `0x010F101D`로 식별했다.
소비 후 ESP는 `0x0158CC4C`이므로 문제의 consumed slot은
`0x0158CC48`이다.

### 작업

1. Task 618 설계대로 opt-in direct-call stack trace ring을 구현한다.
2. Linux x64 frame에 trace 상태와 고정 크기 record를 추가한다.
3. long-mode direct `CALL` emission에서 flags-preserving 기록 sequence를
   조건부로 삽입한다.
4. zero return-frame 진단에 consumed slot match 결과를 추가한다.
5. core probe와 Linux x64 재실행으로 writer 유무를 확인한다.
6. 확인 결과를 `docs/analysis/linux-port-frontier.md`와 작업 로그에 남긴다.

### 안전 제약

* 기본 실행에서 trace emission은 비활성이다.
* trace 코드는 반환 주소, guest GPR, guest flags를 변경하지 않는다.
* direct-call 기록이 없더라도 zero target을 추측하거나 자동 수정하지 않는다.

### 완료 조건

* 지정된 core probe가 통과한다.
* trace 실행 로그에 consumed slot과 matching direct-call record 여부가
  나타난다.
* 설계·작업 지시서·작업 로그·분석 문서가 모두 갱신되고 커밋된다.

## English

### Background

Task 617 established that `guest_source=0` is a direct `RET` reading zero from
the guest stack, not an indirect target. The producer site is `0x010F101D`.
After the pop, ESP is `0x0158CC4C`, so the consumed slot is `0x0158CC48`.

### Work

1. Implement the opt-in direct-call stack trace ring from the Task 618 design.
2. Add trace state and fixed-size records to the Linux x64 frame.
3. Insert a flags-preserving record sequence conditionally in long-mode direct
   `CALL` emission.
4. Add consumed-slot match results to the zero return-frame diagnostic.
5. Run the core probe and Linux x64 reproduction to determine whether a direct
   call wrote the slot.
6. Record the result in `docs/analysis/linux-port-frontier.md` and the work log.

### Safety constraints

* Trace emission is disabled in the default run.
* The trace sequence does not change the return address, guest GPRs, or guest
  flags.
* Do not guess or repair a zero target when no direct-call record exists.

### Done criteria

* The specified core probe passes.
* The traced run reports the consumed slot and whether a matching direct-call
  record exists.
* The design, work order, work log, and analysis documents are updated and
  committed.
