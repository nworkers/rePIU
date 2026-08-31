# 20260901-559 x64 stack 명령 lowering 작업 로그

설계: [20260901-559](../design/20260901-559-x64-stack-instruction-lowering.md) ·
작업 지시: [20260901-559](../work-orders/20260901-559-x64-stack-instruction-lowering.md)

## 한국어

### 결과 — `operand-width`가 8,217에서 3으로

| 항목 | Task 557 | 지금 | 차 |
|---|---:|---:|---:|
| lowering | 12,242 | 20,456 | **+8,214** |
| **방출 가능** | 52.46% | **66.17%** | +13.71%p |
| 거절 | 28,479 | 20,265 | −8,214 |
| `operand-width` | 8,217 | **3** | −8,214 |
| 완결 block | 316 (2.21%) | 381 (2.66%) | +65 |

`operand-width`가 사실상 사라졌습니다. 남은 3건은 `PUSH`/`POP` `FS`·`GS`와 `FF /6`처럼
설계가 명시적으로 남겨 둔 형태입니다.

### 이번 단위의 핵심은 `LEA`였습니다

guest `PUSH`와 `POP`은 **flag를 바꾸지 않습니다.** `sub r15d, 4`는 바꿉니다. 그래서 `ESP`
조정을 전부 `LEA`로 했습니다.

이것이 왜 중요한지는 **반대로 해 보면** 드러납니다. `LEA`를 `SUB`/`ADD`로 바꾸면 — 바이트
수도 4로 같고 명령 개수도 1로 같습니다 — 이렇게 됩니다.

```text
guest_stack_data=true                                  ← 값은 전부 맞음
  zf_after_push_pop observed=0x0 expected=0x1  MISMATCH ← flag만 파괴됨
```

**데이터는 완벽히 맞고 flag만 틀립니다.** 값을 보는 시험은 이것을 영원히 못 잡습니다.
guest는 `push`/`pop` 다음의 `jz`에서 조용히 다른 길로 갑니다.

> 값이 틀리면 스스로 드러납니다. flag가 틀리면 아무것도 일으키지 않습니다.

### 실행으로 확인했습니다

Task 558의 harness 위에 세 시나리오를 올렸습니다.

```text
  edx_from_pop        observed=0x66778899  expected=0x66778899
  esp_after           observed=0x200017fc  expected=0x200017fc
  guest_stack_memory  observed=0xa1b2c3d4  expected=0xa1b2c3d4
guest_stack_data=true

  zf_after_push_pop   observed=0x1  expected=0x1
  edi_from_pop        observed=0xbadf00d  expected=0xbadf00d
guest_stack_flags=true

  zf_restored         observed=0x1  expected=0x1
  esp_balanced        observed=0x20001800  expected=0x20001800
guest_stack_flags_round_trip=true
```

`guest_stack_memory`는 레지스터가 아니라 **guest stack 메모리를 직접 읽은 값**입니다.
`push`가 실제로 게스트 메모리에 썼다는 것을 레지스터를 거치지 않고 확인합니다.

flag 시나리오에 `edi_from_pop`을 함께 둔 것은 의도입니다 — flag가 보존됐다는 결과가
**아무 일도 안 한 시퀀스** 때문이 아님을 같은 자리에서 보입니다.

### `PUSHFD`/`POPFD`는 host stack을 경유합니다

long mode에는 32비트 `PUSHFD`가 없고, flag를 레지스터로 꺼내는 명령도 없습니다. 그래서
`pushfq` → `pop r14` → guest stack에 4바이트, 되돌릴 때는 그 역순입니다. host `RSP`는
들어올 때와 나갈 때가 같고, `esp_balanced`가 그것을 확인합니다.

### 손으로 쓴 인코딩 표를 기계가 검사합니다

11개 시퀀스를 long mode 디코더로 읽어 **명령 하나하나의 mnemonic과 개수**를 확인합니다.
Task 557에서 쓴 방법 그대로입니다.

`6A imm8`의 부호확장은 항목을 따로 뒀습니다. `push -1`을 `0x000000FF`로 밀면 guest 스택에
다른 값이 들어가고 **아무 신호도 없습니다.**

### 검증기의 규칙을 약하게 만들지 않고 정확하게 만들었습니다

Task 553은 long mode 방출에서 map entry 하나가 명령 **하나**임을 확인했고, 그것이 "조용히
다른 명령"을 잡는 힘이었습니다. stack 시퀀스는 명령이 여럿이므로 그대로 두면 옳은 시퀀스가
decode failure가 됩니다.

규칙을 지우는 대신 `LowerLongModeBytes`가 **낸 명령 개수를 함께 보고**하고, 방출 루프가
entry별로 기억하고, 검증이 **디코드된 개수 == 방출자가 의도한 개수**를 봅니다. 길이만 맞고
개수가 다른 바이트열은 여전히 잡힙니다.

