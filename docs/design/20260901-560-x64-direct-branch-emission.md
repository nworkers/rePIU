# 20260901-560 x64 direct branch emission 설계

## 한국어

### 목적

long-mode emission에서 `kDirectJump`와 `kConditionalBranch`를 방출합니다. Task 559
이후 완결 block이 2.66%에 그친 이유가 **block이 control flow로 끝나는데 그것이 하나도
방출되지 않기 때문**이고, 이 둘이 그중 가장 싼 절반입니다.

### 측정이 순서를 정했습니다

`not-a-copy-record` 12,856을 plan kind로 나눈 결과입니다(Task 560 census).

| kind | 개수 | 비중 | 필요한 것 |
|---|---:|---:|---|
| `kConditionalBranch` | 5,202 | 40.46% | `0F 8x rel32` — long mode에서 그대로 유효 |
| `kDirectCall` | 4,204 | 32.70% | guest 주소 push + `E9` — stack lowering 필요 |
| `kDirectJump` | 1,663 | 12.94% | `E9 rel32` — 그대로 유효 |
| `kReturn` | 1,105 | 8.60% | inline cache slot — **resolver 필요** |
| 나머지 8종 | 682 | 5.30% | HLE·port I/O·segment·jump table |

**block terminator가 12,174 / 12,856 (94.7%)** 입니다. 이번 단위는 새 기계장치가 전혀
필요 없는 `kConditionalBranch`+`kDirectJump` **6,865(53.4%)** 만 다룹니다.
`kDirectCall`은 guest stack을 건드리고 `kReturn`은 resolver를 요구하므로 각각 별도
단위입니다.

### 이미 있는 선례를 따릅니다

block fallthrough가 이 문제를 이미 이렇게 풀었습니다.

```cpp
// The `E9 rel32` below is emitted in both modes: its encoding and its
// meaning are the same in long mode. The timer safe point in front of
// it is not -- it is a hand-built 32-bit `pushfd`/`popfd` sequence, so
// a long-mode image goes without one.
```

이 단위는 같은 판단을 두 kind에 확장하는 것이지 새 판단을 세우는 것이 아닙니다.

### 결정

#### 1. 두 encoding은 그대로 방출합니다

`E9 rel32`와 `0F 8x rel32`는 long mode에서 encoding도 의미도 같습니다. rel32는 cache
내부 상대 변위이고, cache는 한 덩어리로 2 GiB보다 훨씬 작으므로 범위도 보장됩니다.

#### 2. timer safe point는 x64에서 붙이지 않습니다

`EmitTimerSafePoint`는 세 군데가 조용히 틀립니다.

| 바이트 | 32비트 | 64비트 |
|---|---|---|
| `9C` / `9D` | `pushfd` / `popfd` (4바이트) | `pushfq` / `popfq` — **8바이트, host RSP** |
| `83 3D <abs32> 00` | `cmp dword ptr [abs32],0` | **RIP-relative** — 다른 주소를 읽음 |

셋 다 예외를 일으키지 않습니다. Task 550이 분류한 "조용히 다른 명령" 부류 그대로이며,
fallthrough가 같은 이유로 이미 제외하고 있습니다. **backward edge가 x64에서 timer
safe point를 잃는다는 뜻이고, guest가 아직 실행되지 않으므로 지금은 기록으로 충분합니다.**

#### 3. cache 밖 target은 32비트 stub이 아니라 boundary로 보냅니다

target이 cache 안에 없으면 `EmitUnresolvedDirectEdgeDispatch`가 `68 imm32` 기반 32비트
stub을 냅니다. 그것은 x64에서 쓸 수 없고, `enable_dbt_direct_edge_dispatch`는 기본
`false`라 그 경로는 **이미지 빌드 전체를 실패시킵니다**.

long mode에서는 그 slot을 `0xCC`로 덮고 boundary로 셉니다. Task 553 결정 2의
fail-closed를 그대로 유지하는 선택이며, **오늘 성립하는 이미지 빌드가 이번 변경으로
실패하지 않게 하는 것**이기도 합니다.

덮어쓸 위치는 fixup의 `cache_patch_offset`에서 kind로 역산합니다 — `kDirectJump`는
`-1`(`E9`), `kConditionalBranch`는 `-2`(`0F 8x`). **덮기 전에 그 바이트가 예상한
opcode인지 확인하고, 아니면 덮지 않고 실패로 답합니다.** 배치가 바뀌면 조용히 망가지는
대신 드러나야 합니다.

