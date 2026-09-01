# 20260901-564 x64 `ESP` operand 재인코딩 설계

## 한국어

### 목적

`ESP`를 이름으로 쓰는 명령을 `R15D`를 쓰도록 다시 인코딩합니다. Task 563이 잰
**진입점에서 도달 가능한 block 1개**를 막고 있는 것이 정확히 이것입니다 — 진입 다음
block의 첫 명령이 `stack-pointer`로 거부됩니다.

### 왜 거부되고 있었는가

Task 546 결정 3이 host `RSP`를 SysV stack으로 두고 guest `ESP`를 `R15D`에 놓았습니다.
그러므로 long mode에서 `ESP`를 이름으로 쓰는 바이트는 guest의 stack이 아니라 **host의
것**에 닿습니다. Task 555가 그것을 거부로 만든 이유이고, 거부는 옳았습니다 — 없는
것은 재인코딩이었습니다.

### 세 자리, 한 가지 변환

`ESP`는 encoding 상 세 곳에 나타날 수 있고, Zydis의 `operand.encoding`이 어디인지
말해 줍니다.

| 자리 | 예 | 바꿀 것 |
|---|---|---|
| ModRM `rm` (`mod=11`) | `add esp, 16` | `rm`을 `111`로, `REX.B` |
| ModRM `reg` | `mov eax, esp` | `reg`를 `111`로, `REX.R` |
| SIB `base` | `mov eax, [esp+8]` | SIB `base`를 `111`로, `REX.B` |

셋 다 "필드를 `111`로 바꾸고 대응하는 `REX` 비트를 세운다"는 한 가지 변환입니다. 한
명령이 둘을 동시에 가질 수 있으므로(`mov esp, esp`) 비트는 독립적으로 세웁니다.

**`ESP`는 SIB index가 될 수 없습니다.** `index=100`이 "index 없음"을 뜻하는 인코딩이라
그 자리는 존재하지 않습니다.

### `REX`는 어디에 넣는가

legacy prefix 뒤, opcode 앞입니다. guest는 32비트 코드이므로 `REX`가 이미 있을 수
없고, 따라서 항상 **삽입**이며 명령이 한 바이트 길어집니다.

Zydis가 opcode의 offset을 주므로 그 앞까지를 prefix로 복사하고, `REX`를 쓰고, 나머지를
그대로 복사합니다. displacement와 immediate는 손대지 않습니다.

### 주소는 64비트로 계산합니다

메모리 base를 `R15`로 바꾼 뒤 **`0x67`을 붙이지 않습니다.** `R15`의 상위 절반이 0이고
guest memory가 하위 4 GiB에 있으므로(Task 551), 64비트 계산 결과가 곧 guest 주소입니다.
Task 559의 push 시퀀스가 이미 `mov dword ptr [r15], imm32`로 같은 관례를 씁니다.

**대신 32비트 wraparound가 보존되지 않습니다.** `ESP + disp`가 32비트를 넘어 감싸는
경우 64비트 계산은 감싸지 않습니다. arena가 `0x085E7000` 아래라 현재 구성에서는 일어날
수 없지만, **우연한 안전이지 규칙이 아닙니다.** 기록해 둡니다.

index register가 함께 쓰이는 형태(`[esp+ebx*4]`)는 그 index의 상위 절반도 0이어야
하며, 이는 Task 558이 명시한 "guest GPR *n*이 host GPR *n*" 전제가 이미 보장합니다.

### 무엇을 통과시키지 않는가

- **opcode에 register가 박힌 형태** (`push esp` = `54`, `inc esp` = `44`). 앞의 것은
  Task 559의 stack lowering이, 뒤의 것은 Task 557이 이미 다룹니다. 이 단위는 ModRM과
  SIB만 건드립니다.
- **`ESP`가 암묵 operand인 명령** — `push`/`pop`/`call`/`ret` 등. 이미 각자의 slot이
  있거나 거부되고 있습니다.

fail-closed는 유지합니다. 위 세 자리로 설명되지 않는 `ESP` 사용은 계속 거부합니다.

### 검증 — 실행으로

`R15D`를 guest `ESP`로 놓고 세 자리를 각각 실행합니다.

