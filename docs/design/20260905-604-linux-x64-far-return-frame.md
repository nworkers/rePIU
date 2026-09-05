# 설계 20260905-604 — Linux x64 관찰된 far-return frame HLE

## 목적

Task 603에서 `0x01100040`의 `66 CB`를 fail-closed boundary로 분리한 뒤,
boundary 시점의 stack을 다시 관찰했습니다. `ESP=0x0158CC54`에서
`[ESP]=0x010F0232`, `[ESP+4]=0x00000024`가 확인됩니다. 원본 object 2가
`PUSH CS`와 `PUSH 0x010F0232`를 수행했고, object 3은 16-bit code object에서
operand-size override가 붙은 `66 CB`를 실행하므로 이 값은 `offset`과 padded
CS slot으로 해석할 수 있습니다.

Intel RET 정의에 따르면 far return의 32-bit operand-size 경로는 EIP와 CS를
각각 32-bit stack pop으로 읽으며, CS의 상위 16-bit는 버립니다. 따라서 이
관찰된 frame의 소비량은 `ESP += 8`입니다. 이번 단위는 이 정확한 frame 형식을
현재 selector table의 object mode metadata로 검증한 뒤 Linux x64 HLE로 실행하는
것을 목표로 합니다.

## 범위

* LE object flags의 executable 및 big/default 정보를 selector descriptor metadata로
  전달합니다.
* 현재 code descriptor가 16-bit default이고 target descriptor가 executable일 때만
  `66 CB`를 32-bit far return으로 처리합니다.
* stack에서 offset dword와 CS dword를 읽고, CS low word를 selector로 사용합니다.
* target selector/offset를 selector table로 검증한 후 `EIP`, `SegCs`, `ESP += 8`을
  갱신합니다.
* 순수 selector/frame resolution probe와 Linux x64 runtime 재현으로 검증합니다.

## 범위 밖

* 16-bit operand-size far return(`CB` 또는 다른 mode 조합)의 일반화
* privilege-level change, call-gate, outer-stack 전환
* 현재 엔진이 표현하지 않는 별도 SS descriptor의 B-bit 모델링
* `INT 31h AX=1E7Fh` private ABI 변경
* `0x000000FF`를 반환 주소로 보정

## 처리 흐름

```mermaid
flowchart LR
    A[guest 01100040: 66 CB] --> B[current CS descriptor]
    B -->|16-bit default + executable| C[read [ESP] offset dword]
    C --> D[read [ESP+4] CS dword]
    D --> E[validate target selector and offset]
    E -->|valid| F[EIP=target, SegCs=selector, ESP+=8]
    E -->|invalid| G[decline and keep fail-closed boundary]
```

## 설계 결정

1. `GuestDescriptor`에 raw LE object flags와 code-mode metadata를 별도 필드로
   보관합니다. 기존 descriptor access flags 의미와 LE object flags를 섞지 않습니다.
2. relocated selector binding이 object flags를 전달하되, 기존 aggregate initializer의
   앞부분과 호환되도록 부가 metadata는 뒤에 둡니다.
3. 현재 EIP를 포함하는 descriptor가 executable이고 `OBJBIGDEF`가 없을 때만 이
   관찰된 `66 CB` 형식을 선택합니다. 32-bit default code에서 같은 bytes가 다른
   operand-size를 의미할 수 있으므로 bytes만으로 전역 처리하지 않습니다.
4. Intel의 32-bit far-return stack form에 맞춰 CS slot을 dword로 읽고 low 16-bit만
   selector로 사용하며, frame 전체는 8 bytes를 소비합니다.
5. target descriptor가 executable이고 offset이 limit 안에 있으며 target byte가
   guest arena에서 읽히는 경우에만 context를 변경합니다. 어느 검증이든 실패하면
   기존 `CC` boundary를 유지합니다.
6. 기존 near return resolver와 `1E7Fh` HLE는 변경하지 않습니다.

## 검증 기준

* selector/frame probe가 `offset=0x010F0232`, `selector=0x0024`, `stack_bytes=8`,
  target linear `0x010F0232`를 확인합니다.
* non-executable target, out-of-limit offset, 32-bit default current code는 거절됩니다.
* Linux x64 `repiu`와 `repiu_core_probe`가 빌드됩니다.
* core probe 전체가 통과합니다.
* probe-success runtime이 `0x01100040` boundary를 통과한 뒤 wrapper의 다음
  실제 instruction boundary를 관찰합니다.

