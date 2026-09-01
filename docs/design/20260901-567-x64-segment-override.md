# 20260901-567 x64 segment override 설계

## 한국어

### 목적

`kSegmentOverrideMem`을 long mode에서 방출합니다. Task 565가 남긴 정지 지점이
`26 8b 1d 5c 00 00 00` = `mov ebx, es:[0x5c]`이고, Task 566이 **guest의 segment base가
0이 아님**을 측정해 방법을 좁혔습니다.

### 왜 prefix를 뗄 수 없는가

long mode는 `CS`/`DS`/`ES`/`SS` override를 무시하고 base를 0으로 봅니다. base가 0인
guest라면 prefix 제거가 곧 항등이었을 것입니다. **아닙니다.**

```text
selector=0x1c base=0x1000000   selector=0x24 base=0x1010000
selector=0x2c base=0x1100000   selector=0x34 base=0x1110000
```

`es:[0x5c]`는 `[0x5c]`가 아니라 `[ES_base + 0x5c]`이고, prefix를 떼면 **예외 없이 다른
주소를 읽습니다.** Task 550이 분류한 "조용히 다른 명령이 되는" 부류입니다.

### i386이 쓰는 방법

`EmitSegmentOverrideSlot`이 이미 옳게 합니다.

```text
pushfd
cmp word [shadow_selector], S      ← 주소와 S는 나중에 patch
je do_access
  popfd
  int3                             ← selector가 바뀌었으면 boundary
do_access:
  popfd
  <access, base가 disp32에 접힘>    ← disp32도 나중에 patch
  jmp fallthrough
```

**guard가 요점입니다.** guest는 `ES`를 다시 실어 넣을 수 있으므로, 접어 넣은 base는
selector가 그대로일 때만 유효합니다. 어긋나면 boundary로 갑니다.

### x64에서 깨지는 것 셋

| 조각 | i386 | long mode |
|---|---|---|
| `9C` / `9D` | `pushfd` / `popfd` | **8바이트, host `RSP`** |
| `66 81 3D <abs32>` | 절대 주소 비교 | **`RIP`-relative** |
| access의 `mod=00 rm=101` | 절대 `disp32` | **`RIP`-relative** |

세 조각 모두 이미 답이 있습니다 — 첫째는 Task 559의 flags 시퀀스(guest `ESP`=`R15D`),
나머지 둘은 Task 552의 SIB 절대형입니다. **이 단위는 새 기법이 아니라 셋의 합성입니다.**

```text
<lowered pushfd>                       Task 559
67 66 81 3C 25 <abs32> <imm16>         cmp word [abs32], S — SIB 절대형
je do_access
  <lowered popfd>
  int3
do_access:
  <lowered popfd>
  67 <op> <modrm rm=100> 25 <disp32>   접힌 base + SIB 절대형
  E9 fallthrough
```

### 반드시 함께 와야 하는 것 — patch

disp32와 guard의 주소·selector는 **emit 시점에 0이고 나중에 patch됩니다**
(`AotSegmentOverrideSite`). patch하는 쪽이 없으면 slot은 **base 0으로 읽습니다** —
prefix를 뗀 것과 똑같이 틀리고, 그런데도 census는 "방출됨"으로 셉니다.

> 방출과 정확성은 다른 것이고, patch 없이 방출만 하면 **도달 가능 block이 늘면서
> 실행은 틀립니다.** 이번 세션이 반복해서 잡아 온 바로 그 형태입니다.

따라서 이 단위는 slot과 patch를 **함께** 가져가며, 검증은 probe가 site를 실제로 patch한
뒤 실행하는 것으로 합니다.

### 이번 단위가 통과시키는 것

정지 지점의 형태 하나 — **access가 `mod=00 rm=101`(절대 `disp32`)인 경우**입니다.
base/index를 쓰는 형태는 접힌 disp32에 더해 `0x67`만 붙이면 되지만, 그것까지 한 번에
열면 검증 대상이 둘이 됩니다. Task 557의 방침대로 가장 작은 것을 증명하고, **제한이
무엇을 놓치는지는 census가 잽니다** — Task 565에서 그 방침이 곧바로 답을 준 적이
있습니다.

