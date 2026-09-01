# 20260901-562 x64 return dispatch 작업 로그

## 한국어

### 결과

**x64에서 call과 return이 이어졌습니다.**

```text
returned_to_after_call  observed=0x4444 expected=0x4444
resolver_asked          observed=0x14000a expected=0x14000a
resolver_calls          observed=0x1 expected=0x1
esp_balanced            observed=0x20001800 expected=0x20001800
```

호출하고, 피호출자가 실행되고, `ret`이 resolver에게 물어, **호출 다음 명령이
실행됐습니다.** 값 셋이 실패 셋을 구분하도록 짰습니다 — `0x3333`이면 피호출자에서
멈춘 것, `0x1111`이면 call도 가지 않은 것입니다.

| 항목 | 559 | 560 | 561 | 562 |
|---|---:|---:|---:|---:|
| 방출 가능 | 66.17% | 77.60% | 84.62% | **86.46%** |
| **완결 block** | 2.66% | 38.32% | 59.90% | **64.13%** |
| non-copy 거부 | 12,856 | 6,009 | 1,805 | **700** |

### return이 앞의 것들과 다른 이유

Task 560·561이 방출한 edge는 전부 대상이 emit 시점에 알려져 rel32 하나로 끝났습니다.
`ret`은 대상이 **실행 중에 guest stack에서 나오는 guest 주소**이고 뛸 곳은 **cache
주소**입니다. emit 시점에 둘을 잇는 것이 없으므로 실행 중에 물어야 합니다.

### 설계에서 배제한 것 셋

| 대안 | 왜 안 되는가 |
|---|---|
| `jmp rel32`로 thunk 호출 | cache와 engine image 사이 거리가 Task 554의 배치 사다리 때문에 보장되지 않음 |
| `jmp qword ptr [rip+disp]` | code 한가운데 8바이트 데이터가 남고 **검증기가 그것을 명령으로 decode**함 — Task 561이 정확히 그 형태로 걸림 |
| scratch로 R13 | **실행 harness가 state 포인터로 사용 중** — emitted code가 harness를 부숨 |

남은 것이 R12 + `movabs`입니다.

### alignment는 가정하지 않고 강제했습니다

thunk는 `call`이 아니라 `jmp`로 도달하므로 진입 시 RSP 위상에 계약이 없습니다.
`and rsp, -16`으로 맞추고 되돌립니다. 위상을 계산해 `sub rsp, 8`로 맞추는 것은 진입
경로가 하나뿐일 때만 성립하는 가정이고, 지금은 하나지만 그것이 계약은 아닙니다.

### 규칙이 바뀌자 그 규칙을 검사하던 probe가 빨개졌습니다

`long_mode_emission`이 "`kCopy` 외 전부 boundary"를 **`kReturn`으로** 검사하고
있었습니다. 562가 정확히 그것을 바꿨으므로 probe는 **과거를 주장하고 있었습니다.**
아직 slot이 없는 `kPortIo`로 옮겼습니다.

> 규칙을 바꾸면 그 규칙을 검사하던 것도 함께 바뀌어야 한다. 빨개지는 것이 그 사실을
> 알려주는 방법이다.

### census가 어긋난 것을 스스로 잡았습니다

`agrees=false`가 나왔고 차이가 정확히 1,105 — return 수였습니다.

여기서 새 성질이 하나 생겼습니다. **return 방출은 host에 따라 달라지는 첫 번째
결과입니다** — thunk가 있어야 합니다. 그 전까지 long-mode 판정은 전부 바이트에 대한
판단이라 어느 host에서든 같은 답이었습니다.

그래서 census가 `#if`를 복사하지 않고 `LongModeReturnDispatchAvailable()`로 **emitter
에게 묻도록** 했습니다. 복사한 규칙이 어긋나는 일이 방금 일어났으므로, 같은 방식을
한 번 더 쓸 이유가 없습니다.

### 아직 아닌 것

**guest는 여전히 실행되지 않습니다.** Task 544의 fence가 그대로이고, 이번에 이어진
call/return은 **probe가 만든 프로그램**입니다. resolver도 probe의 것으로, image의
address map을 그대로 조회합니다. engine runtime이 x64에 닿으면 전역 셋이 실제
`ThreadContext`의 필드가 됩니다.

