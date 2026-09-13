# Task 661 작업 로그: Linux x64 epilogue stack delta 진단

## 한국어

### 결과

Task 660 이후의 zero-return frontier에서 `PUSH ES`와 호출·복귀 경계를
분리해 관찰했습니다. 이번 작업은 소스 코드를 변경하지 않았으며,
기존 Debug 빌드와 fail-closed return thunk 동작을 그대로 사용했습니다.

`PUSH ES` watch:

```text
[repiu-watch] event=fault guest=0x010EFEC4 n=1 at=0x2000182B esi=0x00000001 esp=0x0158C84C ebx=0x0158C92C eflags=0x00200202
[repiu-watch] event=step guest=0x010EFEC4 n=1 at=0x010EFEC4 le_bytes=0x00000B24E8E58906 esi=0x00000001 esp=0x0158C84C ebx=0x0158C92C eflags=0x00200302
[repiu-segment-hle-watch] eip=0x010EFEC4 opcode=0x06 selector=0x0024 destination=0x0158C848 value=0x00000024 esp=0x0158C84C->0x0158C848 next_eip=0x010EFEC5 size=1
```

따라서 `PUSH ES`는 selector `0x0024`를 guest dword로 저장하고 ESP를
정확히 4바이트 감소시킵니다.

`0x010F0232` execution trace:

```text
[repiu-exec-trace] #0 eip=0x010F0232 esp=0x0158C84C stack=0x0158C92C eax=0x0000FFFF ebx=0x0158C92C edx=0x0138C679 ebp=0x0158C848 eflags=0x00200346
```

이 capture는 `POP ES` 직전 ESP가 `0x0158C84C`임을 확인합니다. 이후
zero-return frame은 기존과 같이 `guest EIP=0x010F0237`의 `RET`가 zero
target을 관찰한 상태를 보여 줍니다.

### 정적 map 대조

`0x010EFEC4` map은 다음을 확인했습니다.

```text
0x010EFEC4 06                         cache=0x2000182B emitted=CC
0x010EFEC7 E8 24 0B 00 00             CALL 0x010F09F0, fallthrough=0x010EFECC
0x010F0232 07                         cache=0x200018C3 emitted_len=56
```

`0x010F09F0` map은 `PUSH ESI`로 시작해 조건 분기 뒤
`0x010F0A14 POP ESI`, `0x010F0A15 RET`로 끝나는 경로를 포함합니다.
따라서 정적 bytes만으로는 callee가 `RET imm16`으로 추가 조정한다고
볼 수 없습니다.

### stack tail 대조

Task 660 tail을 256 records로 확장한 실행에서 관련 순서는 다음과
같았습니다.

```text
[repiu-x64-return-stack-tail] index=14133 writer=guest-push site=0x010EFE55 ... esp=0x0158C860 value=0x010EFE2C
[repiu-x64-return-stack-tail] index=14134 writer=direct-call site=0x010EFE5A fallthrough=0x010EFE5F esp=0x0158C85C value=0x010EFE5F
[repiu-x64-return-stack-tail] index=14135 writer=guest-push site=0x010EFEC0 ... esp=0x0158C858 value=0x00000000
[repiu-x64-return-stack-tail] index=14136 writer=guest-push site=0x010EFEC1 ... esp=0x0158C854 value=0x010FB81E
[repiu-x64-return-stack-tail] index=14137 writer=guest-push site=0x010EFEC2 ... esp=0x0158C850 value=0x00000001
[repiu-x64-return-stack-tail] index=14138 writer=guest-push site=0x010EFEC3 ... esp=0x0158C84C value=0x0158C92C
[repiu-x64-return-stack-tail] index=14139 writer=direct-call site=0x010EFEC7 fallthrough=0x010EFECC esp=0x0158C844 value=0x010EFECC
```

ring은 emitted native stack write와 direct CALL을 기록하며, HLE `PUSH ES`
자체의 별도 watch line은 위의 `segment-hle-watch`에서 확인했습니다.

### 판정

- **확인됨:** `PUSH ES`는 원인으로 의심한 4바이트를 정상적으로 guest
  ESP에 반영합니다.
- **확인됨:** `0x010EFEC7` direct CALL의 guest fallthrough는
  `0x010EFECC`입니다.
