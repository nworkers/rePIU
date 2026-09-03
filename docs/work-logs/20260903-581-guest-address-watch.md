# 작업 기록 20260903-581 — i386은 cache 안에서 `sti`를 실행하고, 그것을 서비스한다

설계: [20260903-581](../design/20260903-581-guest-address-watch.md) ·
작업 지시: [20260903-581](../work-orders/20260903-581-guest-address-watch.md)

## 결과 — Task 580의 결론 두 개가 반증됐습니다

측정 하나가 두 가지를 뒤집었습니다.

```text
i386  [repiu-watch] event=fault guest=0x010F1728 n=1 at=0xF5372005
      [repiu-watch] event=priv  guest=0x010F1728 n=1 at=0x010F1728

x64   [repiu-watch] event=fault guest=0x010F1728 n=1 at=0x20000005
      [repiu-fault] unhandled signal=0xb eip=0x20000005 access=0x0
```

| Task 580이 적은 것 | 실제 |
|---|---|
| i386은 그 `sti`를 cache에서 실행하지 않을 가능성이 높다 (추정으로 표시) | **cache에서 실행한다** |
| cache 안 access-violation 폴트를 guest 주소로 되돌리는 경로가 **없다** (확인됨으로 표시) | **있다 — `AotHleTranslationScope`** |

두 번째가 더 무겁습니다. Task 580은 그것을 "확인된 것"으로 적었고, 근거는
`HandleAotReentry`가 `kBreakpoint`에서만 되돌린다는 관찰이었습니다. **관찰은
옳았고 결론이 틀렸습니다** — 그것이 유일한 경로가 아니었습니다.

## 무엇을 읽어야 그 결론이 나오는가

`priv` 이벤트의 `at=`이 **guest 주소**입니다. 폴트는 cache 주소
(`0xF5372005`)로 도착했는데, `HandlePrivilegedTrapInstruction`은 guest 주소
(`0x010F1728`)에서 명령 바이트를 읽고 서비스했습니다. 그 사이에서 `Eip`를 바꾼
것이 있다는 뜻이고, 코드를 따라가면 `AotHleTranslationScope`입니다.

`src/engine/execution/execution_trampoline.cpp`의 RAII scope로, HLE handler
chain의 머리에서 생성됩니다.

- 생성자 — `Eip`가 cache 주소면 `FindAotGuestAddress`로 guest 주소를 찾아 넣는다.
- 소멸자 — handler가 `Eip`를 옮겼으면(`sti` 서비스의 `++Eip`) 그 guest 주소를
  `FindAotCacheAddress`로 cache 주소로 되돌리고, 아니면 원래 cache 주소를
  복원한다.

**Task 580이 "작아 보이는 것이 함정"이라며 필요하다고 적은 반대 방향이 바로
여기 있습니다.** Task 580이 연 두 방향 중 첫 번째는 구현할 것이 아니라 이미
구현되어 있었습니다.

## `step`·`dispatch`·`cache` 이벤트는 하나도 없습니다

43초 실행에서 그 세 이벤트가 **0**입니다. i386은 그 block에 dispatch로 들어가지
않습니다 — cache 안의 직접 점프로 닿습니다.

이것이 설계 결정 3이 미리 대비한 경우입니다. `fault` 이벤트를 넣지 않았다면
결과가 전부 0이 되고, "실행이 닿지 않았다"와 "직접 점프로 닿았다"를 구분할 수
없었습니다. **설계가 그 구멍을 미리 막은 것이 이 단위에서 유일하게 예측이 맞은
부분입니다.**

`n=1`입니다. 그 block은 43초 동안 한 번 돕니다.

## 두 호스트가 같은 폴트를 냅니다

| | i386 | x64 |
|---|---|---|
| 폴트 주소 | `0xF5372005` | `0x20000005` |
| 되돌린 guest 주소 | `0x010F1728` | `0x010F1728` |
| cache offset | 5 | 5 |
| 결과 | 서비스됨, 실행 계속 | 미처리, `exit=139` |

방출된 entry block이 양쪽 다 5바이트라는 뜻이기도 합니다. Task 579의 census가
long-mode 이미지에서 본 `cache=0x0 len=5`가 i386 이미지에서도 같습니다.

## 그래서 남은 질문이 더 좁아졌습니다

`fault` 훅은 `DispatchGuestFault`의 **모든 조기 반환보다 위**에 있습니다. x64에서
그것이 찍혔다는 것은 **폴트가 그 함수에 도달한다**는 뜻입니다.

**따라서 문제는 폴트가 안 오는 것이 아니라, `DispatchGuestFault` 진입과 HLE
chain 사이에서 x64가 그것을 거절한다는 것입니다.** i386은 같은 구간을 지나
서비스합니다.

