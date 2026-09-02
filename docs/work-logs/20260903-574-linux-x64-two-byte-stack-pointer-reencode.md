# 작업 기록 20260903-574 — Linux x64 두 바이트 opcode의 `ESP` 재인코딩

설계: [20260903-574](../design/20260903-574-linux-x64-two-byte-stack-pointer-reencode.md) ·
작업 지시: [20260903-574](../work-orders/20260903-574-linux-x64-two-byte-stack-pointer-reencode.md)

## 측정 — mnemonic을 붙이자 한 행이 두 작업으로 갈라졌습니다

정지 표의 `kCopy` 행은 이유만 달고 있었습니다. 이유는 분류기가 왜 거절했는지를
말할 뿐 **무엇을 쓸지는 말하지 않습니다.** Task 573이 `kIndirectExit`를 형식으로
나눈 논리를 한 단계 아래에 적용해 mnemonic을 붙였고, 그러자 두 가지가 보였습니다.

| 정지 | 건수 |
|---|---:|
| `invalid-in-long-mode` `push` | 15 |
| `stack-pointer` `push` | 12 |
| `kJumpTable` | 12 |
| `stack-pointer/lowering-declined` `imul` | 10 |
| `stack-pointer/lowering-declined` `movzx` | 7 |

첫째, `push`라는 이름 아래 **완전히 다른 두 작업**이 있었습니다. 앞의 15건은 한
바이트 세그먼트 push(`06`/`0E`/`16`/`1E`)로 shadow selector가 필요한 slot이고,
뒤의 12건은 `FF /6`로 implicit stack operand 때문에 stack sequence가 필요합니다.
`kCopy stack-pointer 12` 하나만 보고 있었을 때는 이 둘이 구분되지 않았습니다.

둘째, `lowering-declined` 17건이 `imul`과 `movzx` 둘뿐이라는 것입니다. 그 사실이
근인을 바로 가리켰습니다.

## 근인 — 주석의 주장이 사실이 아니었습니다

`LowerLongModeBytes`의 `kStackPointerToR15` 분기는 이렇게 적혀 있었습니다.

```cpp
// ... 두 바이트 opcode map은 여기 오지 않는다. `ESP`가 ModRM이나 SIB에 있는
// 것은 한 바이트 opcode의 형태이고, 그 밖은 위에서 이미 거절됐기 때문이다.
if (instruction.opcode_map != ZYDIS_OPCODE_MAP_DEFAULT)
{
    return false;
}
```

`movzx esi, byte ptr [esp+8]`은 `0F B6`이고 `ESP`는 SIB base입니다 — 두 바이트
opcode이면서 정확히 이 unit이 허용한다고 말하는 형태입니다. **주장이 틀렸습니다.**

`movzx`의 인코딩은 `0F B6`과 `0F B7`뿐이므로 census가 센 18건은 다른 경로로
거절될 수 없습니다. 즉 이 조건이 근인이라는 것은 추정이 아니라 소거로 확정됩니다.

## 구현

opcode 위치를 `modrm.offset - 1`에서 `raw.prefix_count`로 바꿨습니다. 앞의 식은
**한 바이트 opcode에서만** 참이고, 두 바이트를 허용하는 순간 `0F`와 실제 opcode
사이에 REX를 끼우게 됩니다 — 그것은 raise 없이 실행되는 **다른 명령**입니다.

그리고 map별 레이아웃을 단언으로 남겼습니다: `DEFAULT`는
`modrm.offset == prefix_count + 1`, `0F`는 `+ 2`, 그 밖은 거절. `0F38`/`0F3A`는
분류기가 이미 막지만, 레이아웃을 쓰는 함수가 그 레이아웃을 확인해야 합니다.

## 검증에서 제가 틀렸던 것

byte 검사의 기대값을 처음에 `41 0F B6 74 24 08`로 적었습니다. 실제 출력은
`41 0F B6 74 27 08`이었고, **틀린 쪽은 코드가 아니라 제 기대값이었습니다.**

SIB base를 `100`(ESP) 그대로 두고 REX.B만 붙이면 그것은 R15가 아닙니다. R15를
가리키려면 REX.B가 상위 비트를, SIB base 필드 `111`이 하위 세 비트를 대야
합니다. 즉 `24` → `27`은 lowering이 옳게 하고 있던 일입니다.

기대값을 고쳤고, 주석에 그 이유를 남겼습니다 — SIB가 그대로 살아남기를 기대하는
검사는 **host RSP로 주소를 계산하는 lowering을 통과시키는 검사**이기 때문입니다.

실행 probe가 먼저 통과하고 byte 검사만 실패한 것이 이것을 빨리 잡아 주었습니다.
두 검사가 서로를 견제한 셈입니다.

