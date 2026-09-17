# Task 696 작업 로그 — Linux x64 fatal breakpoint AOT 재진입

## 결과

`AotHleResumeOrigin`을 추가해 기존 pending/legacy 재진입과 dispatcher가 이미
처리한 guest boundary 재진입을 구분했습니다. 일반 호출자의 상태 gate는 유지하고,
Linux x64 원본 fatal breakpoint가 EIP를 진행시킨 경우에만 handled-boundary로
`TryResumeAotAfterHandledHle`를 호출합니다. 이후 arena, quarantine, cache lookup 또는
translation, span safety 검사는 기존 경로를 그대로 사용합니다.

재진입이 실패한 x64 continuation이 long-mode 비동일 명령이면 fault를 넘겨 raw guest
실행을 막습니다. i386과 AOT 미사용 fatal-tail 경로는 변경하지 않았습니다.

## 검증

* WSL Ubuntu 24.04 Linux x64 Debug `repiu_core_probe`: 27/27 통과
* Linux x64 Debug `repiu`: 빌드 성공
* 실제 `pumpit2a` bounded 실행:

```text
[repiu-hle-reentry] stage=entry handled=0x010EFEB8 current=0x010EFEB9 detail=handled-boundary
[repiu-hle-reentry] stage=cache-hit-span-safe cache_target=0x20001806 detail=resume-candidate
[repiu-hle-reentry] stage=resumed cache_target=0x20001806 detail=resume
```

기존 raw guest `0x010F0D96` RSP 손상은 재발하지 않았습니다. 실행은 더 뒤의 guest
`0x010F777C`에 대응하는 cache `0x200695A3` SIGTRAP에서 새로 중단되었습니다. 이는
fatal-tail 재진입 수정과 분리해 다음 작업에서 boundary provenance를 조사합니다.

## English

### Result

Added `AotHleResumeOrigin` to distinguish existing pending/legacy re-entry from
a guest boundary already handled by the dispatcher. Existing callers retain
their state gate. Only a Linux x64 original fatal breakpoint that advances EIP
calls `TryResumeAotAfterHandledHle` with handled-boundary origin. The existing
arena, quarantine, cache lookup or translation, and span-safety checks remain
shared.

If x64 re-entry fails and the continuation is not long-mode identical, the
fault is passed through instead of running raw guest code. i386 and non-AOT
fatal-tail paths are unchanged.

### Verification

* WSL Ubuntu 24.04 Linux x64 Debug `repiu_core_probe`: 27/27 passed
* Linux x64 Debug `repiu`: build passed
* Real bounded `pumpit2a` run:

```text
[repiu-hle-reentry] stage=entry handled=0x010EFEB8 current=0x010EFEB9 detail=handled-boundary
[repiu-hle-reentry] stage=cache-hit-span-safe cache_target=0x20001806 detail=resume-candidate
[repiu-hle-reentry] stage=resumed cache_target=0x20001806 detail=resume
```

The former raw guest `0x010F0D96` RSP corruption did not recur. Execution
stopped later at a new cache SIGTRAP at `0x200695A3`, corresponding to guest
`0x010F777C`. Its boundary provenance is deliberately left for the next task,
separate from the fatal-tail re-entry fix.
