# 작업 기록 20260903-582 — x64는 첫 handler에 닿기도 전에 나간다

설계: [20260903-582](../design/20260903-582-fault-exit-attribution.md) ·
작업 지시: [20260903-582](../work-orders/20260903-582-fault-exit-attribution.md)

## 답 — `guest-stack-not-entered`

```text
x64   [repiu-watch] event=fault guest=0x010F1728 n=1 at=0x20000005
      [repiu-exit]  site=guest-stack-not-entered eip=0x20000005 code=0x0000000B
                    guest_stack=1 call_state=0 n=1
      [repiu-fault] unhandled signal=0xb eip=0x20000005 access=0x0
```

설계가 적어 둔 세 후보 중 **두 번째**입니다.

```c
if (context->use_guest_stack &&
    (context->active_call_state == nullptr ||
     context->active_call_state->host_esp == 0))
{
    return repiu::platform::FaultDisposition::kNotHandled;
}
```

`guest_stack=1 call_state=0`이므로 첫 항이 참, 둘째 항의 왼쪽이 참입니다.
그래서 `DispatchGuestFault`는 **AOT transfer 블록도, single-step 처리도, HLE
chain도 시작하기 전에** 반환합니다.

Task 581이 찾은 `AotHleTranslationScope`는 실행되지 않습니다. 고장 나서가 아니라
**도달하지 못해서**입니다.

## 설계 결정 4가 값을 했습니다

`site=` 이름만으로도 후보 3은 갈렸겠지만, 후보 2의 조건은 두 항의 OR이라 이름만
보면 "어느 항 때문인가"가 남습니다. 같은 줄에 `guest_stack=`과 `call_state=`를
찍은 것이 그 질문을 같이 닫았습니다 — **한 번 돌려 세 후보와 그 이유가 동시에
확정됐습니다.**

## 대조군이 대칭을 확정합니다

i386에서 같은 추적을 켜고 43.8초를 돌렸습니다.

```text
i386  [repiu-watch] event=fault guest=0x010F1728 n=1 at=0xF5357005
      [repiu-watch] event=priv  guest=0x010F1728 n=1 at=0x010F1728
      [repiu-exit] 0줄
```

**폴트를 거절하는 일 자체가 없습니다.** 그래서 이것은 "i386도 가끔 여기서
나가는데 x64가 더 자주"가 아니라 **x64만 나간다**입니다.

(i386의 cache 주소가 Task 581의 `0xF5372005`에서 `0xF5357005`로 바뀐 것은
placement 주소가 실행마다 다르기 때문입니다. 되돌아간 guest 주소는 같습니다.)

## 왜 그런가 — 확인된 구조

`GuestCacheEntryThreadProc`(Task 578의 x64 thread proc)은 i386의 **direct**
경로와 같은 모양입니다 — fault handler를 걸고, 실행하고, 지웁니다.
`active_call_state`를 채우지 않습니다.

i386에서 `use_guest_stack`이 참이면 `GuestEntryThreadProc`은 stack-switch
분기로 가서 `StackSwitchCallState`를 채우고 `active_call_state`를 설정합니다.

**x64는 `use_guest_stack`을 참으로 둔 채 그 분기를 거치지 않습니다.**

Task 578의 작업 로그는 "울타리는 둘이 아니라 셋이었습니다"라고 적었습니다.
**넷이었습니다.** 같은 비대칭이 네 번째로 나타난 것입니다.

## 같은 사실이 한 곳 더를 막고 있습니다

`GuestThreadFaultCallback`도 `active_call_state == nullptr`이면 거절을 복구로
바꾸지 못하고 그대로 돌려보냅니다. 그 함수의 주석이 이미 적어 두었습니다 —
"direct-entry 경로에는 착지할 host frame이 없다".

그러니 x64를 통과시키는 것은 **한 곳이 아니라 두 곳**을 여는 일입니다.

## 구현

설계대로입니다. 세 조각뿐입니다.

- `veh_exit_site.h`에 `kForeignThread`·`kGuestStackNotEntered`를 **추가**했습니다.
  재정렬하지 않았습니다 — 그 헤더가 값의 안정성을 명시적으로 요구합니다.
- 조기 반환 두 곳에서 반환 **직전에** 표시했습니다. 회전 블록은 옮기지
  않았습니다(설계 결정 3의 이유).
- `fault_exit_trace.{h,cpp}`와 `GuestThreadFaultCallback` 한 줄.

## 검증

| 항목 | 결과 |
|---|---|
| 추적 없는 i386 `repiu` (45s) | `[repiu-exit]` 0줄, `[repiu-watch]` 0줄, `last_eip=0x010F2786` |
| `REPIU_FAULT_EXIT_TRACE=1` x64 | `site=guest-stack-not-entered`, `guest_stack=1 call_state=0` |
| 같은 추적 i386 대조군 (43.8s) | `[repiu-exit]` 0줄 — 거절 자체가 없음 |
| Linux i386 `repiu_core_probe` | 19/19, failures 0 |
| Linux x64 `repiu_core_probe` | 20/20, failures 0 |
| Win32 configure·build | exit 0, error 0 |
| Win32 `repiu_core_probe` | 19/19, failures 0 |
| Win32 `repiu_aot_probe` (pumpit1) | `_all=true` 41개, `_all=false` 0개 |

Win32 빌드는 이번에는 configure를 명시적으로 먼저 돌렸고 한 번에 통과했습니다.
Task 581에서 스크립트 첫 호출이 실패했던 것과 같은 일은 없었습니다 — **그것이
원인을 확인해 준다고는 보지 않습니다.** 한 번의 성공은 한 번의 성공입니다.