## 검증

### Linux x64 Release — 확인됨

`repiu_core_probe`: 20/20, failures 0, skipped 2.

```text
long_mode_lowering_two_byte_esp=true,two_byte=410fb6742708,one_byte=418b442708
  two_byte_esp_product    observed=0x2a        expected=0x2a
  two_byte_esp_preserved  observed=0x20001800  expected=0x20001800
  two_byte_esp_lowered    observed=0x2         expected=0x2
guest_two_byte_esp=true lowered=2 copied=0
```

실행 검사는 `[esp+8]`에 `7`, `[esp+4]`에 `6`을 두고 `movzx` 뒤 `imul`을 돌려
**곱** `0x2a`를 봅니다. 값 하나를 복사하는 검사였다면 바이트는 맞게 읽고 곱하는
쪽을 틀린 실행도 통과했을 것입니다.

재인코딩이 없었다면 두 명령은 host RSP로 주소를 계산합니다 — 그곳은 매핑되어
있고 읽을 수 있으며 **완전히 틀린 자리**입니다. 그래서 실패가 fault가 아니라
값이고, guest stack에 알려진 값을 심는 것만이 그것을 잡습니다.

`one_byte=418b442708`은 opcode 위치를 바꾼 것이 기존 한 바이트 형식을 움직이지
않았다는 확인입니다.

### census — 확인됨

| 항목 | Task 573 | Task 574 |
|---|---:|---:|
| lowered | 31,746 | **31,805** (+59) |
| emittable | 73,689 (99.13%) | **73,748 (99.21%)** |
| refused | 644 | **585** |
| complete block | 15,614 (89.91%) | **15,646 (90.09%)** |
| 도달 가능 block | 7,462 (42.97%) | **7,723 (44.47%)** |
| reachable instrs | 32,055 | **34,188** |
| first stop | `0x1101370` | `0x1101370` (불변) |

`agrees=true`. `+59`는 census가 앞서 센 `imul` 41 + `movzx` 18과 **정확히**
같습니다. 정지 표에서 `stack-pointer/lowering-declined` 17건이 전부
사라졌습니다.

### 회귀 — 확인됨

- **Linux i386 Release `repiu_core_probe`**: 19/19, failures 0, skipped 3.
  `aot_long_mode_compatibility.cpp`는 공용 경로이므로 필요한 확인이었습니다.
- **Win32 x86 Debug `repiu_aot_probe`**: `_all=true` 41개, `_all=false` 0개.

## 남은 것

정지 표는 이제 이렇습니다.

| 정지 | 건수 |
|---|---:|
| `invalid-in-long-mode` `push` (세그먼트 push) | 15 |
| `stack-pointer` `push` (`FF /6`) | 12 |
| `kJumpTable` | 12 |
| edge outside the plan | 24 |

`push` 두 부류가 여전히 가장 큽니다. `FF /6`은 image 전체로 261건이라 남은 거절
585건의 **44.6%**이고, Task 573의 합성-lowering 기법을 그대로 쓸 수 있습니다.
세그먼트 push는 shadow selector가 필요하므로 patch site가 있는 slot이어야 하고,
planner가 그것을 `kCopy`로 분류한다는 점 때문에 앞의 세 세그먼트 단위보다
구조가 큽니다.

---

# Work log 20260903-574 — Long-mode `ESP` re-encoding for two-byte opcodes

Design: [20260903-574](../design/20260903-574-linux-x64-two-byte-stack-pointer-reencode.md) ·
work order: [20260903-574](../work-orders/20260903-574-linux-x64-two-byte-stack-pointer-reencode.md)

## Measurement — attaching mnemonics split one row into two units of work

The stop table's `kCopy` rows carried only a reason, and a reason says why the
classifier refused — it **does not say what to write**. Applying Task 573's
argument for splitting `kIndirectExit` by form one level down showed two things.

| Stop | Count |
|---|---:|
| `invalid-in-long-mode` `push` | 15 |
| `stack-pointer` `push` | 12 |
| `kJumpTable` | 12 |
| `stack-pointer/lowering-declined` `imul` | 10 |
| `stack-pointer/lowering-declined` `movzx` | 7 |

First, there were **two entirely different jobs** under the name `push`. The 15
are one-byte segment pushes (`06`/`0E`/`16`/`1E`), needing a slot with the
shadow selector; the 12 are `FF /6`, needing a stack sequence because of the
implicit stack operand. Looking at `kCopy stack-pointer 12` alone did not
distinguish them.

Second, the 17 `lowering-declined` were only `imul` and `movzx` — and that fact
pointed straight at the root cause.

