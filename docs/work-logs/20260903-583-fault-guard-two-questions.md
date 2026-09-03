# 작업 기록 20260903-583 — x64가 `sti`를 서비스하고, 673바이트 뒤에서 멈춘다

설계: [20260903-583](../design/20260903-583-fault-guard-two-questions.md) ·
작업 지시: [20260903-583](../work-orders/20260903-583-fault-guard-two-questions.md)

## 결과 — x64가 HLE 서비스를 받았습니다

```text
[repiu-watch] event=fault guest=0x010F1728 n=1 at=0x20000005
[repiu-watch] event=priv  guest=0x010F1728 n=1 at=0x010F1728
```

**i386이 내는 것과 같은 두 줄입니다.** 폴트가 cache 주소로 도착했고, 서비스는
guest 주소에서 일어났습니다. Task 581이 찾아낸 `AotHleTranslationScope`가 x64에서
처음으로 실제로 동작했습니다.

`[repiu-exit] site=guest-stack-not-entered`는 사라졌습니다.

## 설계가 적어 둔 예상 실패는 일어나지 않았습니다

설계는 "서비스 후 `++Eip`한 guest `0x010F1729`를 `FindAotCacheAddress`로
되돌리는 조회가 실패할 수 있다"고 적고, 실패하면 관측으로 남기라고 했습니다.

**실패하지 않았습니다.** 되돌아갔고 실행이 이어졌습니다. 그 증거는 다음 폴트가
같은 자리가 아니라 **673바이트 뒤**에서 났다는 것입니다.

## 새 정지점 — guest `0x010F18A4`

```text
[repiu-exit]  site=no-host-frame-to-unwind eip=0x200002AA code=0x0000000B
              guest_stack=1 call_state=0 n=1
[repiu-fault] unhandled signal=0xb eip=0x200002aa access=0x200202
```

Task 579가 만든 census `--cache`가 그 자리를 특정합니다.

```text
   cache=0x26e len=58  guest=0x10f189a  kind=kSegmentOverrideMem
      emitted: ... 66 67 8e 1c 25 00 00 00 00 e9 00 00 00 00
   cache=0x2a8 len=2   guest=0x10f18a2  kind=kCopy   emitted: 29 ed
>> cache=0x2aa len=3   guest=0x10f18a4  kind=kCopy   emitted: 67 8b 06
```

guest `0x010F18A4`의 `mov eax, [esi]`입니다.

정지점이 cache offset `0x5`에서 `0x2AA`로 옮겨갔습니다. 그 사이에
`kSegmentOverrideMem` 블록 하나와 guarded DS load가 있습니다.

## 새 exit site가 제 역할을 했습니다

이번 정지는 `no-host-frame-to-unwind`로 이름이 붙었습니다. 이것은 이 단위가
추가한 값이고, **의미하는 바가 정확합니다** — handler chain이 서비스하지 못했고,
x64에는 되감을 프레임이 없으므로 거절했습니다.

거절이 옳은 선택이라는 근거는 설계 결정 2에 있습니다. `RecoverToHost`로 보냈다면
`ud2`를 향해, 그것도 32비트로 잘린 주소로 뛰었을 것입니다. 거절은 **x64가 지금까지
하던 것과 정확히 같은 결말**이므로 이 변경이 x64를 나쁘게 만들 수 없습니다.

## 추정 — 세그먼트 base가 붙지 않았을 가능성

접근 주소가 `0x200202`입니다. 이 이미지의 selector base는 `0x1000000`,
`0x1010000`, `0x1100000`, `0x1110000`입니다. **어느 base도 더해지지 않은 값과
일관됩니다** — `0x1010000`이 더해졌다면 `0x1210202` 근방이었을 것입니다.

바로 앞 블록이 guarded DS load로 끝나고, 문제의 명령은 세그먼트 override가 없는
`kCopy`라 **기본 DS**를 씁니다. long mode에서 DS base는 0입니다.

**이것은 추정입니다.** `ESI`의 실제 값을 재지 않았으므로 "base가 빠졌다"와
"`ESI`가 애초에 그 값이었다"를 구분하지 못합니다. 이 세션에서 추정이 다섯 번
반증됐으므로 결론으로 적지 않습니다. **다음 단위는 이것부터 재야 합니다.**

## 구현

설계대로 네 조각입니다.

