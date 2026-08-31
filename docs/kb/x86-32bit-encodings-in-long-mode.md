# 32비트 encoding을 long mode에서 실행할 때 / Running 32-bit encodings in long mode

## 한국어

### 왜 이 프로젝트에 필요한가

rePIU는 원본 32비트 코드를 그대로 실행하는 것이 목표이고, i386 host에서는 AOT
code cache가 guest instruction 바이트를 복사해 그대로 실행합니다. x86-64 host로
옮기면 이 항등 관계가 깨집니다. 어떤 바이트가 어떻게 달라지는지는 "대략 비슷하다"로
넘어갈 수 없는 종류의 사실입니다.

### 세 가지 다른 실패

| 종류 | 무슨 일이 일어나나 | 발견 난이도 |
|---|---|---|
| `#UD` | 그 opcode가 long mode에 없다 | 쉬움 — 예외가 난다 |
| 폭 변화 | 같은 연산이 64비트 폭으로 수행된다 | 중간 — 값이 어긋난다 |
| **의미 변화** | **바이트가 다른 명령으로 해석된다** | **어려움 — 조용하다** |

세 번째가 이 문서의 이유입니다.

### 의미가 바뀌는 encoding

#### `40`–`4F`: `INC`/`DEC r32` → REX prefix

32비트 모드에서 `40`은 `inc eax`입니다. long mode에서 `40`–`4F`는 REX prefix이고,
**뒤따르는 명령을 수정합니다**. 즉 명령 하나가 사라지고 다음 명령이 다른 것이 됩니다.
long mode에서 `inc eax`는 `FF C0`으로 인코딩됩니다.

#### `62`, `63`, `C4`, `C5`: prefix로 재배정된 opcode

| 바이트 | 32비트 | 64비트 |
|---|---|---|
| `62` | `BOUND` | EVEX prefix (AVX-512) |
| `63` | `ARPL` | `MOVSXD` |
| `C4` | `LES` | 3-byte VEX prefix |
| `C5` | `LDS` | 2-byte VEX prefix |

`C4`/`C5`는 32비트 모드에서도 ModRM `mod=11`일 때 VEX로 해석됩니다. 이 겹침이 VEX
설계가 32비트 모드와 공존하려고 택한 방식입니다.

#### `A0`–`A3`: `moffs32` → `moffs64`

`MOV AL, moffs`와 `MOV eAX, moffs` 계열은 절대 주소를 immediate로 싣습니다. long
mode에서 그 immediate는 **8바이트**입니다. 따라서 명령 길이가 5에서 9로 바뀌고,
바이트 스트림을 순차 해석하는 쪽은 이 명령 이후 전부 어긋납니다.

#### ModRM `mod=00`, `rm=101`: 절대 `disp32` → `RIP`-relative

32비트 모드에서 이 조합은 base register 없는 절대 주소입니다.

```
8B 05 78 56 34 12    32비트: mov eax, [0x12345678]
                     64비트: mov eax, [rip + 0x12345678]
```

long mode에서 절대 주소를 쓰려면 SIB로 `base=101`, `index=100`, `mod=00`을 만들어야
합니다. 전역 변수를 절대 주소로 읽는 코드에서는 이 형태가 압도적으로 흔하므로,
발산 한 줄이 코드 전체에 퍼집니다.

### 폭이 바뀌는 encoding

long mode에서 **stack 관련 명령의 기본 operand size는 64비트**이고, 32비트로 낮출
방법이 없습니다. `66` prefix는 16비트를 줍니다.

`PUSH`/`POP r32` (`50`–`5F`), `PUSH imm` (`68`, `6A`), `POP r/m` (`8F /0`),
`PUSHFD`/`POPFD` (`9C`, `9D`), `RET` (`C2`, `C3`), `LEAVE` (`C9`),
`CALL rel32` (`E8`), `CALL`/`JMP r/m` (`FF /2`, `/3`),
`PUSH`/`POP FS`,`GS` (`0F A0`/`A8`/`A1`/`A9`).

`JMP rel32` (`E9`)는 stack을 건드리지 않으므로 여기 해당하지 않습니다.

### long mode에 없는 encoding

`06`/`0E`/`16`/`1E` (`PUSH ES`/`CS`/`SS`/`DS`), `07`/`17`/`1F` (`POP ES`/`SS`/`DS`),
`27` (`DAA`), `2F` (`DAS`), `37` (`AAA`), `3F` (`AAS`), `60`/`61` (`PUSHAD`/`POPAD`),
`9A` (far `CALL`), `CE` (`INTO`), `D4` (`AAM`), `D5` (`AAD`), `D6` (`SALC`),
`EA` (far `JMP`).

### 주소 크기와 `67` prefix

long mode의 기본 address size는 **64비트**입니다. ModRM memory operand가 있는 32비트
바이트는 그대로 두면 64비트 register를 base/index로 씁니다. `67` prefix를 붙이면
32비트 주소 계산으로 돌아가고 결과는 64비트로 zero-extend 됩니다.

따라서 `67`은 **접근 대상이 하위 4 GiB에 있을 때만** 32비트 의미와 일치합니다. 이는
prefix 하나로 해결되는 문제가 아니라 주소 공간 배치 결정입니다.

