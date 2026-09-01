# 20260901-560 x64 direct branch emission 작업 로그

## 한국어

### 결과

`kDirectJump`와 `kConditionalBranch`를 long-mode emission에서 방출합니다.

| 항목 | 559 | 560 |
|---|---:|---:|
| copied | 19,187 (32.03%) | 19,187 (32.03%) |
| lowered | 20,456 (34.15%) | 20,456 (34.15%) |
| **branches** | — | **6,847 (11.43%)** |
| **방출 가능** | 39,643 (66.17%) | **46,490 (77.60%)** |
| **완결 block** | 381 (2.66%) | **5,482 (38.32%)** |
| non-copy 거부 | 12,856 | 6,009 |

**명령 수는 11.4%p 늘었는데 완결 block은 14.4배가 됐습니다.** block은 terminator 하나만
있으면 완결되기 때문입니다 — 이 두 수를 함께 읽어야 한다는 Task 556의 지적이 반대
방향으로도 성립합니다.

### 측정이 순서를 정했습니다

작업 전에 `not-a-copy-record` 12,856을 plan kind로 나눴습니다. **예측은 "block
terminator가 지배적"이었고, 맞았습니다 — 12,174 / 12,856 (94.7%).**

| kind | 이전 | 이후 |
|---|---:|---:|
| `kConditionalBranch` | 5,202 | **18** |
| `kDirectJump` | 1,663 | **0** |
| `kDirectCall` | 4,204 | 4,204 |
| `kReturn` | 1,105 | 1,105 |

남은 18개는 `ReadConditionOpcode`가 철자를 모르는 조건입니다. i386에서도 INT3이므로
새 항목이 아닙니다.

### 미해결 edge는 0입니다

```text
branch edges        emitted=6847 unresolved=0
```

6,847개 edge가 전부 cache 안에서 해결됐습니다. **따라서 완결 block 38.32%는 미해결
edge로 부풀려진 수치가 아닙니다.**

다만 이는 cache 밖 target을 boundary로 돌리는 경로가 **이 이미지에서는 한 번도 실행되지
않았다**는 뜻이기도 합니다. 실제 이미지로는 검증되지 않은 안전망입니다 — 아래 probe가
우연히 그것을 실행시켰다는 점만이 유일한 확인입니다.

### 실행으로 확인했고, 첫 실행이 틀린 것을 잡았습니다

block 세 개짜리 이미지를 만들어 조건만 바꿔 두 번 호출했습니다.

```text
branch_taken_eax       observed=0x1111 expected=0x1111
branch_fallthrough_eax observed=0x2222 expected=0x2222
```

한 방향만 봤다면 "분기하지 않는 `jz`"와 "분기를 아예 빼먹은 emitter"를 구분하지 못했을
것입니다.

**첫 실행은 실패했고, 그것이 유용했습니다.** `unresolved=1`이 나왔는데 원인은 emitter가
아니라 **probe가 만든 프로그램**이었습니다 — join block이 어떤 block도 소유하지 않는
주소로 fallthrough 하고 있었고, emitter는 그 edge를 규정대로 boundary로 돌린 것입니다.
`MakePlan`이 하는 대로 닫는 return을 붙여 고쳤습니다.

> 안전망이 처음 울린 곳은 안전망이 틀린 곳이 아니었습니다.

### 검증

| Host | 결과 |
|---|---|
| Linux x64 Release | `core_probe_all=true`, 20/20, skipped 2 |
| Linux i386 Release | `core_probe_all=true`, 19/19, skipped 3 |
| Win32 x86 Debug | `core_probe_all=true`, 19/19, skipped 3 |

i386 경로는 한 줄도 바뀌지 않습니다. 새 방출은 전부
`if (options.enable_long_mode_emission)` 안에 있고, fixup 쪽 분기도 같은 조건으로
막혀 있습니다.

census의 `agrees=true`가 유지됩니다 — 새 branch 카운터까지 emitter와 일치합니다.

### 다음

남은 non-copy 6,009 중 `kDirectCall` 4,204(69.96%)와 `kReturn` 1,105(18.39%)가 88%입니다.
`kDirectCall`은 guest stack에 guest 주소를 push해야 하고(Task 559의 시퀀스가 이미 있음),
`kReturn`은 guest 주소를 cache 주소로 바꾸는 **dispatch resolver**를 요구합니다.

## English

### Result

`kDirectJump` and `kConditionalBranch` are emitted under long-mode emission.

| Item | 559 | 560 |
|---|---:|---:|
| copied | 19,187 (32.03%) | 19,187 (32.03%) |
| lowered | 20,456 (34.15%) | 20,456 (34.15%) |
| **branches** | — | **6,847 (11.43%)** |
| **Emittable** | 39,643 (66.17%) | **46,490 (77.60%)** |
| **Complete blocks** | 381 (2.66%) | **5,482 (38.32%)** |
| non-copy refusals | 12,856 | 6,009 |

**Instructions rose by 11.4 points and complete blocks went up 14.4-fold**, because a
block needs only its terminator to be complete. Task 556's point that the two numbers have
to be read together holds in this direction too.

### The measurement set the order

Before the work, `not-a-copy-record`'s 12,856 was split by plan kind. **The prediction was
"block terminators dominate", and it held: 12,174 of 12,856 (94.7%).**

| Kind | Before | After |
|---|---:|---:|
| `kConditionalBranch` | 5,202 | **18** |
| `kDirectJump` | 1,663 | **0** |
| `kDirectCall` | 4,204 | 4,204 |
| `kReturn` | 1,105 | 1,105 |

The remaining 18 are conditions `ReadConditionOpcode` cannot spell. They are INT3 on i386
too, so they are not new.

### No edge went unresolved

```text
branch edges        emitted=6847 unresolved=0
```

All 6,847 resolved inside the cache, so **the 38.32% is not inflated by edges that became
boundaries.**

It also means the path that turns an out-of-cache target into a boundary **never ran on
this image**. It is a safety net this measurement did not exercise -- the probe below
happening to trip it is the only thing that has.

### Confirmed by execution, and the first run caught a wrong one

A three-block image, called twice with only the condition changed:

```text
branch_taken_eax       observed=0x1111 expected=0x1111
branch_fallthrough_eax observed=0x2222 expected=0x2222
```

Checking one direction would not have separated "a `jz` that never jumps" from "an emitter
that dropped the branch".

**The first run failed, and that was useful.** It reported `unresolved=1`, and the cause
was not the emitter but **the program the probe built**: its join block fell through to an
address no block owned, and the emitter turned that edge into a boundary exactly as it
should. Closing the block with a return, the way `MakePlan` does, fixed it.

> Where the safety net first fired was not where the safety net was wrong.

### Verification

| Host | Result |
|---|---|
| Linux x64 Release | `core_probe_all=true`, 20 of 20, 2 skipped |
| Linux i386 Release | `core_probe_all=true`, 19 of 19, 3 skipped |
| Win32 x86 Debug | `core_probe_all=true`, 19 of 19, 3 skipped |

The i386 path does not change by a line: every new emission sits inside
`if (options.enable_long_mode_emission)`, and the fixup branch is fenced by the same
condition.

The census still reports `agrees=true`, now including the new branch counter.

### Next

Of the remaining 6,009 non-copy records, `kDirectCall` (4,204, 69.96%) and `kReturn`
(1,105, 18.39%) are 88%. `kDirectCall` must push a guest address onto the guest stack --
Task 559's sequences already do that -- and `kReturn` needs the **dispatch resolver** that
turns a guest address into a cache address.
