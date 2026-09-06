# Task 619: Linux x64 일반 AOT stack writer 추적

## 한국어

### 목적

Task 618은 `0x0158CC48`에 대한 direct `CALL` 기록이 모두 정상임을
확인했다. 따라서 `0x010F101D`의 `RET`가 소비한 최종 0은 마지막 direct
`CALL` 이후의 일반 게스트 stack writer일 가능성을 추적해야 한다.

### 설계

1. `REPIU_LINUX_X64_STACK_TRACE=1`에서 `LowerLongModeBytes`가 처리한
   `PUSH` instruction 뒤에도 기존 stack-write 512-record ring 기록 sequence를
   붙인다.
2. direct `CALL` record는 `fallthrough` 필드에 기대한 반환 주소를 보존하고,
   일반 `PUSH` record는 그 필드를 0으로 둔다. resolver는 이를 이용해
   `direct-call`과 `guest-push`를 구분한다.
3. 기록 시점은 lowering sequence가 guest `R15D`를 갱신하고 `[R15]`에 값을
   쓴 직후다. 따라서 기록값은 실제 guest stack slot의 현재 값이다.
4. `PUSH ES/CS/SS/DS` 및 `PUSH FS/GS`가 HLE/미지원 경계로 가는 경우에는
   이번 trace에 포함하지 않는다. 해당 경계는 별도 HLE trace 대상이다.

```mermaid
flowchart LR
    A[AOT instruction] --> B{direct CALL?}
    B -- yes --> C[record expected fallthrough]
    B -- no --> D{lowered guest PUSH?}
    D -- yes --> E[record actual [R15] value]
    D -- no --> F[existing emission]
    C --> G[shared stack-write ring]
    E --> G
    G --> H[zero RET consumed-slot comparison]
```

### 비범위

이 작업은 일반 `PUSH`의 의미나 stack pointer lowering을 바꾸지 않는다.
HLE segment push, DOS/DPMI host call의 stack access, 임의 zero target 복구는
범위 밖이다.

### 검증 기준

* core probe가 계속 `24/24` 통과한다.
* terminal zero `RET`의 consumed slot에 대해 `guest-push` 기록이 출력되면
  마지막 덮어쓰기 후보를 확보한다.
* direct-call만 기록되고 일반 push 기록이 없으면 HLE/host 경계를 다음 후보로
  유지한다.

## English

### Purpose

Task 618 confirmed that all direct-call records for `0x0158CC48` were correct.
Task 619 traces ordinary guest stack writers after the last direct CALL, where
the final zero consumed by `0x010F101D` may have been produced.

### Design

1. When `REPIU_LINUX_X64_STACK_TRACE=1`, append the existing stack-write
   sequence after `PUSH` instructions handled by `LowerLongModeBytes`; the ring
   has 512 records so this run's 321 writes are retained.
2. Direct-call records retain their expected return address in `fallthrough`;
   ordinary `PUSH` records set that field to zero. The resolver uses this to
   distinguish `direct-call` from `guest-push`.
3. Record after the lowering sequence updates guest `R15D` and stores to
   `[R15]`, so the value is the actual current guest stack-slot value.
4. Segment pushes that go through HLE or an unsupported boundary are not part of
   this trace; they remain a separate HLE-trace target.

### Out of scope

This task does not change ordinary `PUSH` semantics or stack-pointer lowering.
HLE segment pushes, DOS/DPMI host-call stack access, and arbitrary zero-target
repair remain out of scope.

### Verification criteria

* The core probe remains green at `24/24`.
* A `guest-push` record matching the terminal zero `RET` consumed slot identifies
  a final overwrite candidate.
* If only direct-call records appear, HLE/host boundaries remain the next
  candidates.