### 정리

> 32비트 바이트를 long mode에서 실행하는 것은 부분집합 관계가 아니다. 겹치는 영역이
> 넓을 뿐이고, 겹치지 않는 곳 중 일부는 예외 없이 다른 일을 한다.

### 출처

* Intel® 64 and IA-32 Architectures Software Developer's Manual, Vol. 2 — opcode map,
  "Instructions Not Supported in 64-Bit Mode", ModRM/SIB `RIP`-relative 항목
* AMD64 Architecture Programmer's Manual, Vol. 3 — 64-bit mode에서의 명령 차이
* 이 프로젝트의 적용: [20260831-550 설계](../design/20260831-550-linux-x64-long-mode-byte-compatibility.md)

## English

### Why this project needs it

rePIU exists to run the original 32-bit code as it is, and on an i386 host the AOT code
cache copies guest instruction bytes and executes them unchanged. Moving to an x86-64
host breaks that identity. Which bytes change, and how, is not the kind of fact that
survives "roughly the same".

### Three different failures

| Kind | What happens | How hard to notice |
|---|---|---|
| `#UD` | the opcode does not exist in long mode | easy -- it raises |
| Width change | the same operation runs at 64-bit width | moderate -- values drift |
| **Meaning change** | **the bytes decode as a different instruction** | **hard -- it is silent** |

The third is why this page exists.

### Encodings whose meaning changes

#### `40`–`4F`: `INC`/`DEC r32` becomes a REX prefix

In 32-bit mode `40` is `inc eax`. In long mode `40`–`4F` are REX prefixes that **modify
the instruction that follows**: one instruction disappears and the next becomes something
else. Long mode encodes `inc eax` as `FF C0`.

#### `62`, `63`, `C4`, `C5`: opcodes reassigned to prefixes

| Byte | 32-bit | 64-bit |
|---|---|---|
| `62` | `BOUND` | EVEX prefix (AVX-512) |
| `63` | `ARPL` | `MOVSXD` |
| `C4` | `LES` | three-byte VEX prefix |
| `C5` | `LDS` | two-byte VEX prefix |

`C4`/`C5` also decode as VEX in 32-bit mode when ModRM has `mod=11`; that overlap is how
VEX was designed to coexist with 32-bit code.

#### `A0`–`A3`: `moffs32` becomes `moffs64`

The `MOV AL, moffs` and `MOV eAX, moffs` forms carry an absolute address as an immediate.
In long mode that immediate is **eight bytes**, so the instruction's length goes from five
to nine -- and anything decoding the byte stream in sequence is wrong from there on.

#### ModRM `mod=00`, `rm=101`: absolute `disp32` becomes `RIP`-relative

In 32-bit mode this combination is an absolute address with no base register.

```
8B 05 78 56 34 12    32-bit: mov eax, [0x12345678]
                     64-bit: mov eax, [rip + 0x12345678]
```

Long mode needs a SIB byte with `base=101`, `index=100`, `mod=00` to express an absolute
address. Code that reads globals by absolute address uses this form constantly, so a
single divergence spreads across the whole program.

### Encodings whose width changes

In long mode the **default operand size of the stack instructions is 64 bits**, and there
is no way to ask for 32; a `66` prefix gives 16.

`PUSH`/`POP r32` (`50`–`5F`), `PUSH imm` (`68`, `6A`), `POP r/m` (`8F /0`),
`PUSHFD`/`POPFD` (`9C`, `9D`), `RET` (`C2`, `C3`), `LEAVE` (`C9`), `CALL rel32` (`E8`),
`CALL`/`JMP r/m` (`FF /2`, `/3`), and `PUSH`/`POP FS`,`GS` (`0F A0`/`A8`/`A1`/`A9`).

`JMP rel32` (`E9`) is not among them, because it touches no stack.

### Encodings that do not exist in long mode

`06`/`0E`/`16`/`1E`, `07`/`17`/`1F`, `27` (`DAA`), `2F` (`DAS`), `37` (`AAA`),
`3F` (`AAS`), `60`/`61` (`PUSHAD`/`POPAD`), `9A` (far `CALL`), `CE` (`INTO`),
`D4` (`AAM`), `D5` (`AAD`), `D6` (`SALC`), `EA` (far `JMP`).

### Address size and the `67` prefix

Long mode's default address size is **64 bits**. Left alone, 32-bit bytes with a ModRM
memory operand use 64-bit registers as base and index. A `67` prefix restores 32-bit
address computation, zero-extended to 64 bits.

So `67` matches the 32-bit meaning **only while what is being accessed lives below
4 GiB**. That is an address-space placement decision, not something one prefix settles.

### In short

> Running 32-bit bytes in long mode is not a subset relationship. The overlap is large,
> and part of what falls outside it does something different without raising anything.

### Sources

* Intel® 64 and IA-32 Architectures Software Developer's Manual, Vol. 2 -- opcode map,
  "Instructions Not Supported in 64-Bit Mode", and the ModRM/SIB `RIP`-relative entries
* AMD64 Architecture Programmer's Manual, Vol. 3 -- instruction differences in 64-bit mode
* Applied in this project: [20260831-550 design](../design/20260831-550-linux-x64-long-mode-byte-compatibility.md)
