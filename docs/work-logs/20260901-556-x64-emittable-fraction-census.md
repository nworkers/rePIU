# 20260901-556 x64가 낼 수 있는 명령의 비율 작업 로그

설계: [20260901-556](../design/20260901-556-x64-emittable-fraction-census.md) ·
작업 지시: [20260901-556](../work-orders/20260901-556-x64-emittable-fraction-census.md)

## 한국어

### 결과 — 명령의 51%, block의 1.8%

`pumpit1`의 `PIU.EXE`, 14,307 block · 59,908 명령입니다.

| 항목 | 수 | 비율 |
|---|---:|---:|
| 복사 (`kIdenticalBytes`) | 19,187 | 32.03% |
| lowering | 11,458 | 19.13% |
| **방출 가능 합계** | **30,645** | **51.15%** |
| 거절 | 29,263 | 48.85% |
| **끝까지 방출되는 block** | **260 / 14,307** | **1.82%** |

emitter가 직접 센 카운터와 census의 분해가 **정확히 일치합니다**(`agrees=true`).
Windows와 Linux x64에서 같은 숫자입니다.

### 이 두 숫자가 함께 있어야 하는 이유

**명령 비율만 보면 절반을 왔다고 읽게 됩니다. 그게 이 측정의 함정입니다.**

block은 control flow로 끝나고, 그것이 안 나오면 다음 block으로 갈 길이 없습니다. 완결된
block이 1.82%라는 것은, 완결된 block에서 출발해도 **다음 block이 완결일 확률이 1.82%**라는
뜻입니다. 기대 연쇄 길이가 **약 1 block**입니다.

> 명령의 51%는 실행의 51%가 아닙니다. 실행은 사실상 0입니다.

### 무엇 때문에 못 내는가

| 사유 | 건수 | 거절 중 비중 |
|---|---:|---:|
| `not-a-copy-record` (control flow·guarded slot·port I/O) | 12,856 | 43.9% |
| `operand-width` (사실상 전부 `push`·`pop` — 아래 정정) | 8,217 | 28.1% |
| `stack-pointer` (Task 555) | 6,401 | 21.9% |
| `silently-different` (`INC`/`DEC r32`, moffs 등) | 1,466 | 5.0% |
| `rip-relative/lowering-declined` | 265 | 0.9% |
| `invalid-in-long-mode` | 58 | 0.2% |

거절된 mnemonic 상위(분류기 거절만 — `not-a-copy-record`는 이 표에 없습니다):

```text
push 5212   mov 4551   pop 3291   add 665   inc 600
sub 455     fstp 338   cmp 228    dec 205   lea 182
```

### 이 표가 정하는 순서

**두 덩어리가 거의 같은 크기이고, 둘 다 있어야 연쇄가 생깁니다.**

* **stack 계열** = `operand-width` + `stack-pointer` = **14,618건, 거절의 50%.**
  `push`/`pop`만 8,503건입니다.
* **control flow** = `not-a-copy-record` = **12,856건, 거절의 44%.**

어느 하나만 해도 block은 완결되지 않습니다. `push`를 전부 낮춰도 block 끝의 `ret`이
INT3이고, control flow만 세워도 그 앞의 `push`가 INT3입니다. **그래서 두 개가 한 묶음이고,
그것이 다음 두 단위입니다.**

싼 것 하나가 따로 있습니다. **`INC`/`DEC r32`(805건)** 는 `40+r`을 `FF /0`·`FF /1`로 다시
쓰면 끝나는 2바이트 재인코딩입니다. 연쇄를 열지는 못하지만 노력 대비 건수가 가장 좋고
`silently-different`(가장 위험한 부류)를 절반 넘게 줄입니다.

x87은 별개의 큰 덩어리입니다 — plan 안에 2,758건이고, 40개가 넘는 distinct mnemonic입니다.
stack과 control flow 뒤로 두는 것이 맞습니다. **그 앞이 안 되면 x87을 시험할 방법 자체가
없습니다.**

> **정정 (Task 557).** 위에서 `operand-width`를 "stack 명령 + x87"이라고 적은 것은
> **틀렸습니다.** 사유별로 다시 재니 `operand-width` 8,217건은 `push` 4,923 + `pop`
> 3,291 = 8,214로 사실상 전부 stack 명령이고, **x87은 거기 없습니다.** x87 거절은 전부
> `stack-pointer`이며(`[esp]`를 가리켜서), plan 안 x87 2,758건 중 약 1,900건은 **이미
> 방출되고 있습니다.** 따라서 x87은 별개의 덩어리가 아니라 **대부분 stack lowering이 함께
> 여는 것**입니다. 근거는
> [20260901-557 로그](20260901-557-inc-dec-modrm-lowering.md)에 있습니다.

