# 작업 기록 20260903-573 — Linux x64 indirect call slot

설계: [20260903-573](../design/20260903-573-linux-x64-indirect-call.md) ·
작업 지시: [20260903-573](../work-orders/20260903-573-linux-x64-indirect-call.md)

## 먼저 잰 것 — kind는 unit을 고를 근거가 아니다

Task 572의 정지 표는 `kIndirectExit 60`이라고만 말했습니다. 그것으로는 무엇을
만들지 정할 수 없습니다 — indirect **call**과 indirect **jump**는 같은 slot을 쓰지만
walk를 완전히 다른 곳에 남깁니다. Task 557이 `kCopy` 행에 이유를 붙인 것과 같은
이유로 형식을 붙여 다시 쟀고, **그 측정이 이 unit의 설계를 바꿨습니다.**

| 형식 | 건수 |
|---|---:|
| `call-base+disp` / return-in-plan | **34** |
| `call-base+disp` / return-absent | 20 |
| `call-abs32` / return-absent | 4 |
| `call-sib` / return-absent | 2 |

두 가지가 드러났습니다. 60건 전부가 `FF /2` indirect call이고 jump는 하나도
없다는 것, 그리고 34건은 return 지점에 이미 plan block이 있다는 것입니다.

두 번째를 재기 전에는 이 unit을 할지가 불확실했습니다. planner는 indirect exit에서
block을 끝내고 다음 주소를 push하지 않으므로, slot을 내도 return 지점 block이 없으면
정지가 "edge outside the plan"으로 **이름만 바뀝니다.** 34건이 실제로 이어진다는
것을 확인한 뒤에 설계했습니다.

## 구현 — 새 기계가 없다

indirect call은 기존 세 조각의 결합입니다.

```text
67 44 8B 34 25 <disp32>   mov r14d, [abs32]      target  (Task 572의 주소 재작성)
45 8D 7F FC               lea r15d, [r15-4]      push    (Task 559의 stack sequence)
41 C7 07 <return32>       mov [r15], <return>
49 BC <thunk64>           movabs r12, thunk      transfer (Task 562의 thunk)
41 FF E4                  jmp r12
```

Task 562의 thunk 계약은 "R14D에 guest 주소가 있고 R15D가 guest ESP이니 그 주소로
가라"입니다. **return에 고유한 것이 하나도 없습니다.** 이름이 `ReturnThunk`인 것은
누가 만들었는지를 기록할 뿐입니다.

### 주소 재작성을 손으로 쓰지 않았다

target을 읽는 명령은 guest의 `FF /2`와 같은 memory operand를 가져야 합니다. 그
operand를 long mode로 옮기는 일은 `LowerLongModeBytes`가 이미 하고, **Task 572가
방금 넓힌 바로 그 경로**입니다.

그래서 guest bytes에서 opcode `FF`를 `8B`로, ModRM의 `reg`를 `010`에서 `110`으로
바꾼 32-bit 명령을 합성해 lowering에 넘깁니다. 32-bit에서 그것은
`mov esi, <operand>`이고, `110`은 R14의 하위 3비트이므로 **REX.R 한 바이트**를
opcode 앞에 끼우면 R14D가 됩니다.

합성한 명령에는 prefix가 없으므로 lowering 출력은 반드시 `67 8B`로 시작합니다. 그
두 바이트를 단언하고 아니면 거절합니다 — 삽입 위치를 추측하지 않기 위한
조건입니다.

## 순서 — 이것이 이 unit의 진짜 위험이었다

x86의 `CALL r/m32`은 **target을 먼저 계산하고 그다음에 push합니다.** 순서를
뒤집으면 ESP 상대 operand가 다른 위치를 읽고, operand가 push가 덮는 word를
가리키면 다른 값을 읽습니다.

그리고 load가 먼저이므로 **push sequence가 R14D를 건드리면 안 됩니다.** 현재
`PUSH imm32`는 `lea r15d,[r15-4]`와 `mov [r15], imm32` 두 개뿐이라 안전하지만,
같은 파일의 Task 559 `PUSHFD`는 R14D를 scratch로 씁니다. 즉 이것은 slot이 기대는
전제이지 slot이 보장하는 성질이 아니고, 바뀌면 **조용히** 틀립니다.

