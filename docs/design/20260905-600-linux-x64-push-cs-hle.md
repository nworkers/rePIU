# 설계 20260905-600 — Linux x64 guest `PUSH CS` HLE

## 목적

Task 599의 opt-in `INT 31h AX=1E7Fh` 성공 경로 탐침은 guest
`0x010F0117`의 `0Eh` (`PUSH CS`)에서 SIGILL로 멈췄습니다. 이 명령은
32-bit protected mode에서는 유효하지만 x86-64 long mode에서는 유효하지
않습니다.

기존 `HandleSegmentPushInstruction`은 ES, SS, DS, FS, GS를 이미 32-bit
guest stack에 dword로 push하지만 CS opcode만 인식하지 않습니다. 이 작업은
같은 HLE의 범위를 CS까지 확장합니다.

## 설계 결정

1. `0Eh`에서 임의의 고정 selector를 사용하지 않습니다. 현재 guest EIP를 포함하는
   present descriptor를 selector table에서 찾아 그 selector를 CS 값으로 사용합니다.
2. descriptor 범위가 없거나 겹쳐 유일하게 결정할 수 없으면 HLE는 실패하여 기존
   fail-closed fault 처리를 유지합니다.
3. 성공 시 기존 segment push와 동일하게 `ESP -= 4`, `[ESP] = zero_extend(CS)`,
   `EIP += 1`을 적용하며 EFLAGS와 다른 범용 레지스터는 바꾸지 않습니다.
4. 기존 ES/SS/DS/FS/GS 처리와 AOT emitter 정책은 변경하지 않습니다.
5. Task 599의 `1E7Fh` probe는 계속 opt-in 진단일 뿐이며, `PUSH CS` HLE가
   그 서비스의 성공 계약을 확정하지 않습니다.

```mermaid
sequenceDiagram
    participant G as Guest 32-bit code
    participant F as Linux SIGILL
    participant H as Segment-push HLE
    participant T as Selector table
    participant S as Guest stack

    G->>F: 0Eh PUSH CS
    F->>H: EIP at 0Eh
    H->>T: find present descriptor containing EIP
    T-->>H: current code selector
    H->>S: ESP -= 4; write zero-extended selector
    H-->>G: EIP += 1
```

## 검증

* selector 범위 선택과 dword stack 결과를 단위/핵심 probe로 확인합니다.
* Linux x64 `repiu` 및 `repiu_core_probe`를 빌드합니다.
* `REPIU_DPMI_1E7F_PROBE_SUCCESS=1` 실행에서 기존 `0Eh` SIGILL이 사라지고
  다음 frontier가 관측되는지 확인합니다.

---

# Design 20260905-600 — Linux x64 guest `PUSH CS` HLE

## Purpose

Task 599's opt-in success-path probe for `INT 31h AX=1E7Fh` stopped at guest
`0x010F0117`, opcode `0Eh` (`PUSH CS`), with SIGILL. The instruction is valid
in 32-bit protected mode but invalid in x86-64 long mode.

The existing `HandleSegmentPushInstruction` already pushes ES, SS, DS, FS, and
GS as dwords to the 32-bit guest stack, but does not recognize the CS opcode.
This unit extends that same HLE to CS.

## Decisions

1. Do not use a fixed selector. Find the present selector-table descriptor that
   contains the current guest EIP and use its selector as CS.
2. If no descriptor contains EIP, or more than one does, fail the HLE and retain
   the existing fail-closed fault handling.
3. On success, match the existing segment push: `ESP -= 4`,
   `[ESP] = zero_extend(CS)`, and `EIP += 1`, without altering EFLAGS or other
   general registers.
4. Do not change ES/SS/DS/FS/GS handling or the AOT emitter policy.
5. The Task 599 `1E7Fh` probe remains opt-in diagnostic only; this HLE does not
   establish that private service's success contract.

## Verification

* Verify selector-range selection and the dword stack result through an
  appropriate unit/core probe.
* Build Linux x64 `repiu` and `repiu_core_probe`.
* With `REPIU_DPMI_1E7F_PROBE_SUCCESS=1`, verify the prior `0Eh` SIGILL is gone
  and a subsequent frontier is observed.