1. `mov eax, [esp+8]` — guest stack의 값을 읽어 오는가
2. `add esp, 16` — `R15D`가 16 늘고 **host `RSP`는 그대로인가**
3. `mov eax, esp` — guest `ESP` 값이 나오는가

두 번째가 이 단위의 핵심입니다. 값만 보면 재인코딩하지 않은 것과 구분되지 않을 수
있으므로 **host `RSP`가 살아 있는지**를 함께 봅니다 — 재인코딩이 없었다면 그 시점에
host stack이 파괴되어 돌아올 수 없습니다.

그리고 census로 도달 가능 block이 실제로 늘었는지 잽니다. 이 단위의 목적이 그것이므로,
방출 가능 비율이 아니라 **도달 가능 block**이 성패를 말합니다.

### 비범위

- opcode-embedded `ESP`
- 32비트 wraparound 보존
- x87의 다른 문제들 — 이 단위는 `[esp]` 주소 지정만 다룹니다

## English

### Objective

Re-encode instructions that name `ESP` so they name `R15D`. This is exactly what holds
Task 563's measurement at **one reachable block**: the first instruction of the block
after the entry is refused for `stack-pointer`.

### Why it was refused

Task 546's decision 3 keeps host `RSP` as the SysV stack and puts guest `ESP` in `R15D`,
so in long mode bytes naming `ESP` reach the **host's** stack rather than the guest's.
Task 555 made that a refusal and was right to; what was missing was the re-encoding.

### Three places, one transform

Zydis's `operand.encoding` says which place each is.

| Place | Example | Change |
|---|---|---|
| ModRM `rm` (`mod=11`) | `add esp, 16` | `rm` to `111`, `REX.B` |
| ModRM `reg` | `mov eax, esp` | `reg` to `111`, `REX.R` |
| SIB `base` | `mov eax, [esp+8]` | SIB `base` to `111`, `REX.B` |

All three are one transform -- set the field to `111` and the matching `REX` bit -- and
the bits are set independently, since one instruction can have two (`mov esp, esp`).

**`ESP` cannot be a SIB index**: `index=100` is the encoding that means "no index", so
that place does not exist.

### Where `REX` goes

After the legacy prefixes and before the opcode. Guest code is 32-bit so it can never
already carry one, which makes this always an **insertion** that lengthens the instruction
by a byte. Zydis gives the opcode's offset, so everything before it is copied, the `REX`
is written, and the rest follows unchanged; displacement and immediate are untouched.

### Addresses are computed at 64 bits

After the base becomes `R15`, **no `0x67` is added**. `R15`'s upper half is zero and guest
memory is below 4 GiB (Task 551), so the 64-bit result is the guest address. Task 559's
push sequence already uses `mov dword ptr [r15], imm32` under the same convention.

**What this gives up is 32-bit wraparound.** If `ESP + disp` wrapped past 32 bits the
64-bit computation would not. The arena ends below `0x085E7000` so it cannot happen in
this configuration -- **accidental safety rather than a rule**, and recorded as such.

A form with an index (`[esp+ebx*4]`) also needs that index's upper half to be zero, which
is guaranteed by the "guest GPR *n* in host GPR *n*" premise Task 558 made explicit.

### What is not admitted

Registers embedded in the opcode (`push esp` = `54`, `inc esp` = `44`) -- the first is
Task 559's stack lowering, the second Task 557's. This unit touches ModRM and SIB only.
Instructions where `ESP` is an implicit operand already have their own slots or are
refused. Anything naming `ESP` that these three places do not explain stays refused.

### Verification -- by execution

With `R15D` holding guest `ESP`, run one of each place: `mov eax, [esp+8]` reading the
guest stack, `add esp, 16` moving `R15D`, and `mov eax, esp` producing the guest value.

The second is the one that matters. Values alone might not separate a re-encoded
instruction from one that was not, so **the host's `RSP` is checked for still being
alive** -- without the re-encoding it would have been destroyed at that instruction and
there would be no way back.

Then measure with the census whether **reachable blocks** actually grew. That is what this
unit is for, so that number decides it rather than the emittable fraction.

### Out of scope

Opcode-embedded `ESP`; preserving 32-bit wraparound; the rest of x87's problems -- this
unit is only about addressing through `[esp]`.