### 도구가 스스로를 검사합니다

census는 방출 규칙을 다시 구현하지 않습니다. plan으로 **실제 image를 빌드해서**
`long_mode_copied_count`·`long_mode_lowered_count`·`long_mode_refused_count`를 읽고, 그것을
자신의 분해와 대조해 `agrees=`로 출력합니다. emitter 규칙이 바뀌면 census가 조용히
거짓말하는 대신 어긋난 것이 보입니다.

emitter의 guest-address dedup까지 따라했기 때문에 `considered=59908`이 plan의 명령 수와
같고 카운터와도 같습니다.

## English

### Result -- 51% of instructions, 1.8% of blocks

`pumpit1`'s `PIU.EXE`: 14,307 blocks and 59,908 instructions.

| Item | Count | Share |
|---|---:|---:|
| Copied (`kIdenticalBytes`) | 19,187 | 32.03% |
| Lowered | 11,458 | 19.13% |
| **Emittable total** | **30,645** | **51.15%** |
| Refused | 29,263 | 48.85% |
| **Blocks that come out complete** | **260 / 14,307** | **1.82%** |

The emitter's own counters and the census breakdown **agree exactly**
(`agrees=true`), and Windows and Linux x64 produce the same numbers.

### Why both numbers have to appear together

**Read the instruction share alone and it looks half done. That is this measurement's
trap.**

A block ends in control flow, and without that there is no way to the next block. 1.82%
complete means that starting from a complete block, **the next block is complete with
probability 1.82%** -- an expected chain of **about one block.**

> 51% of instructions is not 51% of execution. Execution is effectively zero.

### What stops the rest

| Reason | Count | Share of refusals |
|---|---:|---:|
| `not-a-copy-record` (control flow, guarded slots, port I/O) | 12,856 | 43.9% |
| `operand-width` (essentially all `push` and `pop` -- see the correction below) | 8,217 | 28.1% |
| `stack-pointer` (Task 555) | 6,401 | 21.9% |
| `silently-different` (`INC`/`DEC r32`, moffs, ...) | 1,466 | 5.0% |
| `rip-relative/lowering-declined` | 265 | 0.9% |
| `invalid-in-long-mode` | 58 | 0.2% |

Top refused mnemonics (classifier refusals only -- `not-a-copy-record` is not in this
table):

```text
push 5212   mov 4551   pop 3291   add 665   inc 600
sub 455     fstp 338   cmp 228    dec 205   lea 182
```

### The order this table sets

**Two masses of nearly equal size, and a chain needs both.**

* **The stack family** = `operand-width` + `stack-pointer` = **14,618, half of all
  refusals.** `push` and `pop` alone are 8,503.
* **Control flow** = `not-a-copy-record` = **12,856, 44% of refusals.**

Either alone leaves blocks incomplete. Lower every `push` and the `ret` at the block's end
is still an INT3; build control flow alone and the `push` in front of it is still an INT3.
**So they are one pair, and they are the next two units.**

One cheap item stands apart. **`INC`/`DEC r32` (805)** is a two-byte re-encoding of `40+r`
into `FF /0` and `FF /1`. It opens no chain, but it has the best count-per-effort here and
removes more than half of `silently-different`, which is the most dangerous class.

x87 is its own mass -- 2,758 in the plan across more than 40 distinct mnemonics. It belongs
after stack and control flow: **until those work there is no way to exercise x87 at all.**

> **Correction (Task 557).** Describing `operand-width` above as "stack instructions + x87"
> was **wrong.** Measured again with reasons attached, its 8,217 are `push` 4,923 plus
> `pop` 3,291 = 8,214 -- essentially all stack instructions, and **x87 is not there.**
> Every x87 refusal is `stack-pointer`, for pointing through `[esp]`, and about 1,900 of
> the plan's 2,758 x87 instructions **are already being emitted.** So x87 is not a separate
> mass but **mostly something the stack lowering opens with it.** The evidence is in the
> [20260901-557 log](20260901-557-inc-dec-modrm-lowering.md).

### The tool checks itself

The census does not reimplement the emission rule. It **actually builds an image** from the
plan, reads `long_mode_copied_count`, `long_mode_lowered_count` and
`long_mode_refused_count`, and prints `agrees=` after comparing them with its own
breakdown. If the emitter's rule changes, the mismatch shows instead of the census quietly
lying.

It mirrors the emitter's guest-address dedup too, which is why `considered=59908` equals
both the plan's instruction count and the emitter's totals.
