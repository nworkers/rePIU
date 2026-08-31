# 20260901-559 x64 stack 명령 lowering / Lowering the stack instructions

상위 설계: [20260831-546 x64 AOT/DBT 실행 모델](20260831-546-linux-x64-aot-dbt-execution-model.md) ·
직접 기반: [20260901-558 guest 상태 배치](20260901-558-x64-guest-register-placement.md) ·
근거: [20260901-556 방출 비율 census](20260901-556-x64-emittable-fraction-census.md) ·
현황: [Linux 이식 frontier](../analysis/linux-port-frontier.md)

## 한국어

### 목적

long mode에는 32비트 `PUSH`·`POP`이 **없습니다**. `66` 접두는 16비트를 뜻하지 32비트가
아니므로, 되돌릴 방법이 인코딩에 존재하지 않습니다. Task 550이 이것들을
`kNeedsReencode`로 표시만 하고 변환을 남겨 둔 이유입니다.

Task 558이 guest `ESP`를 `R15D`에 두기로 정했으므로, 이제 그 변환을 쓸 수 있습니다.

census 기준 `operand-width` **8,217건**이 이 단위의 대상이고, 실질적으로 전부
`push`(4,923)와 `pop`(3,291)입니다.

### 범위 — stack만, control flow는 다음

Task 556은 stack과 control flow가 한 묶음이라고 적었습니다. 그것은 **연쇄가 생기려면 둘 다
필요하다**는 뜻이지 한 작업이라는 뜻이 아닙니다. `CALL`·`RET`은 stack만이 아니라 `EIP`를
바꾸므로 dispatch resolver와 함께 가야 합니다.

| 이 단위 | 다음 |
|---|---|
| `PUSH`/`POP` r32·imm, `PUSHFD`/`POPFD`, `LEAVE` | `CALL`·`RET`·`JMP` (resolver와 함께) |
| | ESP를 피연산자로 쓰는 일반 명령 6,401건 (별도 재인코더) |

### 결정

#### 1. `ESP` 조정은 `LEA`로 합니다. `SUB`/`ADD`가 아닙니다

이것이 이 설계에서 가장 중요한 항목입니다.

guest `PUSH`와 `POP`은 **flag를 바꾸지 않습니다.** 그런데 `sub r15d, 4`는 바꿉니다. 그대로
쓰면 guest가 보는 flag가 조용히 달라지고, 바로 다음 `jz`가 다른 길로 갑니다 — 아무것도
일으키지 않는 종류의 오류입니다.

`LEA`는 flag를 건드리지 않습니다.

```text
lea r15d, [r15 - 4]     45 8D 7F FC
lea r15d, [r15 + 4]     45 8D 7F 04
```

32비트 목적지이므로 결과가 상위 절반을 0으로 만들고(Task 558 결정 3 유지), 64비트로 계산한
뒤 32비트로 자르므로 **guest의 wraparound도 그대로**입니다.

#### 2. `R14D`가 emitter의 scratch입니다

Task 558이 비워 둔 `R14`를 여기서 씁니다. `PUSHFD`·`POPFD`·`push esp`가 중간값을 둘 곳을
필요로 하고, guest register 여덟 개는 전부 mapping에 묶여 있습니다.

`R15`와 같은 이유로 안전합니다 — **32비트 인코딩은 `R14`를 이름 부를 수 없습니다.**

#### 3. `PUSHFD`/`POPFD`는 host stack을 경유합니다

long mode에는 32비트 `PUSHFD`가 없고 `PUSHFQ`(8바이트)만 있습니다. flag를 레지스터로
꺼내는 명령은 없으므로, host stack을 **균형 잡힌 임시 통로**로 씁니다.

```text
pushfd →  9C              pushfq            (host stack에 8바이트)
          41 5E           pop r14
          45 8D 7F FC     lea r15d,[r15-4]
          45 89 37        mov [r15], r14d   (guest stack에 4바이트)

popfd  →  45 8B 37        mov r14d,[r15]
          45 8D 7F 04     lea r15d,[r15+4]
          41 56           push r14
          9D              popfq
```

