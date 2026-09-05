# 작업 로그 20260905-592 — Linux x64 long-mode segment guard coverage

설계: [20260905-592](../design/20260905-592-linux-x64-long-mode-guard-coverage.md)  
작업 지시: [20260905-592](../work-orders/20260905-592-linux-x64-long-mode-guard-coverage.md)

## 결과

`ValidateAotCodeCacheHleCoverage`가 long-mode `kGuardedSegmentPop`의 실제 ABI를
검사하도록 보강했습니다. 이 ABI는 guest flags save/restore lowering, saved guest-stack
selector와 shadow selector 비교, mismatch INT3, success guest-ESP `+4`, fallthrough jump로
구성됩니다. validator는 emitted slot의 길이·offset·bytes·resolved fixup을 확인하며,
손상된 fallback INT3는 같은 guest 주소로 계속 거절합니다. i386 guard 검사는 변경하지
않았습니다.

portable `long_mode_emission` probe에 normal pop image와 fallback INT3 손상 image를
추가했고 모두 기대대로 통과/거절했습니다.

watched `pumpit2a`에서 `0x010F4AD1` dynamic append는 이제 cache address `0x2004FB64`로
resolve되어 재진입했습니다.

```text
[repiu-x64-return] result=resolved source=0x010F4AD1 cache=0x2004FB64 detail=
```

기존 `0x010F4ACD` coverage 실패와 return thunk의 signal-5/signal-4 INT3 종료는 더 이상
발생하지 않았습니다. 새 frontier는 raw guest RIP/EIP `0x010F010C`에서의 signal `0xB`
(`SIGSEGV`)입니다.

## 검증

* WSL Linux x64 `repiu_core_probe` 및 `repiu` 빌드 성공.
* `repiu_core_probe`: `long_mode_segment_guard_coverage=true`,
  `pop_corruption_rejected=true`, `core_probe_all=true`.
* `REPIU_GUEST_WATCH=0x010F4A96 REPIU_LINUX_X64_RETURN_TRACE=1 timeout 5s`
  watched `pumpit2a`: `0x010F4AD1` 두 번과 `0x010F0103`을 resolve한 뒤
  `signal=0xB rip=eip=0x010F010C`를 확인했습니다. 외부 timeout 종료 코드는 `124`였습니다.

## 후속 작업

`0x010F010C`가 raw guest address가 된 resolver/dispatch 경로를 분석하고, original bytes를
long mode에서 실행하지 않도록 cache 또는 HLE 경계로 안전하게 복귀시켜야 합니다.

---

# Work log 20260905-592 — Linux x64 long-mode segment-pop coverage

Design: [20260905-592](../design/20260905-592-linux-x64-long-mode-guard-coverage.md)  
Work order: [20260905-592](../work-orders/20260905-592-linux-x64-long-mode-guard-coverage.md)

## Result

Long-mode `kGuardedSegmentPop` coverage now validates the actual guest-flags
save/restore lowering, shadow-selector comparison, mismatch INT3, success
guest-ESP advance, and resolved fallthrough jump. The i386 validation remains
unchanged, and a corrupted fallback remains rejected at the original guest
address.

The portable probe passes. Watched `pumpit2a` now resolves `0x010F4AD1` to
cache `0x2004FB64`; the former `0x010F4ACD` coverage failure and return-thunk
INT3 signals are gone. Execution instead reaches a new raw guest `SIGSEGV` at
`RIP/EIP 0x010F010C`.

## Verification

* WSL Linux x64 `repiu_core_probe` and `repiu` built successfully.
* `long_mode_segment_guard_coverage=true`, `pop_corruption_rejected=true`, and
  `core_probe_all=true`.
* The five-second watched run resolved `0x010F4AD1` twice and `0x010F0103`
  before observing `signal=0xB rip=eip=0x010F010C`; outer timeout exit was
  `124`.

## Follow-up

Trace the resolver or dispatch path that produced raw guest `0x010F010C`, then
return it safely to cache or an HLE boundary without executing raw guest bytes
in long mode.
