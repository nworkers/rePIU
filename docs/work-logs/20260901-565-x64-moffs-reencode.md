# 20260901-565 x64 moffs 재인코딩 작업 로그

## 한국어

### 결과 — 도달 가능 block이 처음으로 움직였습니다

| 항목 | 563 | 564 | 565 |
|---|---:|---:|---:|
| 방출 가능 | 86.46% | 96.74% | **97.88%** |
| 완결 block | 64.13% | 83.01% | **86.13%** |
| **도달 가능 block** | 1 | 1 | **8** |
| serviced 통과 | — | — | **12** |
| 정지 지점 | `stack-pointer` | moffs | **segment override** |

moffs 재인코딩을 실행으로 확인했습니다.

```text
long_mode_lowering_moffs=true,read=0x5a17c0de,wrote=0x1234abcd
```

읽기만 봤다면 저장 형태가 같은 주소에 닿는지 알 수 없었을 것입니다.

### 세 가지가 드러났고, 두 가지는 제 잘못이었습니다

#### 1. 제 제한이 정확히 문제의 명령을 놓쳤습니다

Task 557의 방침을 따라 "맨 5바이트 형태만" 통과시키고, 설계에 **"그 제한이 무엇을
놓치는지는 census가 잰다"** 고 적었습니다. census가 즉시 답했습니다 — 216개가 남았고,
그중 **막고 있던 것이 `66 a3 24 66 1a 01`**, 즉 `mov [0x011A6624], ax`였습니다. 제외한
`66` prefix 형태 바로 그것입니다.

추측하지 않고 **막는 바이트를 찍도록** census를 고친 덕에 한 번에 나왔습니다. Task 564가
"다음은 moffs일 것"이라고 예측했다가 형태를 틀린 직후라, 이번에는 사실로 정했습니다.

#### 2. 제 walk가 벽과 문을 구분하지 않았습니다

`66 A3`를 통과시키자 다음 정지가 `cd 21` — **`INT 21h`** 였습니다. 그건 emitter가 못
내는 것이 아니라 **설계상 HLE dispatcher가 처리하는 것**이고, i386에서는 handler가
서비스한 뒤 다음 명령에서 실행이 이어집니다.

Task 563의 walk는 방출되지 않은 모든 record에서 멈췄고, 그것이 **벽과 문을 같은 것으로
셌습니다.** serviced boundary(`kHleBoundary`, `kPortIo`)를 통과하도록 고치자 도달 가능이
1에서 8로, 통과한 serviced block이 12로 나왔습니다.

> 측정이 낮았던 이유의 일부는 측정의 정의였다.

두 수를 따로 보고합니다. "runtime 도움 없이 갈 수 있는 곳"과 "dispatcher가 제 일을 하면
갈 수 있는 곳"은 다른 주장입니다.

#### 3. 제가 심각한 버그를 만들었고 probe가 잡았습니다

`A0`–`A3`를 거부 목록에서 빼면서 **거기 붙어 있던 `return true;`까지 지웠습니다.** 그
결과 `62`(BOUND)·`63`(ARPL)·`C4`(LES)·`C5`(LDS)가 전부 `default: return false`로 떨어져
**거부되지 않게** 됐습니다. 이 넷은 Task 550이 "조용히 다른 명령이 되는" 부류로 분류한
것들이고, 그대로 복사됐다면 실행되는 잘못된 프로그램이 나왔을 것입니다.

`long_mode_refused_arpl=false` 한 줄이 잡았습니다. Task 550이 이 probe를 **거부를
증명하도록** 쓴 이유가 정확히 이것입니다.

> 통과 목록만 확인하는 probe는 모든 것을 허용하는 판정기에 대해서도 통과한다.

그때 적어 둔 그 문장이 오늘 실제로 값을 했습니다.

### 검증

| Host | 결과 |
|---|---|
| Linux x64 Release | `core_probe_all=true`, 20/20, skipped 2 |
| Linux i386 Release | `core_probe_all=true`, 19/19, skipped 3 |
| Win32 x86 Debug | `core_probe_all=true`, 19/19, skipped 3 |