host `RSP`는 들어올 때와 나갈 때가 같습니다. `R14D`로 쓰면 상위 절반이 0이므로 `popfq`가
읽는 상위 32비트는 0이고, 그것이 `RFLAGS` 예약 영역의 올바른 값입니다.

#### 4. 시퀀스 표

`r`은 guest register 번호(0–7), `ESP`(4)는 아래 특례입니다.

| guest | 낮춘 것 | 바이트 |
|---|---|---|
| `50+r` `push r32` | `lea r15d,[r15-4]` · `mov [r15], r32` | `45 8D 7F FC` · `41 89 (07\|r<<3)` |
| `58+r` `pop r32` | `mov r32,[r15]` · `lea r15d,[r15+4]` | `41 8B (07\|r<<3)` · `45 8D 7F 04` |
| `68 id` `push imm32` | `lea` · `mov dword [r15], imm32` | `45 8D 7F FC` · `41 C7 07 id` |
| `6A ib` `push imm8` | 같음, imm8을 **부호확장**한 imm32 | |
| `9C` `pushfd` | 결정 3 | |
| `9D` `popfd` | 결정 3 | |
| `C9` `leave` | `mov r15d,ebp` · `mov ebp,[r15]` · `lea r15d,[r15+4]` | `41 89 EF` · `41 8B 2F` · `45 8D 7F 04` |

**특례.** `54` `push esp`는 **감소 전의** `ESP`를 밀어야 하므로 scratch를 거칩니다
(`mov r14d,r15d` · `lea` · `mov [r15],r14d`). `5C` `pop esp`는 증가가 적재값에 덮이므로
`mov r15d,[r15]` 하나입니다.

`6A`의 부호확장을 따로 적어 둡니다. `push -1`을 `0x000000FF`로 밀면 아무 신호 없이 다른
값이 스택에 들어갑니다.

#### 5. 아직 거절하는 것

* `FF /6` `PUSH r/m`, `8F /0`의 메모리 형태 — 두 번째 메모리 피연산자가 자체적으로 `ESP`를
  쓸 수 있고, 그것은 다음 단위(일반 재인코더)의 일입니다. `8F`의 `mod=11` 형태는
  `58+r`과 같으므로 함께 낮춥니다.
* `PUSH`/`POP` `FS`·`GS` — segment register, 결정 5(Task 546) 그대로.
* `CALL`·`RET`·`JMP` — resolver와 함께.

#### 6. 검증기의 "명령 하나" 규칙을 고칩니다

Task 553은 long mode 방출에서 **map entry 하나 = 명령 하나**를 확인합니다. 그 규칙이
"조용히 다른 명령"을 잡는 힘이었습니다(553에서 측정). 이제 lowering이 **여러 명령**을
내므로 그대로 두면 옳은 시퀀스가 decode failure가 됩니다.

규칙을 약하게 만들지 않고 **정확하게** 만듭니다. `LowerLongModeBytes`가 몇 개의 명령을
냈는지 함께 보고하고, 방출 루프가 그것을 entry별로 기억하고, 검증이 **디코드된 개수가
방출자가 의도한 개수와 같은지** 봅니다. 길이만 맞고 개수가 다른 바이트열은 여전히
잡힙니다.

기대 개수는 `AotAddressMapEntry`에 필드를 더하지 않고 **방출 함수 안의 지역 벡터**에
둡니다 — 검증이 같은 함수에서 바로 뒤에 돌고, 배치 이후에는 쓰이지 않기 때문입니다.

### 검증 — 디코드하고, 실행합니다

Task 558의 harness가 있으므로 두 층으로 봅니다.