Task 580이 열어 둔 두 방향은 둘 다 틀린 전제 위에 있었으므로, **어느 쪽도 다음
단위가 아닙니다.** 다음 단위의 질문은 이것입니다 — x64는 그 구간의 어느 조기
반환을 타는가.

## 구현

설계대로 다섯 훅을 넣었습니다. 하나만 설계와 다르게 놓았습니다.

- `priv`를 `HandlePrivilegedTrapInstruction`의 세 호출부가 아니라
  `RecordHandledHleTrap` 안에 넣었습니다. 그 함수의 호출자는 그 셋뿐이고,
  그 함수에 도달했다는 것 자체가 "서비스됐다"이므로 이벤트의 정의와 정확히
  일치합니다.
- `ResolveAotTransferTarget`은 본문을 익명 네임스페이스의
  `ResolveAotTransferTargetBody`로 분리하고 wrapper를 남겼습니다. 성공 반환이
  다섯 곳에 흩어져 있어, 훅을 그 다섯 곳에 흩는 대신 wrapper 한 곳에 모았습니다.

## 검증

| 항목 | 결과 |
|---|---|
| 감시 없는 i386 `repiu` (`pumpit2a`, 45s) | `[repiu-watch]` 0줄, `repiu-fault` 0줄, `last_eip=0x010F2786` — Task 580과 동일 |
| `REPIU_GUEST_WATCH=0x010F1728` i386 | `fault`·`priv` 각 1회, 실행 계속 (43.7s) |
| 같은 감시로 x64 `repiu` | `fault` 1회, 그다음 미처리 `exit=139` |
| Linux i386 `repiu_core_probe` | 19/19, failures 0 |
| Linux x64 `repiu_core_probe` | 20/20, failures 0 |
| Win32 x86 Debug 빌드 | 전 타깃 링크, error 0 |
| Win32 `repiu_core_probe` | 19/19, failures 0 |
| Win32 `repiu_aot_probe` (pumpit1·pumpit8) | `_all=true` 41개, `_all=false` 0개 — Task 578과 동일 |

Win32에서 두 가지를 적어 둡니다.

- `scripts/build_win32_x86.ps1`의 **첫 호출이 exit 1로 실패**했고, 그 출력
  필터를 통과한 error 줄은 없었습니다. 아무것도 바꾸지 않고 `cmake --build`를
  직접 다시 돌리니 error 0으로 전 타깃이 링크됐습니다. 새 source가 들어가면서
  프로젝트가 재생성된 직후의 일회성 실패로 봅니다 — **재현하지 않았으므로 원인은
  확인된 것이 아닙니다.**
- `repiu_aot_probe`는 `pumpit2a`의 `PIU.EXE`에 대해서는
  `direct control-flow target is outside the cache`로 실패합니다. 이것은 이번
  변경과 무관하게 그 이미지가 probe의 단정 대상이 아니기 때문이고, Task 578이
  기록한 41/0은 `pumpit1`·`pumpit8`에서 그대로 재현됩니다.

## 아직 확인하지 않음

- **x64가 `DispatchGuestFault`의 어느 조기 반환을 타는지 재지 않았습니다.**
  후보는 guest thread id 검사, `use_guest_stack`·`active_call_state` 검사,
  `AotHleTranslationScope` 앞의 handler들입니다. **후보 목록이지 진단이
  아닙니다.**
- 감시를 다른 주소나 다른 ROM에 대해 돌려 보지 않았습니다.
- `pumpit2a`의 미해결 분기 1건은 여전히 쫓지 않았습니다.

---

# Work log 20260903-581 — i386 runs the `sti` inside the cache, and services it

Design: [20260903-581](../design/20260903-581-guest-address-watch.md) ·
Work order: [20260903-581](../work-orders/20260903-581-guest-address-watch.md)

## Result — two of Task 580's conclusions are refuted

One measurement overturned both.

```text
i386  [repiu-watch] event=fault guest=0x010F1728 n=1 at=0xF5372005
      [repiu-watch] event=priv  guest=0x010F1728 n=1 at=0x010F1728

x64   [repiu-watch] event=fault guest=0x010F1728 n=1 at=0x20000005
      [repiu-fault] unhandled signal=0xb eip=0x20000005 access=0x0
```

| What Task 580 wrote | What is true |
|---|---|
| i386 very likely does not execute that `sti` from the cache (marked inferred) | **it does** |
| **nothing** translates an access-violation fault inside the cache back to a guest address (marked confirmed) | **something does — `AotHleTranslationScope`** |

The second is the heavier one. Task 580 filed it under "Confirmed" on the
strength of an observation: `HandleAotReentry` translates only for
`kBreakpoint`. **The observation was right and the conclusion was wrong** — that
was not the only such path.

## What has to be read to reach that conclusion