probe가 두 가지를 한 값으로 고정합니다. operand를 push가 덮을 word에 두고 거기에
callee 주소를 심어 둡니다. 순서가 옳으면 resolver의 **첫 질문**이 callee이고,
뒤집혔으면 그 word가 이미 return 주소이므로 첫 질문이 `after_call`이 됩니다.
push가 R14D를 파괴해도 첫 질문이 달라집니다.

`ResolverContext`에 `first_asked`를 더한 이유가 이것입니다. round trip에서
**마지막** 질문은 어느 순서든 return 지점이므로, 첫 질문만이 둘을 구분합니다.

## 검증

### Linux x64 Release — 확인됨

`repiu_core_probe`: 20/20, failures 0, skipped 2.

```text
indirect_first_asked      observed=0x140020    expected=0x140020
indirect_resolver_calls   observed=0x2         expected=0x2
indirect_resumed          observed=0x4444      expected=0x4444
indirect_esp_balanced     observed=0x20001800  expected=0x20001800
indirect_return_address   observed=0x14000b    expected=0x14000b
guest_indirect_call=true indcalls=1
guest_indirect_call_refusals=true,memory=1,register=1,jump=1,esp=1
```

`indirect_resumed=0x4444`는 call이 가고, callee가 돌고, return이 돌아온 **전체
왕복**입니다. `0x3333`이었다면 callee에서 멈춘 것이고 `0x1111`이었다면 call이 아예
가지 않은 것입니다.

거절 probe는 admit 하나와 refuse 셋을 함께 봅니다. memory 형식이 통과하지 않으면
나머지 세 거절은 아무것도 증명하지 못하므로, `memory=1`이 나머지를 의미 있게
만듭니다.

### census — 확인됨

| 항목 | Task 572 | Task 573 |
|---|---:|---:|
| indirect calls | 0 | **100** |
| emittable | 73,589 (99.00%) | **73,689 (99.13%)** |
| refused | 744 | **644** |
| complete block | 15,525 (89.39%) | **15,614 (89.91%)** |
| 도달 가능 block | 7,404 (42.63%) | **7,462 (42.97%)** |
| reachable instrs | 31,770 | **32,055** |
| **edge outside the plan** | **0** | **24** |
| first stop | `0x1101370` | `0x1101370` (불변) |

`agrees=true`입니다.

정지 표에서 `kIndirectExit` 60건 중 **58건이 사라졌습니다.** 남은 2건은
`call-sib/return-absent`, 즉 설계 결정 4가 일부러 닫아 둔 ESP 기반 SIB 형식입니다.
거절이 의도대로 동작하고 있다는 확인입니다.

### 설계의 예측 하나가 틀렸습니다

설계는 `edge outside the plan`이 **26**이 될 것이라고 적었습니다(return-absent
20 + 4 + 2). 실제는 **24**입니다.

빠진 것은 `call-sib` 2건이 애초에 거절되어 slot을 받지 못하므로 fallthrough를
push할 일도 없다는 것입니다. 즉 20 + 4 = 24가 맞고, 설계가 자기 결정 4를 예측에
반영하지 않았습니다. 숫자는 맞았고 예측이 틀렸습니다.

### walk의 fallthrough — 오늘은 아무것도 바꾸지 않습니다

census walk는 완결 block의 tail이 이름 붙은 네 kind가 아니면 `default:`에서
`guest_address + length`를 push합니다. `kIndirectExit` block은 지금까지 완결된 적이
없어 이 경로가 **한 번도 실행되지 않았습니다.**

slot이 생기는 순간 실행됩니다. indirect call에게 그 주소는 return 지점이므로
옳지만, indirect jump에게는 fallthrough가 존재하지 않습니다. 그래서
`kIndirectExit` tail은 `FF /2`일 때만 push하도록 했습니다.

**오늘 이 조건은 아무것도 바꾸지 않습니다** — 정지가 전부 call이기 때문입니다.
그것이 목적이었습니다. 나중에 jump slot이 생길 때 이미 자리에 있고, 지금
아무것도 바꾸지 않는다는 것이 확인된 상태입니다.

