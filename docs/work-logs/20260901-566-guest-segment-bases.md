# 20260901-566 guest segment base 측정 작업 로그

## 한국어

### 결과 — guest는 flat model이 아닙니다

```text
selector=0x1c base=0x1000000  limit=0xf       object=1
selector=0x24 base=0x1010000  limit=0xef0cf   object=2
selector=0x2c base=0x1100000  limit=0x47      object=3
selector=0x34 base=0x1110000  limit=0x4c6e5f  object=4
```

segment base가 0이 아니라 **재배치된 object base**입니다.

### 이것이 뒤집은 것

Task 565를 마치며 이렇게 적었습니다.

> `ES`는 `FS`/`GS`와 다릅니다 — long mode에서 `CS`/`DS`/`ES`/`SS` override는 무시되고
> base가 0입니다. guest가 flat model이면 prefix를 떼는 것만으로 같은 의미가 될 수
> 있고 (…) **확인하고 정할 문제이지 지금 단정할 것은 아닙니다.**

확인했고, **아닙니다.** base가 0이 아니므로 long mode가 override를 무시하는 것은
편의가 아니라 **정확히 틀린 답**입니다. `mov ebx, es:[0x5c]`는 `[0x5c]`가 아니라
`[ES_base + 0x5c]`이고, prefix를 떼면 예외 없이 다른 주소를 읽습니다.

Task 550이 분류한 "조용히 다른 명령이 되는" 부류와 같은 성질입니다.

### 그래서 x64 segment override는 두 변환의 합성입니다

i386이 이미 옳은 방법을 씁니다(`EmitSegmentOverrideSlot`) — prefix를 떼고, ModRM을
disp32 형태로 넓히고, **segment base를 displacement에 접어 넣고**, shadow selector가
어긋나면 boundary로 가는 guard를 답니다.

x64에서는 그 결과에 Task 552/564의 memory operand lowering이 한 번 더 얹혀야 합니다.
`0x67`을 붙이거나, base 없는 절대형이면 SIB로 다시 써야 합니다. **변환 둘을 합성하는
것이므로 별도 단위입니다.**

### 이번 구간에서 추측이 세 번 졌습니다

| 단위 | 추측 | 측정 |
|---|---|---|
| 563 | "다음은 engine runtime 연결" | `stack-pointer`가 진입 직후를 막고 있었음 |
| 564 | "다음 장애물은 moffs" | 맞았으나 **형태를 틀림** — `66` prefix 형태 |
| 565 | "flat guest면 prefix를 떼면 됨" | **base가 0이 아님** |

세 번 다 측정이 한 번에 답했고, 그때마다 census에 그것을 묻는 줄을 남겼습니다 — 정지
지점(563), 정지 바이트(565), selector base(566).

> 다음 세션은 추론으로 시작하지 않아도 된다.

### 검증

census만 바뀌었고 emitter와 probe는 그대로입니다.

| Host | 결과 |
|---|---|
| Linux x64 Release | census 실행, selector binding 4개 |
| Linux i386 Release | census 빌드 통과 |
| Win32 x86 Debug | census 빌드 통과 |

## English

### Result -- the guest is not a flat model

The segment bases are the relocated object bases, not zero.

### What that overturns

Finishing Task 565 I wrote that `ES` is not `FS`/`GS`, that long mode ignores the
`CS`/`DS`/`ES`/`SS` overrides with a zero base, and that under a flat guest dropping the
prefix might mean the same thing -- **something to check and decide, not to assert**.

Checked, and it does not. With non-zero bases, long mode ignoring the override is not a
convenience but **exactly the wrong answer**: `mov ebx, es:[0x5c]` means
`[ES_base + 0x5c]`, and dropping the prefix reads a different address without raising
anything. That is Task 550's "quietly a different instruction" shape again.

### So the x64 segment override is two transforms composed

i386 already does the right thing in `EmitSegmentOverrideSlot`: drop the prefix, widen
ModRM to a disp32 form, **fold the segment base into the displacement**, and guard on the
shadow selector still agreeing.

On x64 that result needs Task 552's and 564's memory-operand lowering on top -- a `0x67`,
or a SIB rewrite where the form has no base. **Composing two transforms, so its own unit.**

### Three guesses lost this stretch

| Unit | Guess | Measurement |
|---|---|---|
| 563 | "next is connecting the engine runtime" | `stack-pointer` was blocking right after the entry |
| 564 | "next obstruction is moffs" | right, but **the wrong shape** -- the `66`-prefixed form |
| 565 | "flat guest, so drop the prefix" | **the bases are not zero** |

Each time the measurement answered in one round, and each time the census kept a line that
asks it: where the chain stops (563), the bytes that stop it (565), the selector bases
(566).

> The next session does not have to start by reasoning.

### Verification

Only the census changed; the emitter and probes are untouched. It runs on Linux x64 and
builds on Linux i386 and Win32.
