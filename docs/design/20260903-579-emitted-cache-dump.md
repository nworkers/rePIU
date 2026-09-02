# 설계 20260903-579 — 방출된 캐시 바이트 덤프

## 목적

Task 578이 남긴 관측 하나를 확정합니다.

```text
[repiu-fault] unhandled signal=0xb eip=0x20000005 access=0x0
```

x64 실행이 cache 진입 5바이트 지점에서 `SIGSEGV`로 멈췄습니다. **그 자리에 무엇이
방출되어 있는지 볼 수단이 지금 없습니다.**

## 왜 추론으로 끝내지 않는가

Task 578의 작업 로그는 여기까지 추론했습니다 — 게스트 entry는 `eb 76` 하나짜리
block이고 long mode에서 `E9 rel32` 5바이트가 되므로 `0x20000005`는 두 번째 방출
block의 시작이며, 점프 대상 `0x010F1728`은 `sti`로 시작하고, `sti`는 privileged
이므로 `INT3`이 놓여야 하고 그러면 `SIGTRAP`이어야 한다.

**관측은 `SIGSEGV`였습니다.** 추론의 어딘가가 틀렸고, 어디인지는 바이트를 봐야
알 수 있습니다. 후보가 여럿입니다.

- block 방출 순서가 plan의 worklist 순서이고, 그것은 스택이므로 두 번째 방출
  block이 점프 대상이 아닐 수 있습니다.
- `sti`가 `kHleBoundary`가 아닌 다른 kind일 수 있습니다.
- `E9 rel32`가 5바이트가 아닐 수 있습니다.

이 세션에서 추측을 결론으로 적은 것이 **세 번 반증됐습니다** — Task 574의 SIB
기대값, Task 575의 주소 잘림, Task 577의 `Eip`. 세 번 다 바이트나 로그를 실제로
본 뒤에 답이 바뀌었습니다. 네 번째를 만들지 않습니다.

## 설계 결정 1 — 도구는 census에 붙입니다

census는 이미 long-mode 이미지를 만들고 있고(`enable_long_mode_emission = true`),
plan과 address map을 모두 손에 쥐고 있습니다. 새 도구를 만드는 대신 옵션을
더합니다.

로더 쪽에 로그를 더하지 않는 이유는 두 가지입니다. 로더의 캐시는 실행 중에만
존재하므로 문제를 재현해야 볼 수 있고, census는 같은 이미지를 **실행 없이**
만듭니다. 그리고 census는 이미 `agrees=` 로 emitter와의 일치를 지키고 있어,
덤프가 emitter의 실제 출력이라는 것이 그 줄로 보장됩니다.

## 설계 결정 2 — 질문 방향은 cache offset에서 guest로입니다

fault가 주는 것은 **cache 주소**입니다. 그래서 기본 조회는 `cache offset →
어느 명령인가`이고, 그 반대가 아닙니다.

`--cache <offset>`은 그 offset을 덮는 address-map 항목을 찾아 주변 창을 함께
찍습니다. 한 항목만으로는 부족합니다 — 앞 항목이 무엇으로 끝났는지가 "왜 그리로
갔는가"의 절반이기 때문입니다.

각 항목에 대해 찍는 것:

| 열 | 이유 |
|---|---|
| cache offset · emitted length | fault 주소와 맞춰 보기 위해 |
| guest address · kind | 무엇이었는지 |
| guest bytes | 원본 |
| emitted bytes | **실제로 방출된 것** — 이 단위가 존재하는 이유 |

## 설계 결정 3 — 진단만 하고 고치지 않습니다

이 단위는 **도구와 관측**입니다. 바이트를 본 결과가 무엇이든, 고치는 것은 다음
단위입니다.

이유는 Task 578과 같습니다 — 도구와 수정을 같은 단위에 넣으면 "무엇이 무엇을
움직였는가"를 말할 수 없게 됩니다. 그리고 이 도구는 한 번 쓰고 버릴 것이 아니라,
x64가 실행되기 시작한 지금부터 반복해서 필요합니다.

## 범위

