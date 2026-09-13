# Task 660: Linux x64 zero-return stack tail

## 한국어

### 목적

Task 659에서 `ENTER`의 host-stack 오염은 제거되었지만, 실행은
`guest EIP=0x010F0237`의 `RET`에서 guest target `0`으로 중단됩니다. 현재
frame 진단은 반환 슬롯과 그 슬롯을 쓴 과거 writer만 보여 주므로, 같은
슬롯을 여러 번 재사용한 뒤 어떤 최근 stack operation이 최종 상태를
만들었는지 알 수 없습니다.

이번 작업은 원본 guest 코드를 수정하지 않고, 이미 존재하는 bounded
`REPIU_LINUX_X64_RETURN_STACK_TAIL` ring 출력을 zero-return frame 경로에도
연결합니다. 진단은 최대 요청 개수만 출력하고 기본값은 비활성 상태로
유지합니다.

### 확인된 현재 흐름

```mermaid
sequenceDiagram
    participant C as x64 cache
    participant T as ReturnThunk
    participant F as zero-return frame trace
    participant R as bounded stack-tail

    C->>T: RET site 0x010F0237, target [guest ESP] = 0
    T->>F: frame.guest_source = 0
    F->>F: print frame and matching consumed slot writers
    F->>R: print most recent guest stack writes (opt-in)
    R-->>F: chronological ring window
    F-->>T: diagnostics only; transfer remains fail-closed
```

### 설계 결정

1. `TraceLinuxX64ZeroReturnFrame()`이 기존
   `TraceLinuxX64ReturnStackTail()`을 호출합니다.
2. 출력량은 기존 `REPIU_LINUX_X64_RETURN_STACK_TAIL` 값과
   `REPIU_LINUX_X64_FRAME_STACK_TRACE_CAPACITY` 제한을 그대로 따릅니다.
3. 환경 변수가 없으면 코드·성능·출력 동작이 바뀌지 않습니다.
4. zero target을 임의의 주소로 복구하거나 return semantics를 추측하지
   않습니다. tail은 원인 귀속을 위한 관찰 수단일 뿐입니다.

### 검증 전략

- Linux x64 Debug에서 `repiu`를 빌드합니다.
- 기존 `repiu_core_probe` 전체 결과가 계속 27/27인지 확인합니다.
- `REPIU_LINUX_X64_STACK_TRACE=1`,
  `REPIU_LINUX_X64_RETURN_FRAME_TRACE=1`,
  `REPIU_LINUX_X64_RETURN_STACK_TAIL=96`으로 bounded 실행을 하고
  zero-return 직전 tail이 출력되는지 확인합니다.

## English

### Purpose

Task 659 removed the host-stack corruption caused by `ENTER`, but execution now
stops at guest `EIP=0x010F0237` when the `RET` observes guest target `0`. The
current frame diagnostic shows the return slot and historical writers of that
slot, but not which recent stack operations produced the final state after slot
reuse.

This task connects the existing bounded
`REPIU_LINUX_X64_RETURN_STACK_TAIL` ring output to the zero-return frame path
without modifying original guest code. It prints at most the requested number
of records and remains disabled by default.

### Confirmed flow

The flow is shown in the Mermaid diagram above: the x64 cache reaches the return
thunk with a zero guest target, the frame diagnostic prints its evidence, and
the bounded stack-tail is optionally appended. The transfer remains fail-closed.

### Design decisions

1. `TraceLinuxX64ZeroReturnFrame()` calls the existing
   `TraceLinuxX64ReturnStackTail()` helper.
2. Output remains bounded by the existing
   `REPIU_LINUX_X64_RETURN_STACK_TAIL` value and
   `REPIU_LINUX_X64_FRAME_STACK_TRACE_CAPACITY`.
3. Without the environment variable, code, performance, and output behavior
   are unchanged.
4. The zero target is not guessed into an address and return semantics are not
   inferred. The tail is an attribution aid only.

### Verification strategy

- Build `repiu` in Linux x64 Debug configuration.
- Confirm the existing `repiu_core_probe` suite remains 27/27.
- Run bounded execution with `REPIU_LINUX_X64_STACK_TRACE=1`,
  `REPIU_LINUX_X64_RETURN_FRAME_TRACE=1`, and
  `REPIU_LINUX_X64_RETURN_STACK_TAIL=96`, and confirm the tail appears beside
  the zero-return frame.
