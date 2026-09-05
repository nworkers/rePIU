# 작업 지시 20260905-596 — Linux x64 게스트 INT3 재진입 처리

설계: [20260905-596](../design/20260905-596-linux-x64-guest-int3-reentry.md)

## 목표

Linux x64 AOT cache boundary에서 게스트 소유 `INT3`가 single-step trace에
가려져 반복되는 문제를 수정하고, `pumpit2a`가 해당 바이트를 한 번 전진해
다음 AOT reentry로 이동하는지 확인합니다.

## 작업

1. `DispatchGuestFault`에서 게스트 소유 breakpoint 판정을 single-step
   trace보다 앞에 배치합니다.
2. trace sentinel 및 cache breakpoint의 기존 소유권 규칙을 보존합니다.
3. Linux x64 `repiu`와 `repiu_core_probe`를 재빌드합니다.
4. core probe와 짧은 `pumpit2a` 실행을 수행합니다.
5. 새 runtime evidence를 `docs/analysis/linux-port-frontier.md`와 작업 로그에
   기록합니다.

## 완료 조건

* `repiu_core_probe`가 `core_probe_failures=0`으로 종료됩니다.
* 실행 로그에 `[repiu-guest-int3]`가 나타납니다.
* `last_eip=0x010F022C`가 단조 반복되지 않고 다음 frontier 또는 명확한
  후속 blocker가 관측됩니다.
* 변경 사항과 검증 명령이 작업 로그에 남습니다.

## English

# Work order 20260905-596 — Linux x64 guest INT3 reentry handling

Design: [20260905-596](../design/20260905-596-linux-x64-guest-int3-reentry.md)

## Objective

Fix the guest-owned `INT3` that is hidden by the single-step trace path after
an AOT cache boundary on Linux x64, then verify that `pumpit2a` advances past
the byte and reaches the next AOT reentry.

## Work

1. Move the guest-owned breakpoint check in `DispatchGuestFault` ahead of the
   single-step trace path.
2. Preserve the existing ownership rules for trace sentinels and cache
   breakpoints.
3. Rebuild Linux x64 `repiu` and `repiu_core_probe`.
4. Run the core probe and a short `pumpit2a` sample.
5. Record the new runtime evidence in `docs/analysis/linux-port-frontier.md`
   and the work log.

## Completion criteria

* `repiu_core_probe` exits with `core_probe_failures=0`.
* The runtime log contains `[repiu-guest-int3]`.
* `last_eip=0x010F022C` is not a monotonic repetition and the next frontier or
  a clearly identified blocker is observed.
* The work log records the changes and verification commands.