- `ThreadContext::cache_entry_active` — x64 cache 진입의 실행 창. **오직
  `GuestCacheEntryThreadProc`만 설정합니다.**
- 진입 가드를 `guest_stack_entered`와 `cache_entry_active`를 아는 형태로 다시
  썼습니다. i386에서 새 항이 항상 거짓이므로 그 호스트의 판단은 논리적으로
  동일합니다.
- 포기 지점 두 곳(4055·4405행 근처)에서 `active_call_state`가 널이면 역참조
  대신 `kNotHandled`를 반환하고 `kNoHostFrameToUnwind`로 표시합니다.
- `veh_exit_site.h`에 그 값을 **추가**했습니다(재정렬 금지).

## 낡은 주석 하나를 정정했습니다

`guest_stack_recover_x64.S`가 "x64에서는 게스트 스레드가 시작되지 않으므로 이
심볼들은 도달 불가"라고 적고 있었습니다. Task 575 시점에는 사실, Task 578
이후로는 거짓입니다.

고친 이유는 위생이 아닙니다. **이 단위가 x64를 그 영역에 더 가까이 보냅니다.**
"여기 올 일 없음" 옆에서 도달 가능성을 늘리는 것은 다음 사람에게 정확히 잘못된
안심을 줍니다. `ud2` 본문은 그대로 두었고, 이제 도달 불가는 우연이 아니라
`no-host-frame-to-unwind` 거절로 **강제됩니다.**

## 검증

| 항목 | 결과 |
|---|---|
| **i386 회귀** (`pumpit2a`, 45s, 추적 켬) | `[repiu-exit]` 0줄, `[repiu-fault]` 0줄, `last_eip=0x010F2786`, `single_step=11532`, `aot=11689/25913` — Task 582와 같은 규모 |
| x64: `guest-stack-not-entered` 소멸 | 확인 |
| x64: `event=priv` | **나옴** — i386과 같은 이벤트 |
| x64 새 정지점 | `eip=0x200002AA`, `access=0x200202`, guest `0x010F18A4` |
| Linux i386 `repiu_core_probe` | 19/19, failures 0 |
| Linux x64 `repiu_core_probe` | 20/20, failures 0 |
| Win32 build | exit 0, error 0 |
| Win32 `repiu_core_probe` | 19/19, failures 0 |
| Win32 `repiu_aot_probe` (pumpit1) | `_all=true` 41개, `_all=false` 0개 |

## 아직 확인하지 않음

- **`ESI`의 값을 재지 않았습니다.** 위의 세그먼트 base 추정을 확정하거나
  반증하는 것이 다음 단위의 첫 일입니다.
- **673바이트 사이에서 무엇이 실행됐는지 재지 않았습니다.** `sti` 뒤부터
  `0x2AA`까지 어떤 블록들을 지났는지, guarded DS load가 무엇을 했는지 모릅니다.
- x64가 `pumpit2a` 외의 ROM에서 어떻게 되는지 보지 않았습니다.
- `pumpit2a`의 미해결 분기 1건은 여전히 쫓지 않았습니다.

---

# Work log 20260903-583 — x64 services the `sti` and stops 673 bytes later

Design: [20260903-583](../design/20260903-583-fault-guard-two-questions.md) ·
Work order: [20260903-583](../work-orders/20260903-583-fault-guard-two-questions.md)

## Result — x64 received an HLE service

```text
[repiu-watch] event=fault guest=0x010F1728 n=1 at=0x20000005
[repiu-watch] event=priv  guest=0x010F1728 n=1 at=0x010F1728
```

**The same two lines i386 produces.** The fault arrived at a cache address and
the service happened at the guest address. The `AotHleTranslationScope` Task 581
found has now actually run on x64 for the first time.

`[repiu-exit] site=guest-stack-not-entered` is gone.

## The failure the design anticipated did not happen

The design wrote that the post-service lookup mapping guest `0x010F1729` back
through `FindAotCacheAddress` might fail, and said to record it if so.

**It did not fail.** It mapped back and execution continued. The evidence is
that the next fault is not at the same place but **673 bytes further on**.

## The new stopping point — guest `0x010F18A4`

```text
[repiu-exit]  site=no-host-frame-to-unwind eip=0x200002AA code=0x0000000B
              guest_stack=1 call_state=0 n=1
[repiu-fault] unhandled signal=0xb eip=0x200002aa access=0x200202
```