## 아직 확인하지 않음

- **가드를 통과시킨 뒤 x64가 어디까지 가는지 재지 않았습니다.** `sti`가 서비스된
  뒤의 다음 벽은 열린 채로 남습니다.
- 두 수정 방향 중 어느 쪽인지 정하지 않았습니다. `active_call_state`를 채우는
  쪽은 전환이 없는데 `host_esp`가 무엇을 뜻하는지 정의해야 하고, 가드를 다시
  쓰는 쪽은 그 가드가 실제로 무엇을 보장하려는지 확정해야 합니다. **둘 다
  재보지 않았습니다.**
- `pumpit2a`의 미해결 분기 1건은 여전히 쫓지 않았습니다.

---

# Work log 20260903-582 — x64 leaves before it reaches any handler

Design: [20260903-582](../design/20260903-582-fault-exit-attribution.md) ·
Work order: [20260903-582](../work-orders/20260903-582-fault-exit-attribution.md)

## The answer — `guest-stack-not-entered`

```text
x64   [repiu-watch] event=fault guest=0x010F1728 n=1 at=0x20000005
      [repiu-exit]  site=guest-stack-not-entered eip=0x20000005 code=0x0000000B
                    guest_stack=1 call_state=0 n=1
      [repiu-fault] unhandled signal=0xb eip=0x20000005 access=0x0
```

The **second** of the three candidates the design wrote down.

```c
if (context->use_guest_stack &&
    (context->active_call_state == nullptr ||
     context->active_call_state->host_esp == 0))
{
    return repiu::platform::FaultDisposition::kNotHandled;
}
```

`guest_stack=1 call_state=0` makes the first term true and the left side of the
second true. So `DispatchGuestFault` returns **before the AOT transfer block,
before single-step handling, before the HLE chain.**

The `AotHleTranslationScope` Task 581 found never runs. Not because it is
broken — because it is never reached.

## Design decision 4 earned its place

The `site=` name alone would have separated candidate 3, but candidate 2's
condition is an OR of two terms, so the name alone leaves "which term" open.
Printing `guest_stack=` and `call_state=` on the same line closed that question
too — **one run settled all three candidates and the reason.**

## The control settles the asymmetry

The same trace on i386, over 43.8 seconds.

```text
i386  [repiu-watch] event=fault guest=0x010F1728 n=1 at=0xF5357005
      [repiu-watch] event=priv  guest=0x010F1728 n=1 at=0x010F1728
      zero [repiu-exit] lines
```

**It never declines a fault at all.** So this is not "i386 leaves here sometimes
and x64 more often" — **only x64 leaves.**

(i386's cache address moved from Task 581's `0xF5372005` to `0xF5357005` because
the placement lands at a different address per run. The guest address it maps
back to is the same.)

## Why — the confirmed structure

`GuestCacheEntryThreadProc`, Task 578's x64 thread procedure, has the same shape
as i386's **direct** path: install the fault handler, run, clear it. It does not
fill `active_call_state`.

On i386, when `use_guest_stack` is true, `GuestEntryThreadProc` takes the
stack-switch branch, which fills a `StackSwitchCallState` and sets
`active_call_state`.

**x64 leaves `use_guest_stack` true while never taking that branch.**

Task 578's work log wrote "the fences were three, not two". **They were four.**
This is the same asymmetry surfacing a fourth time.

## One fact is blocking a second place

`GuestThreadFaultCallback` also cannot turn a decline into a recovery when
`active_call_state` is null, and hands it straight back. That function's comment
already said so: "the direct-entry path has no host frame to land on".

So getting x64 through opens **two** places, not one.

## Implementation

As designed. Three pieces only.

- `kForeignThread` and `kGuestStackNotEntered` **appended** to
  `veh_exit_site.h`, with no reordering — that header states the stability
  requirement explicitly.
- Both early returns tagged **immediately before** returning. The rotation block
  was not moved, for the reason in design decision 3.
- `fault_exit_trace.{h,cpp}` and one line in `GuestThreadFaultCallback`.

## Verification

| Item | Result |
|---|---|
| i386 `repiu` with the trace off (45s) | 0 `[repiu-exit]`, 0 `[repiu-watch]`, `last_eip=0x010F2786` |
| x64 with `REPIU_FAULT_EXIT_TRACE=1` | `site=guest-stack-not-entered`, `guest_stack=1 call_state=0` |
| i386 control with the same trace (43.8s) | 0 `[repiu-exit]` — nothing is declined |
| Linux i386 `repiu_core_probe` | 19/19, 0 failures |
| Linux x64 `repiu_core_probe` | 20/20, 0 failures |
| Win32 configure and build | exit 0, 0 errors |
| Win32 `repiu_core_probe` | 19/19, 0 failures |
| Win32 `repiu_aot_probe` (pumpit1) | 41 `_all=true`, 0 `_all=false` |

The Win32 build ran configure explicitly first this time and passed on the first
attempt, unlike Task 581's script invocation. **That is not treated as
identifying the cause** — one success is one success.

## Not yet verified

- **How far x64 gets once past that guard was not measured.** The next wall
  after the `sti` is serviced remains open.
- Neither repair direction was chosen. Filling `active_call_state` requires
  defining what `host_esp` means with no switch to return from; rewriting the
  guard requires settling what it is actually meant to guarantee. **Neither was
  measured.**
- `pumpit2a`'s one unresolved branch is still unchased.
