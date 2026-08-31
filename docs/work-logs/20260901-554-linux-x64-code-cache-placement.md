# 20260901-554 Linux x64 code cache 배치 작업 로그

설계: [20260901-554](../design/20260901-554-linux-x64-code-cache-placement.md) ·
작업 지시: [20260901-554](../work-orders/20260901-554-linux-x64-code-cache-placement.md)

## 한국어

### 결과

**x64 host가 code cache를 배치합니다.** 이전에는 하나도 배치하지 못했습니다.

Task 546 구현 순서 4단계(dispatch resolver)로 들어가려다 그 앞에 막힌 것을 찾았습니다.
resolver가 frame에 써 넣는 continuation이 code cache 주소인데, **x64에는 배치된 cache가
아예 없었습니다.**

### 무엇이 막고 있었나

code cache 주소는 host pointer인데 engine은 그것을 `std::uint32_t`에 담습니다 —
`AotCodeCachePlacement::base_address`, 그리고 그것을 되읽는 `FindAotGuestAddress`와
`FindAotCacheAddress`. i386에서는 항등이고 x86-64에서는 절단입니다. engine은 절단하지 않고
**거절합니다**: `AOT code cache is outside the x86 address range`.

그리고 `PlaceAotCodeCache`는 hint 없이 요청합니다. 커널이 무엇을 주는지 쟀습니다.

| 요청 | 결과 | 하위 4 GiB |
|---|---|---|
| `hint=NULL` | `0x00007fddf72e8000` | 아니오 |
| `MAP_32BIT` | `0x00000000419d5000` | 예 |
| `hint=0x20000000` | `0x0000000020000000` | 예 |
| `hint=0x50000000` | `0x0000000050000000` | 예 |

두 사실을 합치면 결론이 하나입니다. **hint 없이 요청 → 4 GiB 위 → 거절.** step 4의 모든
작업이 이 아래에 있었습니다.

Task 551의 `mmap_min_addr` 여유 0과 같은 종류입니다. 거기서는 기본값이 마침 맞아 i386에서
보이지 않았고, 여기서는 기본값이 맞지 않아 전부 실패했습니다.

### 넓히지 않고 낮췄습니다

`base_address`를 `uintptr_t`로 넓히는 것이 대안이었고, **참조가 121곳**입니다. 배치를
낮추기로 한 이유 중 결정적인 것 하나:

> **cache 주소는 C++ 필드만이 아닙니다.** 방출된 바이트 안에도 있습니다 — inline cache의
> `abs32` patch target, timer safe point의 request address, jump table 항목.
> **C++ 필드를 넓혀도 방출된 `disp32`는 넓어지지 않습니다.**

그리고 guest arena는 이미 하위 4 GiB가 요구사항이므로(결정 4, Task 551), cache도 낮게 두면
cache↔guest 상호 참조가 전부 `disp32`/`rel32` 범위에 남습니다.

배치 정책은 `src/runtime/aot_code_cache_reservation.cpp`로 분리했습니다. 질문이 하나인
정책이고, engine을 링크하는 probe는 렌더러까지 끌고 오기 때문입니다 — 실제로 처음에
engine 헤더로 probe를 쓰다가 `glGetError` 미해결로 링크가 깨져서 알았습니다.

### 측정 — 대비가 이 작업의 증거입니다

같은 probe, 사다리만 끄고 켰습니다.

| 사다리 | x64 base | addressable |
|---|---|---|
| 끔 (이전 동작) | `0x00007f733924e000` | **0** |
| 켬 | `0x0000000020000000` | **1** |

그리고 사다리 rung이 하나여도 안 되는 이유가 같은 probe에 있습니다. 동시에 둘을 잡으면
두 번째가 다음 후보로 갑니다 — `MAP_FIXED_NOREPLACE`라 이미 있는 자리는 덮지 않고
실패하기 때문입니다.

```text
code_cache_concurrent_0 base=0x20000000
code_cache_concurrent_1 base=0x28000000
```

| Host | 결과 | attempt |
|---|---|---|
| Linux x64 Release | `core_probe_all=true`, 19/19, skipped 2 | `0` (`0x20000000`) |
| Linux i386 Release | `core_probe_all=true`, 19/19 | `unhinted` |
| Win32 x86 Debug | `core_probe_all=true`, 19/19 | `unhinted` |

