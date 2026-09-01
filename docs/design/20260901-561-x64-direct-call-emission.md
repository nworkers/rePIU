# 20260901-561 x64 direct call emission 설계

## 한국어

### 목적

long-mode emission에서 `kDirectCall`을 방출합니다. Task 560 이후 남은 non-copy 6,009
중 **4,204(69.96%)** 가 이것입니다.

### 조각이 둘 다 이미 있습니다

i386의 `kDirectCall` slot은 두 부분입니다.

```
68 <fallthrough>     guest 복귀 주소를 guest stack에 push
E9 <rel32>           호출 대상으로 점프
```

x64에서 각각에 대응하는 것이 이미 존재합니다.

| 부분 | 있는 것 |
|---|---|
| push | Task 559의 stack sequence lowering |
| jump | Task 560의 direct edge 방출 |

`68 imm32`를 `LowerLongModeBytes`에 넣으면 실제로 나오는 것을 확인했습니다.

```text
45 8D 7F FC          lea r15d, [r15-4]        ESP -= 4, flag 불변
41 C7 07 78563412    mov dword ptr [r15], imm32
```

**따라서 이 단위는 새 시퀀스를 쓰는 것이 아니라 둘을 잇는 것입니다.** push 바이트를
직접 만들어 넣지 않고 `{0x68, imm32}`를 합성해 기존 lowering에 통과시키는 이유가
그것입니다 — 시퀀스가 한 곳에만 있으면 `LEA`가 `SUB`가 되는 종류의 회귀가 두 곳으로
갈라지지 않습니다.

### 결정

#### 1. push는 합성해서 기존 lowering에 통과시킵니다

`kDirectCall` 레코드의 `fallthrough_target`을 imm32로 하는 `68` 명령을 만들어
`LowerLongModeBytes`에 넘깁니다. 거절하면 boundary로 갑니다 — 직접 바이트를 쓰면
거절이라는 답 자체가 없어집니다.

#### 2. jump는 Task 560과 같은 slot입니다

`E9 rel32` + `kDirectCall` fixup. 대상이 cache 밖이면 560이 만든 것과 같은 경로로
boundary가 됩니다.

#### 3. timer safe point는 여기에도 없습니다

i386의 call slot에도 없습니다. 새로 뺀 것이 아닙니다.

### 이것만으로 실행이 이어지지는 않습니다

정직하게 적어 둡니다. call을 방출해도 **호출된 함수의 `ret`은 여전히 boundary**입니다.
guest stack에는 올바른 복귀 주소가 들어가지만, 그 guest 주소를 cache 주소로 바꿀
dispatch resolver가 없습니다.

즉 이 단위가 늘리는 것은 **방출 범위와 완결 block**이고, 체인이 실제로 길어지는 것은
`kReturn`(1,105)의 resolver가 생긴 뒤입니다. 둘은 짝입니다 — 어느 하나만으로는 함수를
드나들 수 없습니다.

### 검증 — 실행으로

호출자 block과 피호출자 block을 가진 이미지를 만들어 실행합니다. 확인할 것은 둘입니다.

1. **점프가 실제로 갔는가** — 피호출자만 쓰는 값이 나와야 합니다.
2. **복귀 주소가 guest stack에 실제로 들어갔는가** — guest stack 메모리를 직접 읽어
   `fallthrough_target`과 같은지 봅니다. `ESP`가 -4 됐는지도 함께 봅니다.

두 번째가 없으면 "call을 방출했다"가 아니라 "점프를 방출했다"를 확인한 것입니다.

### 비범위

- `kReturn`과 dispatch resolver (다음 단위)
- 간접 call, `kIndirectExit`, `kJumpTable`

## English

### Objective

Emit `kDirectCall` under long-mode emission -- **4,204 of the 6,009 non-copy records
(69.96%)** left after Task 560.

### Both pieces already exist

The i386 slot is a push of the guest return address followed by a jump to the target, and
x64 already has each half: Task 559's stack-sequence lowering, and Task 560's direct edge.
Feeding `68 imm32` through `LowerLongModeBytes` was checked and produces

```text
45 8D 7F FC          lea r15d, [r15-4]        ESP -= 4, no flags touched
41 C7 07 78563412    mov dword ptr [r15], imm32
```

**So this unit joins two things rather than writing a new sequence.** The push bytes are
synthesised as `{0x68, imm32}` and passed through the existing lowering rather than written
directly, so the sequence stays in one place -- a regression of the kind Task 559 found,
where `LEA` became `SUB` and destroyed the guest's flags, must not have two homes.

### Decisions

1. **Synthesise the push and lower it.** Build a `68` instruction whose imm32 is the
   record's `fallthrough_target` and hand it to `LowerLongModeBytes`. A refusal becomes a
   boundary; writing the bytes directly would remove refusal as a possible answer.
2. **The jump is Task 560's slot.** `E9 rel32` with a `kDirectCall` fixup, and an
   out-of-cache target becomes a boundary through the path 560 built.
3. **No timer safe point**, which the i386 call slot does not have either.

### This alone does not make execution continue

Recorded plainly: emitting the call still leaves the callee's `ret` at a boundary. The
correct return address reaches the guest stack, but nothing turns that guest address into
a cache address.

What this unit raises is **emission coverage and complete blocks**. Chains lengthen only
once `kReturn` (1,105) has its resolver. The two are a pair; neither alone lets control
both enter and leave a function.

### Verification -- by execution

An image with a caller block and a callee block, run. Two things are checked:

1. **that the jump happened** -- a value only the callee writes must come back, and
2. **that the return address really reached the guest stack** -- read the guest stack
   memory directly and compare it with `fallthrough_target`, and check `ESP` moved by -4.

Without the second, what is verified is "a jump was emitted", not "a call was emitted".

### Out of scope

`kReturn` and the dispatch resolver; indirect calls, `kIndirectExit`, `kJumpTable`.
