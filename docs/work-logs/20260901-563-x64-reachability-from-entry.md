# 20260901-563 진입점 기준 도달 가능성 측정 작업 로그

## 한국어

### 결과

```text
reachable blocks    1  (0.01% of blocks)
reachable instrs    1
kCopy  stack-pointer            1
entry=0x10f4bb8 first stop=0x10f4c31
```

**진입점에서 실행이 갈 수 있는 block은 1개입니다.**

| 수치 | 값 |
|---|---:|
| 방출 가능 | 86.46% |
| 완결 block | 64.13% |
| **진입점에서 도달 가능한 block** | **1** |

세 단위 내내 진척을 읽어 온 두 수치가 "거의 다 왔다"처럼 보였지만, **실행 관점에서는
한 block도 나아가지 못합니다.** 두 수치가 틀린 것이 아니라 **다른 질문에 답하고**
있었습니다 — 무엇이 빠졌는가와 무엇이 가로막는가는 다른 질문입니다.

### 제 가설이 틀렸습니다

직전 단위를 마치며 "다음은 아마 engine runtime 연결일 것"이라고 적었습니다. **아닙니다.**
runtime을 붙여도 두 번째 block에서 멈춥니다. 남은 non-copy 700개도 아닙니다 — 체인이
거기까지 닿지도 않습니다.

막고 있는 것은 **`stack-pointer` 거부 6,401개**이고, 그중 첫 하나가 진입 직후에
있습니다.

### 왜 하필 거기인가

당연한 자리입니다. `0x10F4C31`은 진입 다음 block의 첫 명령이고, 프로그램 진입부가
가장 먼저 하는 일이 스택 프레임을 세우는 것입니다. Task 555가 `ESP` 쓰기를 거부하게
만든 이유 — long mode에서 `ESP`에 쓰면 zero-extend되어 **host의 `RSP`를 파괴**합니다 —
가 곧 진입부에서 걸리는 이유입니다.

### 6,401개의 형태

| mnemonic | 수 |
|---|---:|
| `mov` | 3,800 |
| `add` | 665 |
| `sub` | 455 |
| `fstp` | 338 |
| `push` | 233 |
| `lea` | 182 |
| `fild` | 174 |
| `cmp` | 127 |
| `fld` | 120 |
| 그 외 | 나머지 |

`mov`가 59%이고 대부분 `[esp+N]` **메모리 base** 형태입니다. `add`/`sub` 1,120은 `ESP`
자체를 operand로 쓰는 형태로, 두 경우가 재인코딩 방식이 다릅니다.

### 인수인계의 판단은 틀리지 않았습니다

3.10 인수인계는 `ESP` 재인코더(6,401)를 두고 "수로는 비슷해 보이지만 먼저 해도 block은
여전히 control flow에서 멈춘다"며 control flow를 앞에 두었습니다. **그 판단은 그때
맞았습니다** — 완결 block이 2.66%에서 64.13%로 갔습니다. 이제 순서가 뒤바뀌었을 뿐이고,
그 사실은 이 측정이 생기기 전에는 알 수 없었습니다.

### 검증

| Host | 결과 |
|---|---|
| Linux x64 Release | census 실행, 도달 가능 block 1 |
| Linux i386 Release | census 빌드 통과 |
| Win32 x86 Debug | census 빌드 통과 |

측정만 추가했으므로 emitter와 probe는 바뀌지 않았습니다.

### 다음

**`ESP` operand 재인코더.** guest `ESP`는 `R15D`에 있으므로, `ESP`를 이름으로 쓰는
명령은 `R15D`를 쓰도록 다시 인코딩해야 합니다 — 메모리 base인 경우 SIB base를, register
operand인 경우 ModRM rm을 바꾸고 둘 다 `REX.B`가 붙습니다.

## English

### Result

**Execution reaches exactly one block from the entry.**

| Number | Value |
|---|---:|
| Emittable | 86.46% |
| Complete blocks | 64.13% |
| **Blocks reachable from the entry** | **1** |

The two numbers this port's progress has been read through looked like "nearly there",
and **in execution terms nothing moves at all**. They are not wrong; they **answer a
different question**. What is missing and what is in the way are not the same.

### My prediction was wrong

Finishing the previous unit I wrote that the next thing was probably connecting the engine
runtime. **It is not.** With the runtime attached, execution would still stop at the
second block. Nor is it the 700 non-copy records -- no chain reaches them.

What blocks it is the **6,401 `stack-pointer` refusals**, the first of which sits
immediately after the entry.

### Why there, of all places

It is the obvious place. `0x10F4C31` is the first instruction of the block after the
entry, and setting up a stack frame is the first thing a program entry does. The reason
Task 555 made `ESP` writes a refusal -- in long mode writing `ESP` zero-extends and
**destroys the host's `RSP`** -- is the same reason it is hit at the entry.

### The shape of the 6,401

`mov` 3,800; `add` 665; `sub` 455; `fstp` 338; `push` 233; `lea` 182; `fild` 174;
`cmp` 127; `fld` 120; the rest smaller.

`mov` is 59% and mostly the `[esp+N]` **memory-base** form. The 1,120 `add` and `sub` name
`ESP` as a register operand. The two cases re-encode differently.

### The handoff's judgement was not wrong

Handoff 3.10 put control flow ahead of the `ESP` re-encoder, reasoning that doing 6,401
first would still leave every block stopping at its control flow. **That was right at the
time** -- complete blocks went from 2.66% to 64.13%. The order has simply reversed, and
that could not have been known before this measurement existed.

### Verification

| Host | Result |
|---|---|
| Linux x64 Release | census run; one reachable block |
| Linux i386 Release | census builds |
| Win32 x86 Debug | census builds |

Only a measurement was added; the emitter and the probes are unchanged.

### Next

**The `ESP` operand re-encoder.** Guest `ESP` lives in `R15D`, so an instruction naming
`ESP` must be re-encoded to name `R15D`: the SIB base where it is a memory base, the ModRM
`rm` where it is a register operand, with `REX.B` in both.