`FS`/`GS` override는 계속 거부합니다. host TLS가 `FS`를 쓰고 있고 Task 546 결정 5가
raw guest segment를 host `FS`/`GS`에 설치하지 않는다고 못박았습니다.

### 검증

1. **실행** — 하위 4 GiB에 shadow selector와 데이터를 두고, site를 patch한 뒤 slot을
   실행합니다. selector가 맞으면 `[base + disp]`를 읽고, **틀리면 boundary로 가야
   합니다.** 두 방향을 다 봅니다 — 맞는 쪽만 보면 guard가 있는지 없는지 구분되지
   않습니다.
2. **도달 가능 block** — 이 단위의 성패는 이것이 말합니다.

### 비범위

- base/index를 쓰는 access 형태
- `FS`/`GS` override
- `kGuardedSegmentLoad`/`Read`/`Pop` — selector를 **쓰는** 쪽이고 다른 문제입니다

## English

### Objective

Emit `kSegmentOverrideMem` in long mode. Task 565's stopping point is
`mov ebx, es:[0x5c]`, and Task 566 narrowed the method by measuring that **the guest's
segment bases are not zero**.

### Why the prefix cannot simply be dropped

Long mode ignores the `CS`/`DS`/`ES`/`SS` overrides and treats their base as zero. Under a
zero-base guest that would have been the identity. It is not: the bases are the relocated
object bases, so `es:[0x5c]` means `[ES_base + 0x5c]` and dropping the prefix **reads a
different address without raising** -- Task 550's "quietly a different instruction" shape.

### What i386 does

`EmitSegmentOverrideSlot` already does it right: save flags, compare the shadow selector
against the one the fold assumed, fall to a boundary if it changed, otherwise perform the
access with the base folded into a `disp32`, then jump to the fallthrough.

**The guard is the point.** The guest can reload `ES`, so a folded base is only valid
while the selector is unchanged.

### Three pieces that break on x64

| Piece | i386 | Long mode |
|---|---|---|
| `9C` / `9D` | `pushfd` / `popfd` | **eight bytes, against host `RSP`** |
| `66 81 3D <abs32>` | absolute compare | **`RIP`-relative** |
| the access's `mod=00 rm=101` | absolute `disp32` | **`RIP`-relative** |

All three already have answers -- Task 559's flags sequence on `R15D` for the first, Task
552's SIB absolute form for the other two. **This unit is those three composed, not a new
technique.**

### What must come with it: the patching

The `disp32` and the guard's address and selector are **zero at emit time and patched
later** through `AotSegmentOverrideSite`. With nothing patching them the slot reads with a
base of zero -- wrong in exactly the way dropping the prefix is wrong, while the census
counts it as emitted.

> Emitting and being correct are different things. Emitting without patching would **raise
> reachable blocks while execution is wrong**, which is the shape this session has been
> catching all along.

So the slot and its patching go together, and verification patches the site before running
it.

### What this unit admits

One shape: the access whose ModRM is `mod=00 rm=101`, an absolute `disp32` -- which is what
the stopping point is. Base/index forms need only a `0x67` on top of the folded
displacement, but opening both at once makes two things to verify. Task 557's policy is to
prove the smallest thing and **let the census measure what the restriction costs**, which
paid off immediately in Task 565.

`FS`/`GS` overrides stay refused: host TLS uses `FS`, and Task 546's decision 5 says raw
guest segments are never installed into host `FS`/`GS`.

### Verification

**Execution**, with a shadow selector and data below 4 GiB and the site patched: a matching
selector reads `[base + disp]`, and a **mismatched one must reach the boundary**. Both
directions, because checking only the match cannot tell a guard from its absence.

**Reachable blocks**, which decide the unit.

### Out of scope

Base/index access forms; `FS`/`GS` overrides; and `kGuardedSegmentLoad`/`Read`/`Pop`, which
are about *writing* selectors and are a different problem.