- **확인됨:** `0x010F09F0`에는 plain `RET` 경로가 있습니다.
- **추정:** `PUSH ES` 직후 기대되는 ESP `0x0158C848`과
  `0x010F0232`에서 관측한 `0x0158C84C` 사이의 차이는 CALL/복귀 또는
  `0x010F022C→0x010F0232` 재진입 경계에서 생겼을 가능성이 있습니다.
- **미확정:** 위 두 경계 중 어느 하나가 실제로 4바이트를 올렸는지,
  그리고 `EAX=0x0000FFFF` 상태에서 `0x010F022D MOV EAX,8BADF00D`가
  동적 경로에 포함되었는지는 아직 확인하지 않았습니다.

따라서 stack width나 return semantics를 수정하지 않았습니다. 다음 작업은
`0x010F022C` guest `INT3` 처리 직후의 `F022D` 재진입과
`0x010EFEC7` 호출의 실제 return resolver 경계를 각각 한 번씩 포착해야
합니다.

추가로 `REPIU_GUEST_WATCH=0x010F022C`를 설정한 bounded 실행에서는
`[repiu-watch]` 또는 `[repiu-guest-int3]`가 출력되지 않고 zero-return
frame으로 진행했습니다. 따라서 현재 실행이 실제로 `0x010F022C`를
통과했다는 근거는 없으며, `0x010F022C→0x010F0232`는 후보 경계로만
남깁니다. 다음 capture는 `0x010F0232`로 들어오는 실제 transfer origin을
먼저 확정해야 합니다.

## English

### Result

This task separated the `PUSH ES` and call/return boundaries at the current
zero-return frontier after Task 660. It made no source changes and reused the
existing Debug build and fail-closed return-thunk behavior.

The existing watch confirmed:

```text
[repiu-segment-hle-watch] eip=0x010EFEC4 opcode=0x06 selector=0x0024 destination=0x0158C848 value=0x00000024 esp=0x0158C84C->0x0158C848 next_eip=0x010EFEC5 size=1
```

`PUSH ES` therefore stores selector `0x0024` as a guest dword and decreases
guest ESP by exactly four bytes.

The bounded epilogue trace recorded:

```text
[repiu-exec-trace] #0 eip=0x010F0232 esp=0x0158C84C stack=0x0158C92C eax=0x0000FFFF ebx=0x0158C92C edx=0x0138C679 ebp=0x0158C848 eflags=0x00200346
```

The pre-`POP ES` ESP is thus `0x0158C84C`. The following zero-return frame
remains the same: the `RET` at guest `EIP=0x010F0237` observes a zero target.

The static map confirms that `0x010EFEC7` is a direct CALL to `0x010F09F0`
with fallthrough `0x010EFECC`, while `0x010F09F0` includes a `PUSH ESI`, a
matching `POP ESI`, and a plain `RET` at `0x010F0A15`. The stack tail showed
the four register pushes at `0x010EFEC0`–`0x010EFEC3` followed by the
`0x010EFEC7` fallthrough write at guest ESP `0x0158C844`.

The result is:

- **Confirmed:** `PUSH ES` applies its four-byte guest stack effect.
- **Confirmed:** the direct CALL fallthrough is `0x010EFECC`.
- **Confirmed:** the callee has a plain `RET` path.
- **Inferred:** the difference between the expected post-`PUSH ES` ESP
  `0x0158C848` and the observed `0x0158C84C` at `0x010F0232` may arise in
  the CALL/return or `0x010F022C`→`0x010F0232` reentry boundary.
- **Unresolved:** which boundary actually increases ESP by four, and whether
  the dynamic path containing `EAX=0x0000FFFF` executed the static
  `0x010F022D MOV EAX,8BADF00D` slot.

No stack-width or return-semantics change was applied. The next task should
capture the `F022D` reentry immediately after guest `INT3` at `0x010F022C`
and the actual return-resolver boundary for the `0x010EFEC7` call separately.

An additional bounded run with `REPIU_GUEST_WATCH=0x010F022C` emitted neither
`[repiu-watch]` nor `[repiu-guest-int3]` before reaching the zero-return frame.
The current run therefore does not prove that it passed through
`0x010F022C`; that edge remains only a candidate. The next capture must first
identify the actual transfer origin entering `0x010F0232`.
