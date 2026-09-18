# Task 703 작업 로그 — Linux x64 타이머 체인 경계 AOT 재진입

## 결과

`HandleTimerInterruptChainBoundary`가 EIP를 진행시킨 Linux x64 AOT 경로에 공용
handled-boundary 재진입을 연결했습니다. cache miss는 기존 동적 번역 정책으로
해결하며, 재진입 실패 후 long-mode semantics가 다른 continuation은 fail closed합니다.
i386, 비-AOT 경로, 타이머 체인의 selector 판정과 EFLAGS 정리는 변경하지 않았습니다.

## 원인 증거

수정 전 추적은 다음을 보였습니다.

* direct-call writer `0x0103F12D`가 `0x0158CBAC`에 `0x0103F132`를 기록
* 타이머 체인 경계 진입 ESP `0x0158CBAC`, 종료 ESP `0x0158CBB0`
* 경계 종료 EIP `0x0103F139`
* `CLI` 진입 ESP가 그대로 `0x0158CBB0`
* `0x010F2773`의 `RET`가 `0x0158CBB0`의 `0x00000080`을 소비

이는 타이머 handler의 4-byte 정리가 아니라, 경계 뒤 direct `CALL`의 4-byte guest
return push가 사라진 문제입니다.

## 검증

* Linux x64 Debug `repiu` 빌드 통과
* Linux x64 Debug `repiu_core_probe`: 27/27 통과
* 실제 `pumpit2a` focused trace:

```text
[repiu-hle-reentry] stage=entry handled=0x0103F133 current=0x0103F139 guest_esp=0x0158CBB0
[repiu-hle-reentry] stage=translation-success cache_target=0x202A4571
[repiu-hle-reentry] stage=resumed current=0x0103F139 guest_esp=0x0158CBB0
[repiu-aot-transfer-target] kind=return source=0x010F2773 target=0x0103F13E esp=0x0158CBB0 consumed=0x0158CBAC stack_target=0x0103F13E
```

기존 target `0x00000080`과 return-thunk unresolved SIGTRAP은 재현되지 않았습니다.
실행은 이후 guest `0x0103F1F5`의 `IRETD` 경계에서 새롭게 정지했습니다. 보고 RIP
`0x202B4F0C`는 cache `INT3` 다음 주소이고 실제 breakpoint는 `0x202B4F0B`입니다.
보고 RIP의 `FB`는 다음 cache byte이지 guest opcode가 아닙니다. 이 IRETD boundary는
다음 작업 범위입니다.

---

## English

### Result

The Linux x64 AOT path now invokes shared handled-boundary reentry when
`HandleTimerInterruptChainBoundary` advances EIP. Cache misses use the existing
dynamic translation policy, while a failed non-identical continuation remains
fail-closed. i386, non-AOT execution, selector classification, and timer-chain
EFLAGS cleanup are unchanged.

### Cause evidence

Before the fix, the timer boundary correctly restored ESP from `0x0158CBAC` to
`0x0158CBB0` and continued at `0x0103F139`. The following direct call executed
as original long-mode bytes, so its four-byte guest return push was absent.
`RET` at `0x010F2773` consequently consumed `0x00000080`.

### Verification

Linux x64 Debug `repiu` built successfully and all 27 core-probe groups passed.
A real focused run dynamically translated continuation `0x0103F139` to
`0x202A4571`, resumed it, and recorded `0x0103F13E` as the return target
consumed from guest stack address `0x0158CBAC`. The old `0x00000080` unresolved
return did not recur. The next stop is the `IRETD` boundary for guest
`0x0103F1F5`. Reported host RIP `0x202B4F0C` follows the actual cache `INT3` at
`0x202B4F0B`; the `FB` at the reported RIP is the next cache byte rather than
the guest opcode.
