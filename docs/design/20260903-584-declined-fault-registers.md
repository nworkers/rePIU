# 설계 20260903-584 — 거절된 폴트의 레지스터를 찍는다

## 목적

Task 583이 추정으로 남긴 것을 확정하거나 반증합니다.

> guest `0x010F18A4`의 `mov eax, [esi]`가 `0x200202`에서 폴트했다. 그 값은
> **어느 selector base도 더해지지 않은 것과 일관된다.**

Task 583의 작업 로그가 그 다음 일을 명시했습니다 — **`ESI`를 먼저 재라.**

## 왜 `ESI` 하나로 갈리는가

방출된 것은 `67 8b 06`, 즉 `mov eax, dword ptr [esi]`입니다. 변위도 index도
없습니다. long mode에서 DS base는 0이므로 CPU가 계산한 선형 주소는 **정확히
`ESI`**입니다.

| 관측 | 뜻 |
|---|---|
| `access == ESI` | 방출된 명령이 base 없이 주소를 만들었다 |
| `access != ESI` | 그 명령이 아니거나, 우리가 읽는 `ESI`가 그 시점의 것이 아니다 |

**이것은 두 갈래를 확정하는 관측이지, 원인을 확정하는 관측이 아닙니다.**
`access == ESI`가 나와도 남는 질문이 있습니다 — **`ESI`가 애초에 그 값이어야
했는가?** 그것은 i386과 나란히 재야 하고, 이 단위의 범위가 아닙니다.

이 구분을 미리 적어 두는 이유는 Task 580의 실수 때문입니다. 그 단위는 옳은
관찰에서 틀린 결론으로 넘어갔고, 이 세션에서 다섯 번 반증된 추정의 첫
번째였습니다.

## 설계 결정 1 — 새 도구가 아니라 Task 582의 줄을 넓힙니다

`[repiu-exit]`는 이미 "거절된 폴트에서 무슨 일이 있었나"를 찍는 자리이고,
`ThreadContext`와 `FaultEvent`를 손에 쥐고 있습니다. 레지스터는 같은 질문의
다음 항입니다.

그리고 이것은 이 한 번을 위한 것이 아닙니다. Task 578의 작업 로그가 적어 두었듯
`InstallHostCrashReporter`의 본문은 통째로 `#if defined(_WIN32)`이고 **Linux에는
아무것도 없으며 WSL에 gdb도 없습니다.** x64가 이제 전진하기 시작했으므로 "어디서
왜 멈췄는가"는 반복해서 물을 질문입니다.

## 설계 결정 2 — 접근 주소를 같은 줄에 넣습니다

지금은 `[repiu-fault]`(platform)와 `[repiu-exit]`(engine)를 나란히 읽어야
`access`와 레지스터를 맞춰 볼 수 있습니다. `FaultEvent::access.fault_address`가
엔진 쪽에도 있으므로 **한 줄에서 비교가 끝나게** 합니다.

`access.valid`가 거짓이면 접근 주소가 없는 폴트(breakpoint 등)이므로 그렇게
표시합니다. 0을 찍어 "주소 0에 접근했다"로 읽히게 두지 않습니다.

## 설계 결정 3 — x64에서 `DS`·`ES`·`SS`는 관측이 아닙니다

이것을 설계에 적는 이유는, 찍고 나면 반드시 오독될 값이기 때문입니다.

`guest_cpu_context.cpp`의 x64 로드 경로는 이렇게 되어 있습니다.

```c
registers->SegDs = 0U;
registers->SegEs = 0U;
registers->SegSs = 0U;
```

Linux의 `mcontext_t`는 `REG_CSGSFS`에 CS·GS·FS만 담고 DS·ES·SS는 저장하지
않습니다. 그러므로 **x64에서 그 세 값은 합성된 0이지 게스트의 상태가
아닙니다.** 지금 벽이 세그먼트 냄새를 풍기는 상황에서 이것을 적어 두지 않으면,
다음 사람이 `ds=0x0000`을 보고 "DS가 0이라서 그렇구나"라고 읽습니다.

그래서 세 값을 **찍지 않습니다.** 없는 것을 0으로 찍는 것보다 없는 것을 비워
두는 편이 정직합니다. 대신 platform이 실제로 주는 CS·FS·GS는 찍습니다.

이 사실 자체는 `docs/analysis/`에 남깁니다 — 이 단위가 발견한 제약이고, 세그먼트
축 작업이 반드시 다시 만날 것입니다.

## 설계 결정 4 — 관측만 하고 고치지 않습니다

Task 583이 수정 단위였고, 이 단위는 다시 관측입니다. 산출물은 **`access`와
`ESI`가 같은가에 대한 답 하나**입니다.

## 출력

```text
[repiu-exit] site=… eip=0x… code=0x… guest_stack=… call_state=… n=…
[repiu-regs] access=0x00200202 eax=0x… ecx=0x… edx=0x… ebx=0x…
             esp=0x… ebp=0x… esi=0x… edi=0x… eflags=0x… cs=0x… fs=0x… gs=0x…
```