**32비트 host는 사다리를 타지 않습니다.** 두 호스트 모두 `attempt=unhinted`이고, 주소는
예전과 같은 방식으로 정해집니다. i386 회귀로 emitter를 직접 쓰는 Windows probe 셋과
`dos4gw_hello` 정적 AOT도 확인했고, 후자는 여전히 Task 434가 기록한 기존 한계에서 멈춥니다.

### 아직 아닌 것

배치가 됐을 뿐입니다. **x64 dispatch slot 방출도, thunk도, resolver 본체도 없습니다.**
guest 실행은 Task 544의 fence 그대로입니다.

## English

### Result

**An x64 host places a code cache.** Before this, it placed none at all.

The blocker turned up on the way into Task 546's step 4 (the dispatch resolver): the
continuation a resolver writes into the frame is a code cache address, and **x64 had no
placed cache to name.**

### What was blocking it

A code cache address is a host pointer, and the engine keeps it in a `std::uint32_t` --
`AotCodeCachePlacement::base_address`, plus the `FindAotGuestAddress` and
`FindAotCacheAddress` pair that reads it back. On i386 that is the identity; on x86-64 it
is a truncation, and the engine **refuses** rather than truncating:
`AOT code cache is outside the x86 address range`.

And `PlaceAotCodeCache` asks without a hint. What the kernel gives back was measured:

| Request | Result | Below 4 GiB |
|---|---|---|
| `hint=NULL` | `0x00007fddf72e8000` | no |
| `MAP_32BIT` | `0x00000000419d5000` | yes |
| `hint=0x20000000` | `0x0000000020000000` | yes |
| `hint=0x50000000` | `0x0000000050000000` | yes |

Put the two together and there is one conclusion: **ask with no hint, land above 4 GiB, be
refused.** Every part of step 4 sat below that.

It is the same kind of thing as Task 551's zero `mmap_min_addr` headroom. There the
defaults happened to line up and it stayed invisible on i386; here they did not, and
everything failed.

### Lowered rather than widened

Widening `base_address` to `uintptr_t` was the alternative, and it has **121 references**.
One reason decided it:

> **A cache address is not only a C++ field.** It is also inside the emitted bytes -- the
> inline cache's `abs32` patch targets, the timer safe point's request address, the jump
> table's entries. **Widening a C++ field does not widen an emitted `disp32`.**

And since the guest arena is already required to be below 4 GiB (decision 4, Task 551),
keeping the cache low keeps every cache-to-guest cross-reference inside `disp32` and
`rel32` range.

The policy moved into `src/runtime/aot_code_cache_reservation.cpp`. It is a policy with one
question, and a probe that links the engine drags in the renderer -- which is how this was
found: the first version of the probe included the engine header and failed to link on
unresolved `glGetError`.

### Measured -- the contrast is the evidence

The same probe, with the ladder off and on.

| Ladder | x64 base | addressable |
|---|---|---|
| off (the previous behaviour) | `0x00007f733924e000` | **0** |
| on | `0x0000000020000000` | **1** |

The same probe also shows why one rung would not do. Hold two at once and the second takes
the next candidate, because the mapping is `MAP_FIXED_NOREPLACE` and refuses rather than
displacing what is already there.

```text
code_cache_concurrent_0 base=0x20000000
code_cache_concurrent_1 base=0x28000000
```

| Host | Result | attempt |
|---|---|---|
| Linux x64 Release | `core_probe_all=true`, 19 of 19, 2 skipped | `0` (`0x20000000`) |
| Linux i386 Release | `core_probe_all=true`, 19 of 19 | `unhinted` |
| Win32 x86 Debug | `core_probe_all=true`, 19 of 19 | `unhinted` |

**A 32-bit host does not walk the ladder.** Both report `attempt=unhinted`, and the address
is chosen the way it always was. The i386 regression also covers three Windows probes that
drive the emitter directly and the `dos4gw_hello` static AOT, which still stops at the
pre-existing limit Task 434 recorded.

### What this is not yet

Only the placement. **There is no x64 dispatch slot emission, no thunk, and no resolver
body.** Guest execution is still fenced exactly as Task 544 left it.