The census `--cache` built by Task 579 pins the place down.

```text
   cache=0x26e len=58  guest=0x10f189a  kind=kSegmentOverrideMem
      emitted: ... 66 67 8e 1c 25 00 00 00 00 e9 00 00 00 00
   cache=0x2a8 len=2   guest=0x10f18a2  kind=kCopy   emitted: 29 ed
>> cache=0x2aa len=3   guest=0x10f18a4  kind=kCopy   emitted: 67 8b 06
```

`mov eax, [esi]` at guest `0x010F18A4`.

The stopping point moved from cache offset `0x5` to `0x2AA`, across one
`kSegmentOverrideMem` block and a guarded DS load.

## The new exit site did its job

This stop is named `no-host-frame-to-unwind`. That value is this unit's
addition, and **it means precisely what happened**: the handler chain could not
service the fault, and x64 has no frame to unwind to, so it declined.

Why declining is the right choice is design decision 2. Sending it to
`RecoverToHost` would have jumped at a `ud2`, through a 32-bit-truncated
address. Declining is **exactly the ending x64 already had**, which is why this
change cannot make x64 worse.

## Inferred — a segment base that may not have been applied

The access address is `0x200202`. This image's selector bases are `0x1000000`,
`0x1010000`, `0x1100000` and `0x1110000`. **The value is consistent with none of
them having been added** — with `0x1010000` applied it would have been near
`0x1210202`.

The preceding block ends in a guarded DS load, and the faulting instruction is a
`kCopy` with no segment override, so it uses the **default DS**, whose base is
zero in long mode.

**This is an inference.** `ESI`'s value was not measured, so "the base is
missing" and "`ESI` simply held that value" are not separated. Five inferences
have been refuted this session, so it is not written down as a conclusion.
**The next unit should measure this first.**

## Implementation

Four pieces, as designed.

- `ThreadContext::cache_entry_active` — the execution window of the x64 cache
  entry. **Set only by `GuestCacheEntryThreadProc`.**
- The entry guard rewritten in terms of `guest_stack_entered` and
  `cache_entry_active`. The new term is always false on i386, so that host's
  decision is logically identical.
- Both give-up sites (near lines 4055 and 4405) return `kNotHandled` and tag
  `kNoHostFrameToUnwind` when `active_call_state` is null, instead of
  dereferencing it.
- That value **appended** to `veh_exit_site.h`, with no reordering.

## One stale comment corrected

`guest_stack_recover_x64.S` said these symbols are unreachable because no guest
thread starts on x64. True as of Task 575; false since Task 578.

The reason to fix it is not hygiene. **This unit moves x64 closer to that
region.** Increasing reachability next to "nothing can get here" gives the next
reader exactly the wrong reassurance. The `ud2` bodies are unchanged, and
unreachability is now **enforced** by the `no-host-frame-to-unwind` refusal
rather than incidental.

## Verification

| Item | Result |
|---|---|
| **i386 regression** (`pumpit2a`, 45s, trace on) | 0 `[repiu-exit]`, 0 `[repiu-fault]`, `last_eip=0x010F2786`, `single_step=11532`, `aot=11689/25913` — the magnitudes Task 582 recorded |
| x64: `guest-stack-not-entered` gone | confirmed |
| x64: `event=priv` | **present** — the same event i386 produces |
| x64's new stopping point | `eip=0x200002AA`, `access=0x200202`, guest `0x010F18A4` |
| Linux i386 `repiu_core_probe` | 19/19, 0 failures |
| Linux x64 `repiu_core_probe` | 20/20, 0 failures |
| Win32 build | exit 0, 0 errors |
| Win32 `repiu_core_probe` | 19/19, 0 failures |
| Win32 `repiu_aot_probe` (pumpit1) | 41 `_all=true`, 0 `_all=false` |

## Not yet verified

- **`ESI` was not measured.** Confirming or refuting the segment-base inference
  above is the next unit's first job.
- **What executed across those 673 bytes was not measured.** Which blocks ran
  between the `sti` and `0x2AA`, and what the guarded DS load did, is unknown.
- x64 was not run on any ROM other than `pumpit2a`.
- `pumpit2a`'s one unresolved branch is still unchased.