1. **디코드.** 낮춘 바이트를 long mode 디코더로 읽어 시퀀스가 의도한 명령들인지 확인합니다.
   손으로 옮긴 인코딩 표를 기계가 검사하는 Task 557의 방식 그대로입니다.
2. **실행.** guest 프로그램을 실제로 돌립니다.
   * `push`한 값이 **guest stack 메모리에 실제로** 있는지 — `R15D`가 가리키는 곳을 읽어서.
   * `pop`이 그 값을 되돌려 주는지.
   * `push`/`pop` 뒤 `ESP`가 정확히 ±4인지.
   * **flag가 보존되는지.** 결정 1을 시험하는 항목입니다: `cmp`로 flag를 정한 뒤
     `push`/`pop`을 사이에 넣고 `jz`가 같은 길로 가는지 봅니다. `LEA` 대신 `SUB`를 쓰면
     이 항목이 실패해야 합니다.
   * `pushfd`/`popfd` 왕복이 flag를 복원하는지.

flag 항목이 이 단위의 핵심입니다. 나머지는 값이 틀리면 바로 보이지만, flag는 **틀려도
아무것도 일으키지 않습니다.**

### 비범위

* `CALL`·`RET`·`JMP`와 dispatch resolver.
* `ESP`를 피연산자로 쓰는 일반 명령의 재인코딩(6,401건).
* guest `EIP`를 register에 두는 것.

## English

### Objective

Long mode **has no 32-bit `PUSH` or `POP`**. A `66` prefix asks for 16 bits, not 32, so
there is no encoding that gets back. That is why Task 550 marked these `kNeedsReencode` and
left the transform undone.

Task 558 settled guest `ESP` into `R15D`, so the transform can now be written.

The target is the census's `operand-width` group -- **8,217 instructions**, essentially all
`push` (4,923) and `pop` (3,291).

### Scope -- the stack only; control flow next

Task 556 called stack and control flow one pair. That means **a chain needs both**, not
that they are one task. `CALL` and `RET` change `EIP` as well as the stack, so they belong
with the dispatch resolver.

| This unit | Next |
|---|---|
| `PUSH`/`POP` r32 and imm, `PUSHFD`/`POPFD`, `LEAVE` | `CALL`, `RET`, `JMP` (with the resolver) |
| | the 6,401 ordinary instructions that name `ESP` (a general re-encoder) |

### Decisions

#### 1. Adjust `ESP` with `LEA`, not `SUB`/`ADD`

This is the most important item in the design.

Guest `PUSH` and `POP` **do not change flags.** `sub r15d, 4` does. Using it would quietly
alter the flags the guest sees, and the very next `jz` would take the other branch -- the
kind of error that raises nothing.

`LEA` touches no flags.

```text
lea r15d, [r15 - 4]     45 8D 7F FC
lea r15d, [r15 + 4]     45 8D 7F 04
```

The 32-bit destination zeroes the upper half, keeping Task 558's decision 3, and computing
in 64 bits then truncating to 32 reproduces **the guest's own wraparound**.

#### 2. `R14D` is the emitter's scratch

Task 558 left `R14` free; this is what takes it. `PUSHFD`, `POPFD` and `push esp` need
somewhere for an intermediate, and all eight guest registers are spoken for by the mapping.

It is safe for the same reason `R15` is: **a 32-bit encoding cannot name `R14`.**

#### 3. `PUSHFD`/`POPFD` go through the host stack

Long mode has no 32-bit `PUSHFD`, only `PUSHFQ` at eight bytes, and there is no instruction
that moves flags to a register. So the host stack is used as a **balanced temporary**.

```text
pushfd →  9C              pushfq            (8 bytes on the host stack)
          41 5E           pop r14
          45 8D 7F FC     lea r15d,[r15-4]
          45 89 37        mov [r15], r14d   (4 bytes on the guest stack)

popfd  →  45 8B 37        mov r14d,[r15]
          45 8D 7F 04     lea r15d,[r15+4]
          41 56           push r14
          9D              popfq
```

