# 작업 로그 20260905-596 — Linux x64 게스트 INT3 재진입 처리

## 결과

Task 595의 `INT 31h AX=1E7F` HLE 처리 뒤 게스트 소유 `INT3`가 single-step
trace 경로에 가려져 같은 주소에서 반복되던 문제를 수정했습니다. `DispatchGuestFault`의
처리 순서를 바꾸어 게스트 breakpoint를 single-step 처리보다 먼저 소비하도록 했습니다.

## 확인된 실행 결과

Linux x64에서 다음을 다시 빌드했습니다.

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
```

`repiu_core_probe` 결과:

```text
core_probe_total=20
core_probe_failures=0
core_probe_all=true
```

`pumpit2a` 짧은 실행에서 다음을 확인했습니다.

```text
[repiu-dos-int] #3 int=31 ax=1E7F
[repiu-guest-int3] #1 eip=0x010F022C eax=0x00008001 ...
[repiu-fault] unhandled signal=0xb rip=0x2004fb6b eip=0x2004fb6b access=0x0 ...
```

`[repiu-guest-int3]`가 한 번만 기록되었고, 이전의 `last_eip=0x010F022C`
반복은 재현되지 않았습니다. 이후 `0x010F0232`에 대한 dispatch/cache 진입이
관찰되었으며, 새 blocker는 `0x2004FB6B`의 AOT cache SIGSEGV로 이동했습니다.

## 판단

Task 596의 목표인 게스트 `INT3` 소비와 다음 AOT frontier 도달은 확인했습니다.
현재 실행을 막는 문제는 게스트 `INT3` 재진입이 아니라, `pop es` 이후 AOT cache
내부에서 null 주소를 참조한 별도 문제입니다. 다음 작업은 종료 fault 시점의 실제
cache opcode와 접근 주소를 기록하여 AOT 슬롯/분기 패치 오류 여부를 확정해야 합니다.

## English

# Work log 20260905-596 — Linux x64 guest INT3 reentry handling

## Result

Task 595's `INT 31h AX=1E7F` HLE handling exposed a guest-owned `INT3` that was
hidden by the single-step trace path and repeated at one address. The fix moves
guest breakpoint consumption ahead of single-step handling in `DispatchGuestFault`.

## Verified runtime result

Linux x64 was rebuilt with:

```text
cmake --build build/linux_x64_debug --target repiu repiu_core_probe -j 2
```

`repiu_core_probe` reported:

```text
core_probe_total=20
core_probe_failures=0
core_probe_all=true
```

A short `pumpit2a` run reported:

```text
[repiu-dos-int] #3 int=31 ax=1E7F
[repiu-guest-int3] #1 eip=0x010F022C eax=0x00008001 ...
[repiu-fault] unhandled signal=0xb rip=0x2004fb6b eip=0x2004fb6b access=0x0 ...
```

`[repiu-guest-int3]` occurred once and the former `last_eip=0x010F022C`
repetition did not recur. Dispatch/cache entry activity for `0x010F0232` was
observed, and the new blocker moved to an AOT-cache SIGSEGV at `0x2004FB6B`.

## Assessment

Task 596's goal—consume the guest `INT3` and reach the next AOT frontier—is
confirmed. The remaining blocker is separate: a null-address access inside the
AOT cache after `pop es`. The next task must capture the actual cache opcode and
fault address to distinguish an AOT slot or branch-patching defect.