census `agrees=true`.

### 다음

정지 지점이 `0x10f4c83`, 바이트는 `26 8b 1d 5c 00 00 00` = **`mov ebx, es:[0x5c]`**
입니다. frontier는 `kGuardedSegmentLoad` 2개와 `kSegmentOverrideMem` 2개입니다.

segment override는 Task 552가 명시적으로 미뤄 둔 항목입니다. 다만 **`ES`는 `FS`/`GS`와
다릅니다** — long mode에서 `CS`/`DS`/`ES`/`SS` override는 무시되고 base가 0입니다. guest가
flat model이면 prefix를 떼는 것만으로 같은 의미가 될 수 있고, i386 경로가 이미
`kSegmentOverrideMem`에서 base를 displacement에 접어 넣는 guard를 씁니다. **확인하고
정할 문제이지 지금 단정할 것은 아닙니다.**

## English

### Result -- reachable blocks moved for the first time

| Item | 563 | 564 | 565 |
|---|---:|---:|---:|
| Emittable | 86.46% | 96.74% | **97.88%** |
| Complete blocks | 64.13% | 83.01% | **86.13%** |
| **Reachable blocks** | 1 | 1 | **8** |
| Walked through serviced | — | — | **12** |

The re-encoding was run: `read=0x5a17c0de, wrote=0x1234abcd`. A read alone would not have
shown the store form reaching the same address.

### Three things surfaced, two of them mine

#### 1. The restriction missed exactly the instruction that mattered

Following Task 557's policy, only the bare five-byte form was admitted, and the design
said **the census would measure what the restriction cost**. It answered immediately: 216
were left behind, and the one still blocking the chain was `66 a3 24 66 1a 01` --
`mov [0x011A6624], ax`, the operand-size form that had been excluded.

Printing the blocking bytes rather than reasoning about them settled it in one round.
Task 564 had just predicted "moffs next" and got the shape wrong, so this time it was made
a fact.

#### 2. The walk did not tell a wall from a door

With `66 A3` admitted, the next stop was `cd 21` -- **`INT 21h`**, which the emitter is
never going to produce because it belongs to the HLE dispatcher, and which on i386 is
serviced before execution carries on at the next instruction.

Task 563's walk stopped at every unemitted record, **counting doors as walls**. Passing
through serviced kinds gives eight reachable blocks and twelve walked through.

> Part of why the number was low was the definition of the number.

Both are reported. "Where execution gets with no runtime help" and "where it gets if the
dispatcher does its job" are different claims.

#### 3. I introduced a serious bug and the probe caught it

Taking `A0`–`A3` off the refusal list removed **the `return true;` those cases shared**,
dropping `62` (BOUND), `63` (ARPL), `C4` (LES) and `C5` (LDS) through to
`default: return false`. Those four are Task 550's "quietly a different instruction"
class; copied, they would have produced a program that runs and is wrong.

One line, `long_mode_refused_arpl=false`, found it -- which is exactly why Task 550 wrote
that probe to **prove refusals**:

> A probe that checks only the pass list also passes against a classifier that allows
> everything.

That sentence earned its keep today.

### Verification

| Host | Result |
|---|---|
| Linux x64 Release | `core_probe_all=true`, 20 of 20, 2 skipped |
| Linux i386 Release | `core_probe_all=true`, 19 of 19, 3 skipped |
| Win32 x86 Debug | `core_probe_all=true`, 19 of 19, 3 skipped |

### Next

The stop is at `0x10f4c83`, bytes `26 8b 1d 5c 00 00 00` -- **`mov ebx, es:[0x5c]`** -- and
the frontier is two `kGuardedSegmentLoad` and two `kSegmentOverrideMem`.

Segment overrides are what Task 552 deferred explicitly. But **`ES` is not `FS`/`GS`**: in
long mode the `CS`/`DS`/`ES`/`SS` overrides are ignored and their base is zero, so under a
flat guest model dropping the prefix could mean the same thing -- and the i386 path
already folds a segment base into the displacement behind a guard. **That is something to
check and decide, not to assert now.**
