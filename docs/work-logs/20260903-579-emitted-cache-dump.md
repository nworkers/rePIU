# 작업 기록 20260903-579 — 방출된 캐시 바이트 덤프

설계: [20260903-579](../design/20260903-579-emitted-cache-dump.md) ·
작업 지시: [20260903-579](../work-orders/20260903-579-emitted-cache-dump.md)

## 관측

```text
-- emitted cache window around 0x5 --
     cache=0x0 len=5  guest=0x10f16b0
        emitted: e9 00 00 00 00
  >> cache=0x5 len=1  guest=0x10f1728
        emitted: fb
     cache=0x6 len=4  guest=0x10f1729
        emitted: 41 83 e7 fc
     cache=0xa len=3  guest=0x10f172c
        emitted: 44 89 fb
```

**`sti`는 `INT3`이 아니라 그대로 복사되어 있습니다.** cache+5의 방출 바이트는
`fb` 한 바이트, 게스트 원본과 같습니다.

이어지는 두 항목이 emitter가 제대로 일하고 있음을 보입니다 — `83 e4 fc`
(`and esp,-4`)는 `41 83 e7 fc`(`and r15d,-4`)로, `89 e3`(`mov ebx,esp`)는
`44 89 fb`(`mov ebx,r15d`)로 재인코딩됐습니다. Task 574·577이 만든 `ESP`→`R15D`
경로가 실제 게스트 코드에서 동작합니다.

## 진단 — 크래시는 결함이 아니라 설계된 메커니즘입니다

`STI`는 CPL 3에서 **#GP**를 일으킵니다. Linux는 general protection fault를
`SIGSEGV`로, `si_addr`을 **0**으로 전달합니다.

```text
[repiu-fault] unhandled signal=0xb eip=0x20000005 access=0x0
```

`signal=0xb`(SIGSEGV)와 `access=0x0`이 정확히 그것입니다. **관측이 완전히
설명됩니다.**

그리고 이것은 사고가 아닙니다. `src/hle/privileged_instruction.cpp`가 opcode
`0xFB`를 `"STI"`로 다룹니다 — i386에서도 `sti`는 그대로 복사되어 캐시에서 실행되고,
#GP를 일으키고, fault handler가 그것을 잡아 HLE로 처리합니다. **폴트가 HLE가
제어를 얻는 방법입니다.**

따라서 x64의 문제는 "`sti`가 잘못 방출됐다"가 아니라 **"그 폴트를 handler가
서비스하지 못했다"**입니다.

## 정정 — Task 578의 추론은 네 번째 반증입니다

Task 578의 작업 로그는 이렇게 적었습니다.

> `sti`는 privileged이므로 분류기가 거절하고 long mode는 `INT3`을 놓아야 하며,
> 그렇다면 `SIGTRAP`(0x5)이 나와야 합니다.

**전제가 틀렸습니다.** 분류기는 `sti`를 거절하지 않고 `kIdenticalBytes`로
통과시킵니다. 그것이 옳은 동작입니다 — i386이 하는 일과 같고, 폴트가 곧 HLE
진입점이기 때문입니다.

Task 578은 그 불일치를 **확정하지 않고 다음 단위로 넘겼습니다.** 그 판단이
이번에 값을 했습니다. 추측을 결론으로 적었다면 "분류기가 `sti`를 거절하지 않는
버그"를 고치러 갔을 것이고, 그것은 i386에서 동작하는 메커니즘을 부수는 일이었을
것입니다.

이 세션에서 관측이 추론을 뒤집은 네 번째입니다 — 574의 SIB 기대값, 575의 주소
잘림, 577의 `Eip`, 그리고 이번 `sti`.

## 곁가지 관측 두 개

**진입 분기의 변위가 0입니다.** `cache=0x0`의 `e9 00 00 00 00`은 미해결이 아니라
**옳게 해결된 것**입니다. patch site는 offset 1이고 대상은 offset 5이므로
`rel32 = 5 - (1 + 4) = 0`입니다. 다음 명령으로 가는 것이 맞습니다.

**`pumpit2a`에는 미해결 분기가 하나 있습니다** — `branch edges emitted=9595
unresolved=1`. `pumpipx3`은 0입니다. 이번 단위에서 쫓지 않았고, 어느 분기인지도
아직 모릅니다.

## 검증

| 항목 | 결과 |
|---|---|
| 도구가 답을 냄 | `--cache 0x5`가 방출 바이트를 찍음 |
| 덤프가 emitter의 출력 | 같은 실행에서 `agrees=true` |
| 옵션 없는 census 불변 | emittable 73,748 · refused 585 · complete 15,646 · reachable 7,723 · `agrees=true` — 전부 Task 577과 동일 |
| 변경 범위 | `src/tools/instruction_census/main.cpp` 한 파일 |

probe 회귀는 돌리지 않았습니다. 바뀐 파일이 census 하나뿐이고 어떤 probe도 그
파일을 링크하지 않습니다.

## 남은 것

| # | 항목 | 상태 |
|---|---|---|
| 0~3 | 방출·심볼·`Esp`·guest entry | 해결 |
| 5 | validator의 i386 전제 | 미해결 (아직 도달 못 함) |
| 6 | cache+5의 SIGSEGV | **진단 완료 — `sti`의 #GP** |
| **7** | **x64 fault handler가 privileged 폴트를 서비스하지 못함** | **신규 — 다음** |