두 줄로 나눕니다 — 한 줄에 넣으면 터미널에서 접혀 `esi=`가 어느 폴트의 것인지
읽기 어려워집니다. 같은 게이트, 같은 상한 아래 있습니다.

## 범위

- 수정: `src/engine/telemetry/fault_exit_trace.{h,cpp}`
- 신규: `docs/analysis/`에 x64 fault context의 세그먼트 제약 기록
- 열지 않음: 원인 수정, `ESI`가 옳은 값인가라는 후속 질문, `pumpit2a`의 미해결
  분기

## 검증

1. **추적이 꺼진 i386이 불변일 것** — `[repiu-exit]`·`[repiu-regs]` 0줄.
2. **x64가 `ESI`를 낼 것** — 그리고 `access`와 같은지 다른지가 이 단위의
   답입니다. **어느 쪽일지 예측하지 않습니다.**
3. 두 호스트 `repiu_core_probe`, Win32 회귀.

---

# Design 20260903-584 — Print the registers of a declined fault

## Purpose

Settle or refute what Task 583 left as an inference.

> `mov eax, [esi]` at guest `0x010F18A4` faulted on `0x200202`, a value
> **consistent with no selector base having been added.**

Task 583's work log named the next job explicitly: **measure `ESI` first.**

## Why one register decides it

What was emitted is `67 8b 06` — `mov eax, dword ptr [esi]`, with no
displacement and no index. DS's base is zero in long mode, so the linear address
the CPU computed is **exactly `ESI`**.

| Observation | Meaning |
|---|---|
| `access == ESI` | the emitted instruction formed its address with no base |
| `access != ESI` | it is not that instruction, or the `ESI` being read is not the one from that moment |

**This settles a fork, not a cause.** Even with `access == ESI` a question
remains — **should `ESI` have held that value at all?** Answering it means
reading i386 alongside, and that is not this unit's scope.

The distinction is written down in advance because of Task 580's mistake: that
unit stepped from a correct observation to a wrong conclusion, and it was the
first of five inferences refuted this session.

## Decision 1 — Widen Task 582's line rather than build a new tool

`[repiu-exit]` is already the place that reports what happened at a declined
fault, and it holds both the `ThreadContext` and the `FaultEvent`. The registers
are the next term of the same question.

And this is not for one use. As Task 578's work log recorded,
`InstallHostCrashReporter`'s body is entirely `#if defined(_WIN32)`; **Linux has
nothing, and there is no gdb in this WSL.** Now that x64 has started making
progress, "where and why did it stop" is a question that will be asked
repeatedly.

## Decision 2 — Put the access address on the same line

Today, matching an access address to registers means reading `[repiu-fault]`
(platform) and `[repiu-exit]` (engine) side by side.
`FaultEvent::access.fault_address` is available engine-side too, so **the
comparison should finish inside one line.**

When `access.valid` is false the fault carries no address (a breakpoint, say),
and it is printed as such. A zero is not printed where it would read as "it
accessed address zero".

## Decision 3 — On x64, `DS`, `ES` and `SS` are not observations

This goes in the design because these are values that will certainly be
misread once printed.

The x64 load path in `guest_cpu_context.cpp` reads:

```c
registers->SegDs = 0U;
registers->SegEs = 0U;
registers->SegSs = 0U;
```

Linux's `mcontext_t` packs only CS, GS and FS into `REG_CSGSFS` and saves no DS,
ES or SS. So on x64 **those three are synthesized zeros, not guest state.** With
the current wall smelling of segments, failing to record this invites the next
reader to see `ds=0x0000` and conclude "DS is zero, that explains it".

So the three are **not printed.** Leaving an absent value absent is more honest
than printing it as zero. CS, FS and GS, which the platform really provides, are
printed.

The fact itself is recorded under `docs/analysis/` — it is a constraint this
unit discovered, and any work on the segment axis will meet it again.

## Decision 4 — Observe only; repair nothing

Task 583 was a repair; this one is observation again. Its product is **one
answer: are `access` and `ESI` the same value?**

## Output

```text
[repiu-exit] site=… eip=0x… code=0x… guest_stack=… call_state=… n=…
[repiu-regs] access=0x00200202 eax=0x… ecx=0x… edx=0x… ebx=0x…
             esp=0x… ebp=0x… esi=0x… edi=0x… eflags=0x… cs=0x… fs=0x… gs=0x…
```

Two lines, because one would wrap in a terminal and make it hard to see which
fault an `esi=` belongs to. Both sit behind the same gate and the same limit.

## Scope

- Modified: `src/engine/telemetry/fault_exit_trace.{h,cpp}`
- New: a `docs/analysis/` record of the x64 fault-context segment constraint
- Not opened: repairing the cause; the follow-up question of whether `ESI` holds
  the right value; `pumpit2a`'s unresolved branch

## Verification

1. **i386 with the trace off is unchanged** — zero `[repiu-exit]` and
   `[repiu-regs]` lines.
2. **x64 produces `ESI`** — and whether it equals `access` is this unit's
   answer. **Which way it goes is not predicted here.**
3. `repiu_core_probe` on both hosts; the Win32 regression.
