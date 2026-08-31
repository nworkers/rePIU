# 20260901-557 `INC`/`DEC r32`를 ModRM 형태로 작업 로그

설계: [20260901-557](../design/20260901-557-inc-dec-modrm-lowering.md) ·
작업 지시: [20260901-557](../work-orders/20260901-557-inc-dec-modrm-lowering.md)

## 한국어

### 결과

`INC`/`DEC r32` **784건**이 `FF /0`·`FF /1`로 낮춰집니다.

| 항목 | Task 556 | 지금 | 차 |
|---|---:|---:|---:|
| lowering | 11,458 | 12,242 | **+784** |
| 방출 가능 | 51.15% | **52.46%** | +1.31%p |
| 거절 | 29,263 | 28,479 | −784 |
| `silently-different` | 1,466 | **682** | **−784** |
| 완결 block | 260 (1.82%) | **316 (2.21%)** | +56 |

가장 위험한 부류인 `silently-different`가 **절반 넘게** 줄었습니다. 남은 682건은 moffs
`A0`–`A3`(`mov` 681건)와 `BOUND`·`ARPL`·`LES`·`LDS`입니다.

### 805이 아니라 784인 이유 — 재서 확인했습니다

Task 556의 표는 거절된 `inc` 600 + `dec` 205 = 805건이라고 적었습니다. 낮춰진 것은
784건이고, 21건이 남았습니다.

설계는 "805보다 적으면 prefix 형태가 남아 있다는 뜻"이라고 적었는데, **그 예상이
틀렸습니다.** mnemonic별 표에 사유를 붙여 다시 재니 이렇습니다.

```text
inc  stack-pointer   20
```

남은 것은 prefix 형태가 아니라 **`FF /0`에 `[esp+N]` 메모리 피연산자가 붙은 형태**입니다.
`dec`은 한 건도 남지 않았습니다.

> **이 guest에는 prefix가 붙은 `INC`/`DEC`가 하나도 없습니다.** 평이한 1바이트 형태로
> 범위를 좁힌 결정이 아무것도 잃지 않았다는 뜻이고, 그것을 추정이 아니라 표가 말합니다.

### 그리고 이 표가 Task 556의 서술 하나를 뒤집었습니다

Task 556 로그와 frontier에 `operand-width`를 **"stack 명령 + x87"**이라고 적었습니다.
**틀렸습니다.** 사유를 붙여 보니 `operand-width` 8,217건은 사실상 전부 `push`·`pop`입니다.

```text
push  operand-width   4923
pop   operand-width   3291      합계 8,214 / 8,217
```

x87은 `operand-width`에 **없습니다.** x87 거절은 전부 `stack-pointer`입니다.

```text
fstp stack-pointer 338   fild stack-pointer 174   fld  stack-pointer 120
fmul stack-pointer  75   fistp stack-pointer 49   fadd stack-pointer  25
fsub stack-pointer  20   fst  stack-pointer  12   fldcw stack-pointer 11
```

이것이 뜻하는 바가 큽니다. **x87 명령의 인코딩은 long mode에서 이미 같습니다.** plan 안의
x87 2,758건 중 거절되는 것은 약 850건뿐이고, 그 이유는 x87이라서가 아니라 **`[esp]`를
가리켜서**입니다. 나머지 약 1,900건은 **이미 방출되고 있습니다.**

> x87은 제가 Task 556에서 말한 것처럼 "별개의 큰 덩어리"가 아닙니다. 대부분 이미 되고,
> 나머지는 **stack lowering이 함께 엽니다.**

관련 문서를 이 작업에서 함께 고쳤습니다.

### 손으로 옮긴 표를 기계가 검사합니다

이 단위의 위험은 인코딩 표를 손으로 옮겨 적는 것이었습니다. 그래서 probe는 다른 손으로 쓴
표와 비교하지 않고, **낮춘 바이트를 long mode 디코더로 다시 읽어** mnemonic과 register가
원본과 같은지 봅니다.

```text
long_mode_inc_dec_lowering=true,lowered=14/14,prefixed_refused=true
```

14는 16개 opcode에서 `inc esp`(`44`)와 `dec esp`(`4C`)를 뺀 수입니다. 그 둘은
`kStackPointerRegister`로 거절되는지 따로 확인합니다 — 낮추면 `FF C4`가 되어 host RSP를
쓰기 때문이고, Task 555가 닫은 구멍이 여기로 다시 들어오지 않게 하는 항목입니다.

이 검사 때문에 `repiu_core_probe`가 `repiu_zydis`를 링크하게 됐습니다. `repiu_exe`가
Zydis를 `PRIVATE`으로 링크해 헤더가 전달되지 않기 때문입니다.

### probe가 스스로 잡은 것

`0x40`을 낮추기 시작하자 **방출 probe가 실패했습니다.** 그 probe의 "거절되는 명령" 사례가
`0x40`이었기 때문입니다. 사례를 moffs `A1`로 옮기고 `inc eax`를 lowering 사례로 새로
넣었습니다.