항목 7이 다음입니다. i386에는 동작하는 경로가 있으므로, 질문은 "무엇을 새로
만드는가"가 아니라 **"x64에서 그 경로의 어디가 끊기는가"**입니다.

## 아직 확인하지 않음

- handler가 왜 서비스하지 못했는지 재지 않았습니다. `IsAotCacheAddress`가
  `0x20000005`를 인식하는지, guest 주소 역매핑이 되는지, privileged 경로에
  닿는지 — 셋 다 확인되지 않았습니다.
- `--cache`는 x64 long-mode 이미지에만 써 봤습니다. i386 구성에서의 출력은
  보지 않았습니다.

---

# Work log 20260903-579 — Dumping the emitted cache bytes

Design: [20260903-579](../design/20260903-579-emitted-cache-dump.md) ·
work order: [20260903-579](../work-orders/20260903-579-emitted-cache-dump.md)

## The observation

```text
-- emitted cache window around 0x5 --
     cache=0x0 len=5  guest=0x10f16b0
        emitted: e9 00 00 00 00
  >> cache=0x5 len=1  guest=0x10f1728
        emitted: fb
     cache=0x6 len=4  guest=0x10f1729
        emitted: 41 83 e7 fc
     cache=0xa len=3  guest=0x10f172c
        emitted: 44 89 fb
```

**`sti` was copied verbatim, not turned into an `INT3`.** The emitted byte at
cache+5 is the single `fb`, identical to the guest's.

The two entries after it show the emitter working correctly: `83 e4 fc`
(`and esp,-4`) became `41 83 e7 fc` (`and r15d,-4`), and `89 e3`
(`mov ebx,esp`) became `44 89 fb` (`mov ebx,r15d`). The `ESP`→`R15D` path Tasks
574 and 577 built works on real guest code.

## Diagnosis — the crash is the designed mechanism, not a defect

`STI` raises **#GP** at CPL 3. Linux delivers a general protection fault as
`SIGSEGV` with `si_addr` of **0**.

```text
[repiu-fault] unhandled signal=0xb eip=0x20000005 access=0x0
```

`signal=0xb` and `access=0x0` are exactly that. **The observation is fully
explained.**

And it is not an accident. `src/hle/privileged_instruction.cpp` handles opcode
`0xFB` as `"STI"`: on i386 too, `sti` is copied verbatim, executed from the
cache, raises #GP, and the fault handler catches it and services it through the
HLE. **The fault is how the HLE gets control.**

So x64's problem is not "`sti` was emitted wrongly" but **"the handler did not
service that fault"**.

## Correction — Task 578's reasoning is the fourth refutation

Task 578's work log wrote:

> `sti` is privileged, so the classifier refuses it and long mode should plant an
> `INT3` — which would raise `SIGTRAP` (0x5).

**The premise was wrong.** The classifier does not refuse `sti`; it passes it as
`kIdenticalBytes`. That is the correct behaviour — the same as i386's, because
the fault is the HLE's entry point.

Task 578 **left that discrepancy unsettled and handed it on.** That judgement
paid here. Had the guess been written down as a conclusion, the next step would
have been to "fix" the classifier for not refusing `sti` — which would have
broken a mechanism that works on i386.

This is the fourth time this session that observation overturned reasoning: Task
574's expected SIB byte, Task 575's address truncation, Task 577's `Eip`, and now
`sti`.

## Two side observations

**The entry branch's displacement is zero.** The `e9 00 00 00 00` at `cache=0x0`
is not unresolved but **correctly resolved**: the patch site is at offset 1 and
the target is offset 5, so `rel32 = 5 - (1 + 4) = 0`. Falling into the next
instruction is the right answer.

**`pumpit2a` has one unresolved branch** — `branch edges emitted=9595
unresolved=1`, where `pumpipx3` has none. Not chased here, and which branch it is
remains unknown.

## Verification

| Item | Result |
|---|---|
| The tool answers | `--cache 0x5` prints the emitted bytes |
| The dump is the emitter's output | `agrees=true` in the same run |
| Census unchanged without the option | emittable 73,748 · refused 585 · complete 15,646 · reachable 7,723 · `agrees=true` — all identical to Task 577 |
| Change scope | one file, `src/tools/instruction_census/main.cpp` |

The probe regressions were not run: the only changed file is the census, and no
probe links it.

## What is left

| # | Item | Status |
|---|---|---|
| 0–3 | emission / symbols / `Esp` / guest entry | done |
| 5 | The validator's i386 assumption | open (not yet reached) |
| 6 | The SIGSEGV at cache+5 | **diagnosed — `sti`'s #GP** |
| **7** | **The x64 fault handler does not service the privileged fault** | **new — next** |

Item 7 is next. i386 has a working path, so the question is not "what must be
built" but **"where does that path break on x64"**.

## Not yet verified

- Why the handler did not service it was not measured. Whether
  `IsAotCacheAddress` recognises `0x20000005`, whether the reverse guest mapping
  succeeds, and whether the privileged path is reached at all are three separate
  unknowns.
- `--cache` has only been used against an x64 long-mode image. Its output under
  an i386 configuration has not been looked at.