Host `RSP` leaves as it arrived. Writing `R14D` zeroes the upper half, so the top 32 bits
`popfq` reads are zero, which is the correct value for `RFLAGS`'s reserved range.

#### 4. The sequence table

`r` is the guest register number 0-7; `ESP` (4) is the special case below.

| Guest | Lowered to | Bytes |
|---|---|---|
| `50+r` `push r32` | `lea r15d,[r15-4]` · `mov [r15], r32` | `45 8D 7F FC` · `41 89 (07\|r<<3)` |
| `58+r` `pop r32` | `mov r32,[r15]` · `lea r15d,[r15+4]` | `41 8B (07\|r<<3)` · `45 8D 7F 04` |
| `68 id` `push imm32` | `lea` · `mov dword [r15], imm32` | `45 8D 7F FC` · `41 C7 07 id` |
| `6A ib` `push imm8` | the same, with the imm8 **sign-extended** to imm32 | |
| `9C` `pushfd` | decision 3 | |
| `9D` `popfd` | decision 3 | |
| `C9` `leave` | `mov r15d,ebp` · `mov ebp,[r15]` · `lea r15d,[r15+4]` | `41 89 EF` · `41 8B 2F` · `45 8D 7F 04` |

**Special cases.** `54` `push esp` must push `ESP` **as it was before the decrement**, so it
goes through the scratch (`mov r14d,r15d` · `lea` · `mov [r15],r14d`). `5C` `pop esp` is a
single `mov r15d,[r15]`, because the loaded value overrides the increment.

`6A`'s sign extension is called out on purpose: pushing `-1` as `0x000000FF` puts a
different value on the stack with no signal at all.

#### 5. Still refused

* `FF /6` `PUSH r/m` and the memory form of `8F /0` -- their second memory operand may name
  `ESP` itself, which is the next unit's general re-encoder. `8F` with `mod=11` is the same
  as `58+r` and is lowered with it.
* `PUSH`/`POP` of `FS`/`GS` -- segment registers, per decision 5 of Task 546.
* `CALL`, `RET`, `JMP` -- with the resolver.

#### 6. The verifier's "one instruction" rule is corrected

Task 553 checks **one map entry, one instruction** under long-mode emission, and that rule
was what caught "quietly a different instruction" (measured in 553). Now that a lowering
emits **several** instructions, leaving it would report correct sequences as decode
failures.

The rule is made **exact** rather than weaker. `LowerLongModeBytes` also reports how many
instructions it produced, the emit loop remembers that per entry, and verification checks
that **the decoded count equals the count the emitter intended.** A byte string of the
right length but the wrong instruction count is still caught.

The expected counts live in **a local vector inside the emit function** rather than a new
field on `AotAddressMapEntry`, because verification runs immediately afterwards in the same
function and nothing after placement needs them.

### Verification -- decode it, then run it

Task 558's harness exists, so this is checked at two levels.

1. **Decode.** Read the lowered bytes with a long-mode decoder and confirm the sequence is
   the instructions intended -- Task 557's method, a hand-copied encoding table checked by
   machine.
2. **Execute.** Run a guest program.
   * That a pushed value **is actually in guest stack memory**, read back through what
     `R15D` points at.
   * That a pop returns it.
   * That `ESP` moves by exactly ±4.
   * **That flags survive.** This is the item that tests decision 1: set flags with a
     `cmp`, put a `push`/`pop` between, and see whether `jz` still goes the same way. With
     `SUB` instead of `LEA` this item must fail.
   * That a `pushfd`/`popfd` round trip restores the flags.

The flag item is the heart of it. The others announce themselves when a value is wrong;
**flags being wrong raises nothing at all.**

### Out of scope

* `CALL`, `RET`, `JMP` and the dispatch resolver.
* Re-encoding the 6,401 ordinary instructions that name `ESP`.
* Holding guest `EIP` in a register.