기대 개수는 `AotAddressMapEntry`에 필드를 더하지 않고 방출 함수 안의 지역 벡터에 두었습니다 —
검증이 같은 함수 몇십 줄 아래에서 돌고, 배치 이후에는 아무도 쓰지 않기 때문입니다.

### 측정

| Host | 결과 |
|---|---|
| Linux x64 Release | `core_probe_all=true`, 20/20, skipped 2 |
| Linux i386 Release | `core_probe_all=true`, 19/19, skipped 3 |
| Win32 x86 Debug | `core_probe_all=true`, 19/19, skipped 3 |

census는 Windows와 Linux x64가 같고 `agrees=true`입니다.

### 남은 것

`not-a-copy-record` 12,856(control flow)과 `stack-pointer` 6,401(`ESP`를 피연산자로 쓰는
일반 명령)이 남았습니다. 완결 block이 2.66%에 그친 것은 **block이 control flow로 끝나기
때문**이고, 그것이 다음 묶음입니다.

## English

### Result -- `operand-width` from 8,217 to 3

| Item | Task 557 | Now | Change |
|---|---:|---:|---:|
| Lowered | 12,242 | 20,456 | **+8,214** |
| **Emittable** | 52.46% | **66.17%** | +13.71pp |
| Refused | 28,479 | 20,265 | −8,214 |
| `operand-width` | 8,217 | **3** | −8,214 |
| Complete blocks | 316 (2.21%) | 381 (2.66%) | +65 |

`operand-width` is effectively gone. The three that remain are the forms the design
explicitly left: `PUSH`/`POP` of `FS`/`GS` and `FF /6`.

### The heart of this unit was `LEA`

Guest `PUSH` and `POP` **change no flags.** `sub r15d, 4` does. So every `ESP` adjustment
is a `LEA`.

Why that matters shows up **when it is done the other way.** Replacing the `LEA` with
`SUB`/`ADD` -- the same four bytes, the same single instruction -- gives:

```text
guest_stack_data=true                                  <- every value correct
  zf_after_push_pop observed=0x0 expected=0x1  MISMATCH <- only the flags destroyed
```

**The data is perfect and only the flags are wrong.** A value-based test would never catch
it, and the guest would quietly take the other branch at the `jz` after its `push`.

> A wrong value announces itself. Wrong flags raise nothing.

### Confirmed by execution

Three scenarios on Task 558's harness.

```text
  edx_from_pop        observed=0x66778899  expected=0x66778899
  esp_after           observed=0x200017fc  expected=0x200017fc
  guest_stack_memory  observed=0xa1b2c3d4  expected=0xa1b2c3d4
guest_stack_data=true

  zf_after_push_pop   observed=0x1  expected=0x1
  edi_from_pop        observed=0xbadf00d  expected=0xbadf00d
guest_stack_flags=true

  zf_restored         observed=0x1  expected=0x1
  esp_balanced        observed=0x20001800  expected=0x20001800
guest_stack_flags_round_trip=true
```

`guest_stack_memory` is **read out of guest stack memory directly**, not from a register --
confirming the push reached memory without going through the register that pushed it.

`edi_from_pop` sits beside the flag result on purpose: it shows in the same place that the
flags survived a sequence that actually did something, rather than one that did nothing.

### `PUSHFD`/`POPFD` go through the host stack

Long mode has no 32-bit `PUSHFD` and no instruction that moves flags into a register, so it
is `pushfq` then `pop r14` then four bytes to the guest stack, and the reverse coming back.
Host `RSP` leaves as it arrived, which `esp_balanced` checks.

### A hand-written encoding table, checked by machine

All eleven sequences are read back with a long-mode decoder, checking **each instruction's
mnemonic and the count**. The same method as Task 557.

`6A imm8`'s sign extension has its own item: pushing `-1` as `0x000000FF` puts a different
value on the guest's stack **with no signal at all.**

### The verifier's rule was made exact rather than weaker

Task 553 checked that a long-mode map entry is **one** instruction, and that rule was what
caught "quietly a different instruction". A stack sequence is several, so leaving it would
report correct sequences as decode failures.

Instead of dropping the rule, `LowerLongModeBytes` now **reports how many instructions it
produced**, the emit loop remembers that per entry, and verification checks **decoded count
equals the count the emitter intended**. A byte string of the right length with the wrong
instruction count is still caught.

The expected counts live in a local vector inside the emit function rather than a new field
on `AotAddressMapEntry`, because verification runs a few dozen lines below in the same
function and nothing after placement wants them.

### What was measured

| Host | Result |
|---|---|
| Linux x64 Release | `core_probe_all=true`, 20 of 20, 2 skipped |
| Linux i386 Release | `core_probe_all=true`, 19 of 19, 3 skipped |
| Win32 x86 Debug | `core_probe_all=true`, 19 of 19, 3 skipped |

The census agrees between Windows and Linux x64, with `agrees=true`.

### What is left

`not-a-copy-record` at 12,856 (control flow) and `stack-pointer` at 6,401 (ordinary
instructions naming `ESP`). Complete blocks only reached 2.66% because **a block ends in
control flow**, and that is the next pair.