## Root cause — the comment's claim was not true

`LowerLongModeBytes`'s `kStackPointerToR15` branch read:

```cpp
// ... a two-byte opcode map never reaches here, because `ESP` in ModRM or SIB
// is a one-byte-opcode shape and anything else was refused above.
if (instruction.opcode_map != ZYDIS_OPCODE_MAP_DEFAULT)
{
    return false;
}
```

`movzx esi, byte ptr [esp+8]` is `0F B6` with `ESP` as the SIB base — a two-byte
opcode in exactly the shape this unit says it admits. **The claim was false.**

`movzx` has no encoding other than `0F B6` and `0F B7`, so the 18 the census
counted cannot have been refused anywhere else. That this condition is the root
cause is settled by elimination rather than inferred.

## Implementation

The opcode's position moved from `modrm.offset - 1` to `raw.prefix_count`. The
former is true **only of a one-byte opcode**, and admitting two-byte ones would
insert the REX between `0F` and the opcode proper — a **different instruction**
that runs without raising.

The per-map layout is asserted: `DEFAULT` requires
`modrm.offset == prefix_count + 1`, `0F` requires `+ 2`, anything else is
refused. `0F38` and `0F3A` are already stopped by the classifier, but a function
that writes a layout checks the layout it writes.

## Where I was wrong in verification

The byte check's expected value was first written `41 0F B6 74 24 08`. The
actual output was `41 0F B6 74 27 08`, and **what was wrong was the expectation,
not the code.**

Leaving the SIB base at `100` (ESP) and adding only REX.B does not name R15.
Naming R15 takes both: the REX bit supplies the high bit and SIB base `111` the
low three. So `24` → `27` was the lowering doing its job.

The expectation was corrected and the reason left in a comment — a check
expecting the SIB byte to survive unchanged is **a check that would pass a
lowering still addressing through host RSP**.

The execution probe passing while only the byte check failed is what caught this
quickly; the two checks held each other honest.

## Verification

### Linux x64 Release — confirmed

`repiu_core_probe`: 20/20, 0 failures, 2 skipped.

```text
long_mode_lowering_two_byte_esp=true,two_byte=410fb6742708,one_byte=418b442708
  two_byte_esp_product    observed=0x2a        expected=0x2a
  two_byte_esp_preserved  observed=0x20001800  expected=0x20001800
  two_byte_esp_lowered    observed=0x2         expected=0x2
guest_two_byte_esp=true lowered=2 copied=0
```

The execution check puts `7` at `[esp+8]` and `6` at `[esp+4]`, runs the `movzx`
then the `imul`, and looks at the **product**, `0x2a`. A check that copied one
value would also have passed a run that read the right byte and multiplied by
the wrong thing.

Without the re-encoding both instructions address through host RSP — a mapped,
readable, **entirely wrong** place. So the failure is a value rather than a
fault, and only planting known values in the guest stack catches it.

`one_byte=418b442708` confirms that moving how the opcode is located did not
move the existing one-byte form.

### Census — confirmed

| Item | Task 573 | Task 574 |
|---|---:|---:|
| Lowered | 31,746 | **31,805** (+59) |
| Emittable | 73,689 (99.13%) | **73,748 (99.21%)** |
| Refused | 644 | **585** |
| Complete blocks | 15,614 (89.91%) | **15,646 (90.09%)** |
| Reachable blocks | 7,462 (42.97%) | **7,723 (44.47%)** |
| Reachable instructions | 32,055 | **34,188** |
| First stop | `0x1101370` | `0x1101370` (unchanged) |

`agrees=true`. The `+59` is **exactly** the `imul` 41 plus `movzx` 18 the census
had already counted, and all 17 `stack-pointer/lowering-declined` stops are
gone.

### Regression — confirmed

- **Linux i386 Release `repiu_core_probe`**: 19/19, 0 failures, 3 skipped.
  `aot_long_mode_compatibility.cpp` is a shared path, so this was necessary.
- **Win32 x86 Debug `repiu_aot_probe`**: 41 `_all=true`, 0 `_all=false`.

## What is left

The stop table now reads:

| Stop | Count |
|---|---:|
| `invalid-in-long-mode` `push` (segment pushes) | 15 |
| `stack-pointer` `push` (`FF /6`) | 12 |
| `kJumpTable` | 12 |
| Edge outside the plan | 24 |

The two `push` families are still the largest. `FF /6` is 261 image-wide, which
is **44.6%** of the 585 refusals that remain, and Task 573's synthesise-then-
lower technique applies to it directly. The segment pushes need the shadow
selector and therefore a slot with a patch site, and because the planner
classifies them as `kCopy` they are structurally larger than the three segment
units before them.