The `priv` event's `at=` is a **guest** address. The fault arrived at a cache
address (`0xF5372005`), yet `HandlePrivilegedTrapInstruction` read the
instruction byte at the guest address (`0x010F1728`) and serviced it. So
something rewrote `Eip` in between, and following the code it is
`AotHleTranslationScope`.

An RAII scope in `src/engine/execution/execution_trampoline.cpp`, constructed at
the head of the HLE handler chain.

- Constructor — when `Eip` is a cache address, resolve the guest address with
  `FindAotGuestAddress` and put it there.
- Destructor — if a handler moved `Eip` (the `++Eip` of servicing `sti`), map
  that guest address back with `FindAotCacheAddress`; otherwise restore the
  original cache address.

**The reverse direction Task 580 called "the trap" is exactly here.** The first
of the two directions it opened was not something to implement; it was already
implemented.

## Not one `step`, `dispatch` or `cache` event

Those three are **zero** over a 43-second run. i386 does not enter that block
through dispatch — it arrives by a direct jump inside the cache.

This is the case design decision 3 was written for. Without the `fault` event
the result would have been all zeros, with no way to separate "execution never
reached it" from "execution reached it by a direct jump". **Closing that hole in
advance is the one prediction this unit got right.**

`n=1`. The block executes once in 43 seconds.

## Both hosts raise the same fault

| | i386 | x64 |
|---|---|---|
| Fault address | `0xF5372005` | `0x20000005` |
| Guest address it maps back to | `0x010F1728` | `0x010F1728` |
| Cache offset | 5 | 5 |
| Outcome | serviced, execution continues | unhandled, `exit=139` |

Which also says the emitted entry block is five bytes on both. What Task 579's
census saw in the long-mode image as `cache=0x0 len=5` holds in the i386 image
too.

## So the remaining question is narrower

The `fault` hook sits **above every early return** in `DispatchGuestFault`. That
it printed on x64 means **the fault does reach that function**.

**So the problem is not that the fault fails to arrive; it is that x64 declines
it somewhere between the entry of `DispatchGuestFault` and the HLE chain,** where
i386 passes through and services it.

Both directions Task 580 left open rest on a wrong premise, so **neither is the
next unit.** The next unit's question is this one: which early return in that
stretch does x64 take?

## Implementation

The five hooks went in as designed. One was placed differently.

- `priv` is recorded inside `RecordHandledHleTrap` rather than at the three call
  sites in `HandlePrivilegedTrapInstruction`. Those three are its only callers,
  and reaching that function *is* "the instruction was serviced" — exactly the
  event's definition.
- `ResolveAotTransferTarget` had its body split into
  `ResolveAotTransferTargetBody` in an anonymous namespace, with a wrapper left
  behind. Its success returns are scattered across five places, so the hooks are
  gathered in the one wrapper rather than scattered to match.

## Verification

| Item | Result |
|---|---|
| i386 `repiu` with the watch off (`pumpit2a`, 45s) | 0 `[repiu-watch]` lines, 0 `repiu-fault`, `last_eip=0x010F2786` — identical to Task 580 |
| i386 with `REPIU_GUEST_WATCH=0x010F1728` | `fault` and `priv` once each, execution continues (43.7s) |
| x64 `repiu` with the same watch | `fault` once, then unhandled, `exit=139` |
| Linux i386 `repiu_core_probe` | 19/19, 0 failures |
| Linux x64 `repiu_core_probe` | 20/20, 0 failures |
| Win32 x86 Debug build | every target linked, 0 errors |
| Win32 `repiu_core_probe` | 19/19, 0 failures |
| Win32 `repiu_aot_probe` (pumpit1, pumpit8) | 41 `_all=true`, 0 `_all=false` — same as Task 578 |

Two things worth recording about the Win32 side.

- The **first invocation of `scripts/build_win32_x86.ps1` failed with exit 1**,
  and no error line survived its output filter. Re-running `cmake --build`
  directly, with nothing changed, linked every target with zero errors. It reads
  as a one-off right after the new source caused the projects to be regenerated
  — **but it was not reproduced, so the cause is not confirmed.**
- `repiu_aot_probe` fails against `pumpit2a`'s `PIU.EXE` with
  `direct control-flow target is outside the cache`. That is unrelated to this
  change: that image is not what the probe's assertions are written against, and
  Task 578's 41/0 reproduces exactly on `pumpit1` and `pumpit8`.

## Not yet verified

- **Which early return in `DispatchGuestFault` x64 takes was not measured.** The
  candidates are the guest thread-id check, the
  `use_guest_stack`/`active_call_state` check, and the handlers ahead of
  `AotHleTranslationScope`. **That is a candidate list, not a diagnosis.**
- The watch was not pointed at any other address, nor run on another ROM.
- `pumpit2a`'s one unresolved branch is still unchased.