그리고 **inline cache가 없습니다.** 지금은 모든 return이 resolver를 부릅니다. 먼저
이어진 뒤에 빠르게 하는 순서입니다.

### 검증

| Host | 결과 |
|---|---|
| Linux x64 Release | `core_probe_all=true`, 20/20, skipped 2 |
| Linux i386 Release | `core_probe_all=true`, 19/19, skipped 3 |
| Win32 x86 Debug | `core_probe_all=true`, 19/19, skipped 3 |

census `agrees=true`. i386과 Win32는 thunk가 없으므로 return이 계속 boundary이고,
`LongModeReturnDispatchAvailable()`이 그 사실을 census에도 그대로 전합니다.

### 다음

남은 non-copy 700 중 `kHleBoundary` 177, `kPortIo` 138, `kIndirectExit` 109,
`kJumpTable` 22, guarded segment 셋이 171입니다. 어느 것도 지배적이지 않으므로
**다음 단위는 수가 아니라 무엇이 실행을 막는가로 골라야 합니다.**

## English

### Result

**A call and a return joined on x64.** Control called in, the callee ran, the `ret` asked
the resolver, and the instruction after the call executed. Three values were arranged to
separate three failures: `0x3333` would have meant stopping in the callee, `0x1111` that
the call never went.

| Item | 559 | 560 | 561 | 562 |
|---|---:|---:|---:|---:|
| Emittable | 66.17% | 77.60% | 84.62% | **86.46%** |
| **Complete blocks** | 2.66% | 38.32% | 59.90% | **64.13%** |
| non-copy refusals | 12,856 | 6,009 | 1,805 | **700** |

### Why return differs from what came before

Every edge Tasks 560 and 561 emitted had its target known at emit time, so one rel32
finished it. A `ret`'s target is a **guest address that appears on the guest stack at run
time** and the place to jump is a **cache address**. Nothing at emit time joins them.

### Three alternatives ruled out

| Alternative | Why not |
|---|---|
| `jmp rel32` to the thunk | the cache-to-engine distance is not guaranteed (Task 554's placement ladder) |
| `jmp qword ptr [rip+disp]` | leaves eight bytes of data inside code that **the verifier decodes as instructions** -- the shape Task 561 was caught by |
| R13 as the scratch | **the execution harness holds its state pointer there**; emitted code would break the harness |

What is left is R12 and `movabs`.

### Alignment forced, not assumed

The thunk is reached by `jmp`, so nothing promises RSP's phase. `and rsp, -16` is right
for any entry path; computing the phase would hold only while there is one, and there
being one today is not a contract.

### Changing the rule turned the probe that checked it red

`long_mode_emission` was checking "everything but `kCopy` is a boundary" **using a
`kReturn`**, which is exactly what this unit changed -- so the probe was asserting the
past. It moved to `kPortIo`, a kind that still has no slot.

> Change a rule and the thing checking that rule has to change with it. Going red is how
> that gets said.

### The census caught its own drift

`agrees=false`, with the difference exactly 1,105 -- the return count.

A new property arrived with it: **emitting a return is the first long-mode outcome that
depends on the host**, because it needs the thunk. Everything before it was a judgement
about bytes and answered the same everywhere.

So the census asks the emitter through `LongModeReturnDispatchAvailable()` rather than
copying the `#if`. A copied rule drifting is what had just happened; there was no reason
to do it a second time.

### What this is not yet

**The guest still does not run.** Task 544's fence stands, and what joined up here is **a
program the probe built**, with the probe's own resolver reading the image's address map.
When the engine runtime reaches x64, the three globals become fields it owns.

**There is no inline cache.** Every return calls the resolver today. Joining comes before
making it fast.

### Verification

| Host | Result |
|---|---|
| Linux x64 Release | `core_probe_all=true`, 20 of 20, 2 skipped |
| Linux i386 Release | `core_probe_all=true`, 19 of 19, 3 skipped |
| Win32 x86 Debug | `core_probe_all=true`, 19 of 19, 3 skipped |

Neither 32-bit host has the thunk, so returns stay boundaries there and
`LongModeReturnDispatchAvailable()` tells the census the same.

### Next

Of the 700 non-copy records left: `kHleBoundary` 177, `kPortIo` 138, `kIndirectExit` 109,
the three guarded-segment kinds 171, `kJumpTable` 22. None dominates, so **the next unit
has to be chosen by what blocks execution rather than by count.**