- 신규: census의 `--cache <offset>` 옵션
- 열지 않음: 발견될 결함의 수정, 로더 쪽 계측, 3.20 항목 5

## 검증

1. **도구가 답을 낼 것** — `--cache 0x5`가 그 자리의 명령과 방출 바이트를
   찍어야 합니다.
2. **덤프가 emitter의 출력일 것** — 같은 실행에서 `agrees=true`가 유지되어야
   합니다. 덤프가 다른 이미지의 것이라면 답이 아닙니다.
3. **관측 기록** — `pumpit2a`의 `PIU.EXE`로 실행해 cache+5에 무엇이 있는지,
   그리고 Task 578의 추론 중 무엇이 틀렸는지 기록합니다.
4. **회귀** — census의 기존 숫자가 바뀌지 않아야 합니다. 옵션을 주지 않은 실행은
   이전과 같아야 합니다.

---

# Design 20260903-579 — Dumping the emitted cache bytes

## Objective

Settle the observation Task 578 left standing:

```text
[repiu-fault] unhandled signal=0xb eip=0x20000005 access=0x0
```

The x64 run stopped with `SIGSEGV` five bytes into the cache. **Nothing today can
show what is emitted there.**

## Why not finish this by reasoning

Task 578's work log reasoned this far: the guest entry is a one-instruction block
`eb 76`, long mode emits it as a 5-byte `E9 rel32`, so `0x20000005` is the start
of the second emitted block; its target `0x010F1728` begins with `sti`; `sti` is
privileged, so an `INT3` should be planted and the fault should be `SIGTRAP`.

**The observation was `SIGSEGV`.** Something in that chain is wrong, and which
link can only be seen in the bytes. There are several candidates:

- block emission follows the plan's worklist, which is a stack, so the second
  block emitted need not be the jump target;
- `sti` may carry a kind other than `kHleBoundary`;
- the `E9 rel32` may not be five bytes.

Writing a guess down as a conclusion has been **refuted three times this
session** — Task 574's expected SIB byte, Task 575's address truncation, Task
577's `Eip`. Each time the answer changed after the bytes or the log were
actually looked at. There will not be a fourth.

## Decision 1 — the tool goes in the census

The census already builds a long-mode image
(`enable_long_mode_emission = true`) and already holds both the plan and the
address map. Rather than a new tool, it gains an option.

Not the loader, for two reasons. The loader's cache exists only while it runs, so
seeing it means reproducing the problem; the census builds the same image
**without executing anything**. And the census already keeps `agrees=` against
the emitter's own counters, so that line is what guarantees the dump is the
emitter's real output.

## Decision 2 — the question runs from cache offset to guest

What a fault hands over is a **cache address**. So the primary lookup is
`cache offset → which instruction`, not the reverse.

`--cache <offset>` finds the address-map entry covering that offset and prints a
window around it. One entry is not enough: what the preceding entry ended with is
half of "why did control arrive there".

Per entry:

| Column | Why |
|---|---|
| cache offset · emitted length | to line up against the fault address |
| guest address · kind | what it was |
| guest bytes | the original |
| emitted bytes | **what was actually produced** — the reason this unit exists |

## Decision 3 — diagnose only; do not fix

This unit is **the instrument and the observation**. Whatever the bytes show,
fixing it is the next unit.

The reason is Task 578's: an instrument and a fix in one unit make it impossible
to say which moved what. And this instrument is not single-use — now that x64
executes, it will be wanted repeatedly.

## Scope

- New: the census's `--cache <offset>` option.
- Not opened: fixing whatever is found, loader-side instrumentation, and 3.20's
  item 5.

## Verification

1. **The tool answers** — `--cache 0x5` must print the instruction there and its
   emitted bytes.
2. **The dump is the emitter's output** — `agrees=true` must still hold in the
   same run. A dump of some other image is not an answer.
3. **Record the observation** — run it on `pumpit2a`'s `PIU.EXE`, record what
   sits at cache+5, and record which link of Task 578's reasoning was wrong.
4. **Regression** — the census's existing numbers must not change; a run without
   the option must be identical to before.