#### 4. 조건 opcode를 읽지 못하면 지금처럼 `0xCC`입니다

`ReadConditionOpcode`가 실패하는 형태는 i386에서도 이미 INT3입니다. 바꾸지 않습니다.

### 검증 — 실행으로

이 저장소의 기준은 Task 558 이후 "방출된 바이트를 실행한다"입니다. 분기는 실행하지
않으면 검증되지 않으므로, 실행 harness에 **조건 분기가 성립하는 경우와 성립하지 않는
경우**를 넣어 서로 다른 값이 나오는지 봅니다. 한 방향만 확인하면 분기가 아니라 직진을
확인한 것입니다.

### 비범위

- `kDirectCall` (4,204) — guest stack에 guest 주소를 push해야 합니다
- `kReturn` (1,105) — dispatch resolver가 있어야 합니다
- x64 timer safe point의 재인코딩
- cache 밖 edge의 x64 dispatch stub

## English

### Objective

Emit `kDirectJump` and `kConditionalBranch` under long-mode emission. Complete blocks
stalled at 2.66% after Task 559 because **a block ends in control flow and none of it is
emitted**, and these two are the cheapest half of that.

### The measurement set the order

Splitting `not-a-copy-record`'s 12,856 by plan kind (Task 560's census):

| Kind | Count | Share | What it needs |
|---|---:|---:|---|
| `kConditionalBranch` | 5,202 | 40.46% | `0F 8x rel32` — unchanged in long mode |
| `kDirectCall` | 4,204 | 32.70% | pushing a guest address — needs the stack lowering |
| `kDirectJump` | 1,663 | 12.94% | `E9 rel32` — unchanged |
| `kReturn` | 1,105 | 8.60% | an inline cache slot — **needs the resolver** |
| the other eight | 682 | 5.30% | HLE, port I/O, segments, jump tables |

**Block terminators are 12,174 of 12,856 (94.7%).** This unit takes only
`kConditionalBranch` + `kDirectJump`, **6,865 (53.4%)**, which need no new machinery.
`kDirectCall` touches the guest stack and `kReturn` requires the resolver; each is its own
unit.

### It follows a precedent that already exists

The block fallthrough solved this problem already, and this unit extends the same
judgement to two more kinds rather than making a new one.

### Decisions

1. **Emit both encodings unchanged.** `E9 rel32` and `0F 8x rel32` mean the same thing in
   long mode, and rel32 is a displacement inside one cache image far smaller than 2 GiB.
2. **No timer safe point on x64.** `EmitTimerSafePoint` is wrong in three places at once:
   `9C`/`9D` become 8-byte `pushfq`/`popfq` against the *host* RSP, and
   `cmp dword ptr [abs32],0` becomes RIP-relative. None of them raise. The fallthrough
   already excludes it for the same reason. Backward edges therefore lose their safe point
   on x64; the guest does not run there yet, so recording it is enough for now.
3. **An out-of-cache target becomes a boundary, not the 32-bit stub.**
   `EmitUnresolvedDirectEdgeDispatch` builds a `68 imm32` sequence that x64 cannot use, and
   `enable_dbt_direct_edge_dispatch` defaults to false, so that path **fails the whole
   image build**. In long mode the slot is overwritten with `0xCC` and counted as a
   boundary, which keeps Task 553's fail-closed rule and keeps today's working image build
   from breaking on this change. The position is derived from the fixup's
   `cache_patch_offset` by kind — `-1` for `E9`, `-2` for `0F 8x` — and **the byte there is
   checked against the expected opcode before it is overwritten**, so a changed layout
   surfaces instead of corrupting.
4. **An unreadable condition opcode stays `0xCC`,** exactly as on i386.

### Verification -- by execution

Since Task 558 the standard here is that emitted bytes are run. A branch that is not
executed is not verified, so the harness gets a conditional branch **taken and not taken**
and checks the two produce different values. Checking one direction only would be checking
a straight line.

### Out of scope

`kDirectCall` (4,204), `kReturn` (1,105), re-encoding the timer safe point for x64, and an
x64 dispatch stub for out-of-cache edges.
