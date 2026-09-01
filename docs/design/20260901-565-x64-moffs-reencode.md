# 20260901-565 x64 moffs 재인코딩 설계

## 한국어

### 목적

`MOV` 의 moffs 형태(`A0`–`A3`)를 SIB 절대형으로 재인코딩합니다. Task 564가 장애물을
21바이트 옮긴 뒤 드러난 것이 이것이고, image 전체 `silently-different` 682개 중
**681개가 이 형태**입니다.

### 왜 조용히 틀리는가

`A1 disp32`는 32비트에서 `mov eax, [disp32]`, 5바이트입니다. long mode에서는 offset이
**8바이트**가 되어 명령 길이가 9바이트로 바뀝니다. 예외는 나지 않고, **그 뒤의 바이트
해석까지 전부 어긋납니다.** Task 550이 이것을 "가장 위험한 부류"로 분류한 이유입니다.

### 변환 — Task 552가 이미 만든 목적지

절대 주소는 SIB로만 표현됩니다(`mod=00`, `rm=100`, SIB `base=101`, `index=100`). Task
552의 `kAbsoluteToSib`가 `8B 05 disp32`를 위해 이미 그 형태를 만듭니다. moffs는 **출발
인코딩만 다르고 도착지가 같습니다** — ModRM이 아예 없으므로 opcode를 바꿔 넣습니다.

| moffs | 32비트 의미 | 재인코딩 |
|---|---|---|
| `A0 disp32` | `mov al, [disp32]` | `67 8A 04 25 disp32` |
| `A1 disp32` | `mov eax, [disp32]` | `67 8B 04 25 disp32` |
| `A2 disp32` | `mov [disp32], al` | `67 88 04 25 disp32` |
| `A3 disp32` | `mov [disp32], eax` | `67 89 04 25 disp32` |

`0x67`이 필요한 이유도 552와 같습니다. 없으면 SIB 절대형의 `disp32`가 64비트로
**sign-extend**되어 bit 31이 선 주소가 `0xFFFFFFFF8…`이 됩니다.

`kAbsoluteToSib`를 재사용하지 않고 별도 lowering으로 두는 이유는 **출발 인코딩이 다르기
때문**입니다 — moffs에는 고칠 ModRM이 없고, 하나의 rewrite에 두 형태를 넣으면 어느
쪽을 다루는지 분기가 생깁니다.

### 통과시키지 않는 것

**prefix 붙은 형태.** `66 A1`은 `mov ax, moffs`로 폭이 다르고, `67 A1`은 이미 주소
크기를 바꿉니다. Task 557이 `inc`에서 한 것과 같은 방침입니다 — 증명하는 것을 가장 작게
두고, **그 제한이 무엇을 놓치는지는 census가 잽니다.**

### 검증

세 가지를 봅니다.

1. **실행** — 하위 4 GiB의 알려진 주소를 moffs로 읽고 쓰며, 재인코딩된 형태가 그
   주소에 닿는지 확인합니다. `A1`(읽기)과 `A3`(쓰기) 양방향을 봅니다.
2. **도달 가능 block** — 이 단위의 성패는 이것이 말합니다. Task 564가 커버리지만 오르고
   도달 가능이 그대로였던 것을 실패로 기록했으므로, 같은 기준을 씁니다.
3. **다음 정지 지점** — 움직였다면 어디로 갔는지. 움직이지 않았다면 왜인지.

### 비범위

- prefix 붙은 moffs
- `BOUND`, `ARPL`, `LES`, `LDS` — 682 중 나머지 1개이며 각각 다른 문제입니다

## English

### Objective

Re-encode `MOV`'s moffs forms (`A0`–`A3`) into the SIB absolute form. This is what Task
564 exposed after moving the obstruction twenty-one bytes, and **681 of the image's 682
`silently-different` records are this shape**.

### Why it is quietly wrong

`A1 disp32` is `mov eax, [disp32]` in five bytes. In long mode the offset is **eight**
bytes, so the instruction becomes nine and **the decode of everything after it moves too**.
Nothing raises. That is why Task 550 filed it under the dangerous class.

### The transform -- a destination Task 552 already built

An absolute address is expressible only through SIB (`mod=00`, `rm=100`, SIB `base=101`,
`index=100`), which is exactly what `kAbsoluteToSib` produces for `8B 05 disp32`. The
moffs forms **differ only in where they start**: there is no ModRM to fix, so the opcode is
replaced instead.

| moffs | 32-bit meaning | Re-encoded |
|---|---|---|
| `A0 disp32` | `mov al, [disp32]` | `67 8A 04 25 disp32` |
| `A1 disp32` | `mov eax, [disp32]` | `67 8B 04 25 disp32` |
| `A2 disp32` | `mov [disp32], al` | `67 88 04 25 disp32` |
| `A3 disp32` | `mov [disp32], eax` | `67 89 04 25 disp32` |

The `0x67` is needed for Task 552's reason: without it the SIB form's `disp32` is
**sign-extended**, so an address with bit 31 set becomes `0xFFFFFFFF8…`.

It is a separate lowering rather than a reuse of `kAbsoluteToSib` because **the starting
encoding differs** -- moffs has no ModRM to rewrite, and folding both into one rewrite
would put a branch inside it over which shape it was handed.

### What is not admitted

**Prefixed forms.** `66 A1` is `mov ax, moffs` at a different width, and `67 A1` already
changes the address size. This follows Task 557's policy for `inc`: keep what is proven to
the smallest thing, and **let the census measure what the restriction costs.**

### Verification

Three things: **execution**, reading and writing a known address below 4 GiB through both
`A1` and `A3`; **reachable blocks**, which decide this unit as they decided the last one;
and **where the stopping point moved**, or why it did not.

### Out of scope

Prefixed moffs; `BOUND`, `ARPL`, `LES` and `LDS`, which are the remaining one of the 682
and each a different problem.