참고: [Intel RET reference](https://www.felixcloutier.com/x86/ret),
[Intel PUSH reference](https://www.felixcloutier.com/x86/push),
[Open Watcom LE flag definitions](https://github.com/open-watcom/open-watcom-v2/blob/master/bld/watcom/h/exeflat.h#L1186-L1243).

---

# Design 20260905-604 — Linux x64 observed far-return frame HLE

## Purpose

After Task 603 separated `66 CB` at `0x01100040` into a fail-closed boundary,
the stack at that boundary was observed again. At `ESP=0x0158CC54`,
`[ESP]=0x010F0232` and `[ESP+4]=0x00000024` are present. Object 2 pushed
`CS` and `0x010F0232`, while object 3 executes `66 CB` in a 16-bit code object
with an operand-size override, so these values are the offset and padded CS slot.

Intel's RET definition says that the 32-bit far-return path pops EIP and CS as
32-bit stack items and discards the upper 16 bits of CS. The observed frame is
therefore consumed with `ESP += 8`. This unit validates that exact frame shape
against selector-table object-mode metadata and executes it through Linux x64 HLE.

## Scope

* Carry executable and big/default LE object information into selector metadata.
* Handle `66 CB` as a 32-bit far return only when the current code descriptor is
  16-bit default and the target descriptor is executable.
* Read the offset dword and CS dword from the stack, using the CS low word as the
  selector.
* Validate the target selector/offset, including the observed relocated-linear
  frame representation when bounded by the target descriptor window, then
  update `EIP`, `SegCs`, and `ESP += 8`.
* Verify the behavior with a pure selector/frame probe and the Linux x64 runtime.

## Out of scope

* Generalizing 16-bit operand-size far returns (`CB` or other mode combinations)
* Privilege-level changes, call gates, or outer-stack switching
* Modeling a separate SS descriptor B-bit not represented by the current engine
* Changing the private `INT 31h AX=1E7Fh` ABI
* Repairing `0x000000FF` as a return address

## Design decisions

1. Keep raw LE object flags and code-mode metadata in separate fields from the
   existing descriptor access flags.
2. Carry object flags through relocated selector bindings, appending metadata so
   existing aggregate initialization remains compatible.
3. Select this observed `66 CB` form only when the current EIP belongs to an
   executable descriptor without `OBJBIGDEF`. Bytes alone cannot be handled
   globally because the same bytes can have another operand size in 32-bit code.
4. Read the CS slot as a dword, use only its low 16 bits as the selector, and
   consume the complete 8-byte frame.
5. Try a selector-relative target first. For the observed frame, accept a raw
   relocated linear target only when it lies inside the target descriptor's
   mapped linear window.
6. Change the context only when the target is executable, validated by one of
   those bounded representations, and readable in the guest arena. Any failed
   validation declines and preserves the
   existing `CC` boundary.
7. Leave the generic near-return resolver and `1E7Fh` HLE unchanged.

## Verification criteria

* The selector/frame probe confirms `offset=0x010F0232`, `selector=0x0024`,
  `stack_bytes=8`, and target linear `0x010F0232`.
* Non-executable targets, out-of-limit offsets, and 32-bit-default current code
  are rejected.
* Linux x64 `repiu` and `repiu_core_probe` build successfully.
* The complete core probe passes.
* Probe-success runtime crosses the `0x01100040` boundary and reaches the next
  real wrapper instruction boundary.

References: [Intel RET reference](https://www.felixcloutier.com/x86/ret),
[Intel PUSH reference](https://www.felixcloutier.com/x86/push),
[Open Watcom LE flag definitions](https://github.com/open-watcom/open-watcom-v2/blob/master/bld/watcom/h/exeflat.h#L1186-L1243).

## 추가 판단

실행 진단에서 raw frame offset `0x010F0232`와 target selector `0x0024`가
확인되었습니다. selector `0x0024`의 relocated base는 `0x01010000`이고
limit은 `0x000EBBDF`이므로 raw 값은 selector-relative offset으로는 범위를
벗어나지만 target descriptor의 relocated linear window 안에 있습니다. 따라서
resolver는 정상적인 selector-relative 해석을 먼저 시도하고, 이 descriptor
window 검증을 통과한 경우에만 관찰된 absolute linear 표현을 인정합니다.

## Additional judgment

Runtime diagnosis confirmed raw frame offset `0x010F0232` with target selector
`0x0024`. Selector `0x0024` has relocated base `0x01010000` and limit
`0x000EBBDF`; the raw value is out of range as a selector-relative offset but
inside the target descriptor's relocated linear window. The resolver therefore
tries the normal selector-relative form first and accepts the observed absolute
linear form only after that descriptor-window check.
