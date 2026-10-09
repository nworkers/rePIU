# issue #8 (Task 765) 작업 로그 — stack bridge probe의 Release 실패 수정

> **경위(2026-10-09, issue #8):** 이 작업은 2026-10-01 Task 765로 끝났지만, 그 커밋(`059a08b`)이 있던
> 브랜치 `docs/763-close-resolved-frontier-items`가 머지되지 않은 채 사라져 수정이 main에 들어가지
> 않았습니다. 같은 실패가 2026-10-05 issue #8로 다시 보고됐고, 2026-10-09 같은 수정을 main 위에 다시
> 적용했습니다. 본문은 Task 765 당시의 기록입니다.


설계: [20261009-i008](../design/20261009-i008-stack-bridge-probe-release-refusal-check.md)
작업 지시: [20261009-i008](../work-orders/20261009-i008-stack-bridge-probe-release-refusal-check.md)

## 요약

Win32 Release `repiu_core_probe`의 `stack_bridge_contract=false`는 엔진의 결함이 아니라 probe의 검사식이
컴파일러의 codegen에 기대고 있던 것이었습니다. 거절 경로가 EAX를 정확히 보존해 이전 호출의 마커를 그대로
돌려주자 "마커가 다시 나오면 안 된다"는 검사가 실패로 읽었습니다. EAX를 `0x0BADF00D`로 고정하고 thunk를
부르는 진입점을 두고, 검사를 "정확히 그 값이 돌아온다"로 바꿨습니다.

## 원인을 찾은 순서

1. Release probe를 다시 빌드해 돌리니 stack bridge의 세 검사 중 `contract`만 실패했습니다
   (`refusal_fallback`·`fault_on_host_stack`은 통과). Linux frontier §6이 함께 적어 둔
   `fault_handler_data_faults`는 지금 Release에서 통과합니다.
2. `ProbeBridgeContract`를 읽으니 두 번째 절반의 `refused != kResolverReturnMarker`가 거절 경로
   (`pushad … popad; ret`)의 반환값, 곧 호출 시점 EAX를 검사하고 있었습니다. 그 EAX를 probe가 정하지 않았고,
   직전 문장이 그 마커를 받아 왔습니다. Release의 MSVC가 그 값을 EAX에 남긴 채 호출하면 올바른 보존이
   실패가 됩니다. Debug는 EAX를 다른 값으로 써서 우연히 통과했습니다.

## 구현

* `RepiuStackBridgeProbeRefusedCall`: MSVC naked 함수(`mov eax, 0BADF00Dh; call Thunk; ret`)와 Linux
  `.S`의 같은 심볼.
* `ProbeBridgeContract`의 거절 검사: `refused == kRefusedEntryMarker`.

## 검증

| 검증 | 결과 |
|---|---|
| Win32 x86 Release `repiu_core_probe` | `stack_bridge_contract=true`, `stack_bridge_all=true`, `core_probe_all=true`, exit 0 (수정 전 `contract=false`, `core_probe_all=false`) |
| Win32 x86 Debug `repiu_core_probe` | `stack_bridge_all=true`, `core_probe_all=true` |
| Linux i386(WSL, Release 트리) `repiu_core_probe` | `stack_bridge_all=true`, `core_probe_all=true` |

로그: `build/task764/win32-release-core-probe.log`, `build/task764/wsl-probes.log`(저장소 밖).

## 재적용 검증 (2026-10-09, issue #8)

`059a08b`의 코드 변경(`stack_bridge_probe.cpp`·`.S`)을 main(v0.0.210) 위에 그대로 적용했고 충돌은
없었습니다.

| 검증 | 결과 |
|---|---|
| Win32 x86 Release `repiu_core_probe`(`build/win32_x86_debug`) | `stack_bridge_contract=true`, `stack_bridge_all=true`, `core_probe_failures=0`, exit 0 (적용 전 `contract=false`) |
| Linux i386(WSL, `build/linux_i386`) `repiu_core_probe` | `stack_bridge_all=true`, `core_probe_all=true` |

Win32 Debug는 다시 돌리지 않았습니다. 새 검사는 EAX를 직접 고정하므로 Debug의 codegen과 무관합니다.

## 남은 것

* 없음. Win32 frontier의 열린 항목 1은 닫혔습니다.

---

# English

> **History (2026-10-09, issue #8):** this was finished on 2026-10-01 as Task 765, but the branch
> holding its commit (`059a08b`), `docs/763-close-resolved-frontier-items`, disappeared unmerged, so
> the fix never reached main. The same failure was reported again on 2026-10-05 as issue #8, and the
> same fix was reapplied on main on 2026-10-09. The body is the Task 765 record.

# Issue #8 (Task 765) work log — fixing the stack bridge probe's Release failure

Design: [20261009-i008](../design/20261009-i008-stack-bridge-probe-release-refusal-check.md)
Work order: [20261009-i008](../work-orders/20261009-i008-stack-bridge-probe-release-refusal-check.md)

## Summary

`stack_bridge_contract=false` in the Win32 Release `repiu_core_probe` was not an engine defect; the
probe's check depended on the compiler's code generation. When the refusal path preserved EAX exactly
and so returned the previous call's marker, the "must not be the marker again" check read that as a
failure. An entry point now pins EAX to `0x0BADF00D` before calling the thunk, and the check became
"exactly that value comes back".

## How the cause was found

1. Rebuilding and running the Release probe failed only `contract` of the three stack bridge checks
   (`refusal_fallback` and `fault_on_host_stack` passed). `fault_handler_data_faults`, which the Linux
   frontier's section 6 had listed alongside, passes in Release now.
2. Reading `ProbeBridgeContract`: the second half's `refused != kResolverReturnMarker` was checking the
   return value of the refusal path (`pushad … popad; ret`), that is, EAX at the call, which the probe
   never set; the previous statement had just received that marker. With MSVC in Release leaving the
   value in EAX, correct preservation becomes a failure. Debug passed by chance with something else in
   EAX.

## Implementation

* `RepiuStackBridgeProbeRefusedCall`: an MSVC naked function (`mov eax, 0BADF00Dh; call Thunk; ret`)
  and the same symbol in the Linux `.S`.
* The refusal check in `ProbeBridgeContract`: `refused == kRefusedEntryMarker`.

## Verification

| Check | Result |
|---|---|
| Win32 x86 Release `repiu_core_probe` | `stack_bridge_contract=true`, `stack_bridge_all=true`, `core_probe_all=true`, exit 0 (before: `contract=false`, `core_probe_all=false`) |
| Win32 x86 Debug `repiu_core_probe` | `stack_bridge_all=true`, `core_probe_all=true` |
| Linux i386 (WSL, Release tree) `repiu_core_probe` | `stack_bridge_all=true`, `core_probe_all=true` |

Logs: `build/task764/win32-release-core-probe.log` and `build/task764/wsl-probes.log` (outside the
repository).

## Reapplied verification (2026-10-09, issue #8)

The code change of `059a08b` (`stack_bridge_probe.cpp` and `.S`) applied cleanly on main (v0.0.210).
Win32 x86 Release `repiu_core_probe`: `stack_bridge_contract=true`, `stack_bridge_all=true`,
`core_probe_failures=0`, exit 0 (before: `contract=false`). Linux i386 (WSL) `repiu_core_probe`:
`stack_bridge_all=true`, `core_probe_all=true`. Win32 Debug was not rerun; the new check pins EAX itself,
so Debug codegen no longer matters.

## Left

* Nothing. Open item 1 of the Win32 frontier is closed.