### 회귀 — 확인됨

- **Linux i386 Release `repiu_core_probe`**: 19/19, failures 0, skipped 3.
- **Win32 x86 Debug `repiu_aot_probe`**: 아래 표 참조.

i386 emitter는 건드리지 않았지만 `aot_code_cache.h`의 image 구조와 census가
바뀌므로 두 축 모두 확인했습니다.

## 남은 것

정지 표의 다음 항목은 `kCopy stack-pointer/lowering-declined` 17건,
`kCopy invalid-in-long-mode` 16건, `kCopy stack-pointer` 12건, `kJumpTable` 12건
입니다. first stop은 `0x1101370`의 `06` = `PUSH ES`로 Task 572 이후 그대로이고,
이것은 `invalid-in-long-mode` 부류입니다.

`edge outside the plan` 24건은 **emitter가 아니라 planner의 구멍**입니다. planner가
indirect call 뒤를 잇지 않기 때문이고, i386에서는 dynamic append가 실행 중에
메우는 자리입니다. 정적 census를 더 밀려면 그쪽을 봐야 합니다.

---

# Work log 20260903-573 — Linux x64 indirect call slot

Design: [20260903-573](../design/20260903-573-linux-x64-indirect-call.md) ·
work order: [20260903-573](../work-orders/20260903-573-linux-x64-indirect-call.md)

## Measured first — a kind is not something to choose a unit from

Task 572's stop table said only `kIndirectExit 60`, and that cannot decide what
to build: an indirect **call** and an indirect **jump** use the same slot but
leave the walk in completely different places. For the same reason Task 557
attached reasons to the `kCopy` rows, the form was attached and the measurement
repeated — and **that measurement changed this unit's design.**

| Form | Count |
|---|---:|
| `call-base+disp` / return-in-plan | **34** |
| `call-base+disp` / return-absent | 20 |
| `call-abs32` / return-absent | 4 |
| `call-sib` / return-absent | 2 |

Two things emerged: all 60 are `FF /2` indirect calls with not one jump among
them, and 34 already have a plan block at their return site.

Before measuring the second, it was unclear whether this unit was worth doing.
The planner ends a block at an indirect exit and does not push the following
address, so a slot with no return-site block only **changes the stop's name** to
"edge outside the plan". The design was written after confirming 34 continue for
real.

## Implementation — no new machinery

An indirect call is a composition of three existing pieces.

```text
67 44 8B 34 25 <disp32>   mov r14d, [abs32]     target   (Task 572's rewrite)
45 8D 7F FC               lea r15d, [r15-4]     push     (Task 559's sequence)
41 C7 07 <return32>       mov [r15], <return>
49 BC <thunk64>           movabs r12, thunk     transfer (Task 562's thunk)
41 FF E4                  jmp r12
```

Task 562's thunk contract is "R14D holds a guest address and R15D is guest ESP;
go there". **Nothing in it is specific to returns.** The name `ReturnThunk`
records who built it, not who may use it.

### The address rewrite was not written a second time

The instruction reading the target must carry the same memory operand as the
guest's `FF /2`, and moving that operand into long mode is what
`LowerLongModeBytes` already does — **on the very path Task 572 just widened.**

So a 32-bit instruction is synthesised from the guest bytes by changing opcode
`FF` to `8B` and ModRM's `reg` from `010` to `110`, and handed to the lowering.
In 32-bit terms that is `mov esi, <operand>`, and because `110` is R14's low
three bits, **one REX.R byte** before the opcode makes it R14D.

The synthesised instruction carries no prefixes, so the lowering's output must
begin `67 8B`. Those bytes are asserted and anything else refused — the
condition that keeps the insertion point from being a guess.

## The order — this unit's real hazard

x86's `CALL r/m32` **computes the target before pushing.** Reversed, an
ESP-relative operand addresses a different place, and an operand pointing at the
word the push overwrites reads a different value.

And because the load comes first, **the push sequence must not touch R14D.**
Today `PUSH imm32` is just `lea r15d,[r15-4]` and `mov [r15], imm32`, but Task
559's `PUSHFD` in the same file does use R14D as scratch. This is a premise the
slot rests on rather than a property it guarantees, and if it changed the slot
would be **silently** wrong.

