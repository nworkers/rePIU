# 20260911-654 설계: Linux x64 legacy bridge TF 전이

## 한국어

### 문제

Task 653의 legacy-resume thunk는 guest EFLAGS에 TF를 설정한 `POPFQ` 뒤에 guest EAX를
복원하는 `MOV`를 실행하고 나서 guest target으로 jump합니다. x86의 trap flag는 다음
명령 경계에 debug exception을 만들기 때문에, TF를 설정한 뒤 guest 전이 전에 host
명령을 두면 guest target에서 시작해야 할 single-step 계약을 보장할 수 없습니다.

실제 `pumpit2a`에서 resolver는 `0x010F925D` bridge를 선택했지만 guest watch는 바로
다음 `0x010F925F`뿐 아니라 `0x010F926E`, `0x010F9271`, `0x010F9273`에서도 한 번도
single-step을 관찰하지 못했습니다. 그 결과 `0x010F9273 CALL 0x010F1D74`가 guest 반환
주소를 만들지 않은 채 long-mode 원본 CALL로 실행됐고, allocator의 두 번째 RET가 zero를
소비했습니다.

### 설계

1. legacy-resume thunk에서 guest continuation과 guest EAX를 먼저 host register에
   복원합니다.
2. 별도 scratch register에 저장된 guest EFLAGS에 TF를 설정합니다.
3. thunk의 마지막 두 명령을 `POPFQ; JMP continuation`으로 고정합니다. jump 자체가
   TF 이후의 host 명령이 되며, debug exception의 다음 instruction address는 guest
   target입니다.
4. guest target에서 기존 single-step/HLE dispatcher가 다시 TF를 무장하고 이후 guest
   명령을 한 단계씩 처리합니다. 이에 따라 뒤의 `E8 rel32`는 Task 652 handler가 guest
   stack 효과를 적용합니다.
5. resolver의 동일성 검사와 unproven target fail-closed 정책은 변경하지 않습니다.

```mermaid
sequenceDiagram
    participant R as Return resolver
    participant B as Legacy bridge
    participant G as Guest code
    participant H as #DB/HLE
    R->>B: continuation + saved registers/flags
    B->>B: restore EAX, set TF in saved flags
    B->>G: POPFQ; JMP target
    G-->>H: #DB at target boundary
    H->>G: re-arm TF and step guest instruction
    G-->>H: next #DB
    H->>H: intercept E8/stack/segment when required
```

### 검증 전략

Linux x64 Debug 빌드와 전체 core probe를 실행합니다. 실제 `pumpit2a`에서
`REPIU_GUEST_WATCH=0x010F925F`와 `0x010F9273`을 각각 관찰해 bridge 직후 guest
single-step과 direct CALL 선점을 확인합니다. stack trace에서 `0x010F9273`이
`0x010F9278`을 guest stack에 기록하고 두 번째 `0x010F1E56` RET가 그 값을 소비하는지
확인한 뒤 새 frontier를 측정합니다.

## English

### Problem

Task 653's legacy-resume thunk executes a `MOV` that restores guest EAX after
`POPFQ` enables TF, and only then jumps to the guest target. Because the x86
trap flag creates a debug exception at the next instruction boundary, placing
another host instruction between TF activation and the guest transfer cannot
guarantee that single-step begins at the guest target.

In the real `pumpit2a` run, the resolver selected the bridge for `0x010F925D`,
but guest watches observed no single-step at `0x010F925F`, `0x010F926E`,
`0x010F9271`, or `0x010F9273`. Consequently, `0x010F9273 CALL 0x010F1D74`
executed as an original long-mode CALL without forming a guest return address,
and the allocator's second RET consumed zero.

### Design

1. Restore the guest continuation and guest EAX into host registers before
   activating TF in the legacy-resume thunk.
2. Set TF in the saved guest EFLAGS held in a separate scratch register.
3. Make `POPFQ; JMP continuation` the thunk's final two instructions. The jump
   itself is then the host instruction following TF activation, and the next
   instruction address reported by the debug exception is the guest target.
4. At the guest target, the existing single-step/HLE dispatcher re-arms TF and
   handles later guest instructions one at a time. A later `E8 rel32` therefore
   receives the Task 652 guest-stack semantics.
5. Keep the resolver's identity proof and fail-closed policy unchanged.

### Verification strategy

Build Linux x64 Debug and run the complete core probe. In real `pumpit2a` runs,
watch `0x010F925F` and `0x010F9273` separately to confirm the first guest step
after the bridge and interception of the direct CALL. Use the stack trace to
confirm that `0x010F9273` writes `0x010F9278` to the guest stack and that the
second `0x010F1E56` RET consumes it, then measure the next frontier.
