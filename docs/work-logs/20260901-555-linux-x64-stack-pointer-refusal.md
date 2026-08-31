# 20260901-555 x64 lowering의 stack pointer 구멍 작업 로그

설계: [20260901-555](../design/20260901-555-linux-x64-stack-pointer-refusal.md) ·
작업 지시: [20260901-555](../work-orders/20260901-555-linux-x64-stack-pointer-refusal.md)

## 한국어

### 결과

guest `ESP`를 host `RSP`로 읽던 구멍을 닫았습니다. stack pointer를 이름 부르는 명령은
이제 `kStackPointerRegister` 사유로 거절되고, emitter는 `0xCC` + 경계로 냅니다.

### 어떻게 찾았나

x64 진행 현황을 설명하려고 판정기를 다시 읽다가 나왔습니다. **코드를 고치다 발견한 것이
아니라, 이미 한 일을 남에게 설명하려다 발견했습니다.**

Task 552의 lowering은 "주소가 하위 4 GiB인가"만 봅니다. base register가 무엇을 담고
있는지는 보지 않습니다. 그런데 Task 546 결정 3은 host RSP를 SysV stack으로 남기고 guest
ESP는 state로 두기로 이미 정해 두었습니다. 두 문장을 붙이면 `ESP`는 guest의 스택이 아닙니다.

### 두 번째 형태가 더 나빴습니다

| 형태 | 이전 판정 | 실제 결과 |
|---|---|---|
| `mov eax,[esp+8]` | `kAddressSizePrefix` | host stack을 읽음 — 잘못된 데이터 |
| `add esp,16` | **`kIdenticalBytes`** | host RSP를 씀 — **host stack pointer 파괴** |

두 번째는 memory operand가 없어서 Task 552의 lowering 경로를 아예 지나가지 않습니다.
`kIdenticalBytes` 경로로 통과했으므로 **Task 552가 아니라 Task 550부터 있던 구멍**이고,
Task 553이 그것을 emitter에 연결했습니다.

### 측정 — 검사를 껐다 켰습니다

```text
검사 끔:  copied=2, lowered=2, refused=2
          long_mode_stack_refused_add_esp_imm8=false (여섯 항목 전부 false)
검사 켬:  copied=1, lowered=2, refused=3
```

`copied`가 2였다는 것은 **`add esp,16`이 cache에 그대로 복사되고 있었다**는 뜻입니다.
추론이 아니라 이 숫자가 그것을 말합니다.

### 말하지 않은 전제를 적었습니다

구멍의 원인은 실수 하나가 아니라 적히지 않은 전제였습니다. Task 552의 lowering이 옳으려면
**lowering된 명령이 실행되는 시점에 guest GPR *n*이 host GPR *n*에 있어야** 합니다.
`0x67`이 뜻을 갖는 이유가 그것인데, 어디에도 적혀 있지 않았습니다.

이제 헤더의 `LongModeLowering` 옆에 문장으로 있습니다. x64 emitter의 register mapping은
아직 결정되지 않았으므로 대부분의 register에 대해 이것은 **미확정**이고, `ESP`만이
**이미 거짓으로 결정된** 경우입니다.

`EBP`를 함께 거절하지 않은 이유도 같습니다. 결정 3은 `RSP`만 지목합니다. `EBP`는 `EAX`나
`EBX`와 같은 정도로 미확정이지 더 나쁘지 않고, 미확정을 이유로 거절하면 Task 551의 측정이
연 memory operand 허용 자체를 되돌리는 일이 됩니다.

### 측정

| Host | 결과 |
|---|---|
| Win32 x86 Debug | `core_probe_all=true`, 19/19 |
| Linux x64 Release | `core_probe_all=true`, 19/19, skipped 2 |
| Linux i386 Release | `core_probe_all=true`, 19/19 |

```text
long_mode_stack_pointer_refused=true,non_stack_base_still_lowered=true
long_mode_emission_counts=true,copied=1,lowered=2,refused=3
```

`non_stack_base_still_lowered=true`가 같은 줄에 있는 것이 의도입니다. **거절이 표적이지
담요가 아니라는 것**을 같은 자리에서 보여야, 다음 사람이 "memory operand를 다 막았구나"로
읽지 않습니다.

### 남은 것

옳은 lowering은 guest `ESP`를 state에서 꺼내는 명시적 시퀀스이고, 그것은 stack 명령
lowering과 같은 일입니다. 다음 단위입니다.

## English

### Result

The hole that read the host `RSP` where the guest meant `ESP` is closed. An instruction
naming the stack pointer is refused with `kStackPointerRegister`, and the emitter puts out
`0xCC` plus a boundary.

### How it was found

It came up while re-reading the classifier to explain the x64 status. **Not while changing
code -- while trying to explain work already done.**

Task 552's lowering asks only whether the address is below 4 GiB. It never asks what the
base register holds. But Task 546's decision 3 had already settled that host RSP stays the
SysV stack and guest ESP is state. Put the two sentences together and `ESP` is not the
guest's stack.

### The second shape was the worse one

| Shape | Previous verdict | What actually happened |
|---|---|---|
| `mov eax,[esp+8]` | `kAddressSizePrefix` | reads the host stack -- wrong data |
| `add esp,16` | **`kIdenticalBytes`** | writes host RSP -- **destroys the host stack pointer** |

The second has no memory operand, so it never entered Task 552's lowering path at all. It
passed on the `kIdenticalBytes` path, which makes it **a hole from Task 550 rather than
Task 552** -- and Task 553 wired it into the emitter.

### Measured -- the check off and on

```text
check off:  copied=2, lowered=2, refused=2
            long_mode_stack_refused_add_esp_imm8=false (all six items false)
check on:   copied=1, lowered=2, refused=3
```

`copied` being 2 is the statement that **`add esp,16` was being copied into the cache
verbatim.** That number says it; no inference is involved.

### The unstated premise is now written down

The cause was not one slip but something never written. For Task 552's lowering to be
correct, **guest GPR *n* must be in host GPR *n* at the moment a lowered instruction runs.**
That is what makes `0x67` mean anything, and it appeared nowhere.

It now sits beside `LongModeLowering` in the header as a sentence. The x64 emitter's
register mapping is undecided, so for most registers this is **undecided** rather than
established, and `ESP` is the one case already **decided false**.

That is also why `EBP` is not refused with it. Decision 3 names `RSP` only. `EBP` is as
undecided as `EAX` or `EBX`, not worse, and refusing on undecidedness would undo the
memory-operand admission that Task 551's measurement opened.

### What was measured

| Host | Result |
|---|---|
| Win32 x86 Debug | `core_probe_all=true`, 19 of 19 |
| Linux x64 Release | `core_probe_all=true`, 19 of 19, 2 skipped |
| Linux i386 Release | `core_probe_all=true`, 19 of 19 |

```text
long_mode_stack_pointer_refused=true,non_stack_base_still_lowered=true
long_mode_emission_counts=true,copied=1,lowered=2,refused=3
```

`non_stack_base_still_lowered=true` sharing that line is deliberate. **Showing the refusal
is targeted rather than a blanket**, in the same place, is what stops the next reader
concluding that memory operands were all shut off.

### What is left

The correct lowering materialises guest `ESP` from state, which is the same work as
lowering the stack instructions. That is the next unit.