The probe pins both with one value. The operand is placed at the word the push
will overwrite, seeded with the callee's address. In the right order the
resolver's **first** question is the callee; reversed, that word already holds
the return address and the first question is `after_call`. A push that destroyed
R14D changes the first question too.

That is why `first_asked` was added to `ResolverContext`: in a round trip the
**last** question is the return site in either order, so only the first can tell
them apart.

## Verification

### Linux x64 Release — confirmed

`repiu_core_probe`: 20/20, 0 failures, 2 skipped.

```text
indirect_first_asked      observed=0x140020    expected=0x140020
indirect_resolver_calls   observed=0x2         expected=0x2
indirect_resumed          observed=0x4444      expected=0x4444
indirect_esp_balanced     observed=0x20001800  expected=0x20001800
indirect_return_address   observed=0x14000b    expected=0x14000b
guest_indirect_call=true indcalls=1
guest_indirect_call_refusals=true,memory=1,register=1,jump=1,esp=1
```

`indirect_resumed=0x4444` is the **whole round trip**: the call went, the callee
ran, the return came back. `0x3333` would mean control stopped at the callee and
`0x1111` that the call never went at all.

The refusal probe checks one admission alongside three refusals. Without
`memory=1` the three refusals would prove nothing — everything would be refused.

### Census — confirmed

| Item | Task 572 | Task 573 |
|---|---:|---:|
| Indirect calls | 0 | **100** |
| Emittable | 73,589 (99.00%) | **73,689 (99.13%)** |
| Refused | 744 | **644** |
| Complete blocks | 15,525 (89.39%) | **15,614 (89.91%)** |
| Reachable blocks | 7,404 (42.63%) | **7,462 (42.97%)** |
| Reachable instructions | 31,770 | **32,055** |
| **Edge outside the plan** | **0** | **24** |
| First stop | `0x1101370` | `0x1101370` (unchanged) |

`agrees=true`.

**Fifty-eight of the 60 `kIndirectExit` stops are gone.** The two that remain are
`call-sib/return-absent` — the ESP-based SIB form design decision 4 deliberately
left closed. That is the refusal doing what it said it would.

### One of the design's predictions was wrong

The design wrote that `edge outside the plan` would become **26** (the
return-absent 20 + 4 + 2). It is **24**.

What it missed is that the 2 `call-sib` records are refused in the first place,
so they never get a slot and never push a fallthrough. 20 + 4 = 24 is right; the
design failed to carry its own decision 4 into its prediction. The number was
correct and the prediction was not.

### The walk's fallthrough — it changes nothing today

The census walk pushes `guest_address + length` from its `default:` case for any
complete block whose tail is not one of the four named kinds. A `kIndirectExit`
block has never been complete, so that path had **never once run**.

It runs the moment a slot exists. For an indirect call the address is the return
site and is right; for an indirect jump there is no fallthrough at all. So a
`kIndirectExit` tail now pushes only when it is `FF /2`.

**Today that condition changes nothing** — every stop is a call. That was the
point: it is in place before a jump slot arrives, with the fact that it changes
nothing now confirmed rather than assumed.

### Regression — confirmed

- **Linux i386 Release `repiu_core_probe`**: 19/19, 0 failures, 3 skipped.
- **Win32 x86 Debug `repiu_aot_probe`**: see the table above.

The i386 emitter is untouched, but the image structure in `aot_code_cache.h` and
the census changed, so both axes were checked.

## What is left

The next rows are `kCopy stack-pointer/lowering-declined` at 17,
`kCopy invalid-in-long-mode` at 16, `kCopy stack-pointer` at 12, and
`kJumpTable` at 12. The first stop is still `0x1101370`, `06` (`PUSH ES`),
unchanged since Task 572 and part of the `invalid-in-long-mode` family.

The 24 `edge outside the plan` are **a hole in the planner, not the emitter**:
the planner does not continue past an indirect call, and on i386 dynamic append
fills that in at run time. Pushing the static census further means looking
there.