의도한 실패입니다. 판정이 바뀌면 그 판정에 기대던 probe가 깨지는 것이 맞고, 조용히 통과하는
것이 문제입니다.

### 측정

| Host | 결과 |
|---|---|
| Win32 x86 Debug | `core_probe_all=true`, 19/19 |
| Linux x64 Release | `core_probe_all=true`, 19/19, skipped 2 |
| Linux i386 Release | `core_probe_all=true`, 19/19 |

census는 Windows와 Linux x64가 같고 `agrees=true`입니다.

### 남은 것

연쇄는 아직 열리지 않았습니다 — 완결 block 2.21%. 다음은 **stack lowering + control flow**
묶음이고, 위 x87 발견 때문에 그 묶음이 여는 범위가 Task 556에서 적은 것보다 넓습니다.

## English

### Result

**784** `INC`/`DEC r32` instructions are now lowered to `FF /0` and `FF /1`.

| Item | Task 556 | Now | Change |
|---|---:|---:|---:|
| Lowered | 11,458 | 12,242 | **+784** |
| Emittable | 51.15% | **52.46%** | +1.31pp |
| Refused | 29,263 | 28,479 | −784 |
| `silently-different` | 1,466 | **682** | **−784** |
| Complete blocks | 260 (1.82%) | **316 (2.21%)** | +56 |

The most dangerous class, `silently-different`, dropped by **more than half**. The
remaining 682 are the moffs forms `A0`-`A3` (681 of them `mov`) plus `BOUND`, `ARPL`, `LES`
and `LDS`.

### Why 784 and not 805 -- measured, not guessed

Task 556's table recorded 600 refused `inc` plus 205 `dec`, which is 805. 784 were lowered,
leaving 21.

The design predicted that "less than 805 means prefixed forms remain". **That prediction
was wrong.** Adding the reason to the per-mnemonic table and measuring again:

```text
inc  stack-pointer   20
```

What remains is not a prefixed form but **`FF /0` with an `[esp+N]` memory operand.** Not
one `dec` is left.

> **This guest contains no prefixed `INC`/`DEC` at all.** Narrowing the unit to the plain
> one-byte form cost nothing, and the table says so rather than an inference.

### And that table overturned one of Task 556's statements

Task 556's log and the frontier described `operand-width` as **"stack instructions + x87"**.
**That was wrong.** With reasons attached, `operand-width`'s 8,217 are essentially all
`push` and `pop`.

```text
push  operand-width   4923
pop   operand-width   3291      total 8,214 of 8,217
```

x87 is **not in `operand-width` at all.** Every x87 refusal is `stack-pointer`.

```text
fstp stack-pointer 338   fild stack-pointer 174   fld  stack-pointer 120
fmul stack-pointer  75   fistp stack-pointer 49   fadd stack-pointer  25
fsub stack-pointer  20   fst  stack-pointer  12   fldcw stack-pointer 11
```

The consequence is large. **x87 encodings are already identical in long mode.** Of the
2,758 x87 instructions in the plan only about 850 are refused, and not for being x87 --
for **pointing through `[esp]`**. The other ~1,900 **are already being emitted.**

> x87 is not "its own mass" as I wrote in Task 556. Most of it already works, and the rest
> **comes with the stack lowering.**

The affected documents were corrected in this task.

### A hand-copied table, checked by machine

The risk in this unit was transcribing an encoding table by hand. So the probe does not
compare against another hand-written table: it **re-reads the lowered bytes with a
long-mode decoder** and checks that the mnemonic and register match the original.

```text
long_mode_inc_dec_lowering=true,lowered=14/14,prefixed_refused=true
```

14 is the 16 opcodes minus `inc esp` (`44`) and `dec esp` (`4C`), which are separately
checked to be refused as `kStackPointerRegister` -- lowering them would give `FF C4`,
writing host RSP, and this item is what keeps Task 555's hole from coming back in through
this door.

That check is why `repiu_core_probe` now links `repiu_zydis`: `repiu_exe` links Zydis
`PRIVATE`, so the headers do not carry through.

### What the probes caught themselves

Lowering `0x40` made **the emission probe fail**, because `0x40` was that probe's example
of a refused instruction. The example moved to the moffs form `A1`, and `inc eax` was added
as a lowering example.

That failure was the right one. When a judgement changes, the probe that depended on it
should break; passing quietly is the problem.

### What was measured

| Host | Result |
|---|---|
| Win32 x86 Debug | `core_probe_all=true`, 19 of 19 |
| Linux x64 Release | `core_probe_all=true`, 19 of 19, 2 skipped |
| Linux i386 Release | `core_probe_all=true`, 19 of 19 |

The census agrees between Windows and Linux x64, with `agrees=true`.

### What is left

No chain yet -- complete blocks are 2.21%. Next is the **stack lowering plus control
flow** pair, and the x87 finding above means that pair opens more than Task 556 said.
