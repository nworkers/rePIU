# Task 645 작업 로그: legacy 진입 출처와 연속 segment push 확인

설계: [20260909-645](../design/20260909-645-direct-edge-fallback-origin-trace.md)  
작업 지시: [20260909-645](../work-orders/20260909-645-direct-edge-fallback-origin-trace.md)  
분석: [linux-port-frontier 3.82](../analysis/linux-port-frontier.md)

## 한국어

기존 `REPIU_AOT_TRANSFER_TARGET_TRACE`를 cache breakpoint의 direct-edge,
address-map, block-fallthrough 역조회까지 확장했습니다. direct-edge와
block-fallthrough helper는 선택적으로 guest source도 반환하며, 관련 probe는
direct-edge target과 source를 함께 검사합니다. segment HLE trace에는 처리 직후
guest bytes 6개를 추가했습니다. 설정이 없으면 출력과 실행은 바뀌지 않습니다.

실제 실행에서 target `0x010F1D74`에 대한 breakpoint lookup 출력은 없었습니다.
대신 다음 연속 경계가 확인됐습니다.

```text
PUSH ES  0x010F9211: ESP 0x0158CC68 -> 0x0158CC64, ES=0x0024
next     0x010F9212: 0F A0 83 3D 98 66 ... (PUSH FS)
watch    0x010F1D74: ESP 0x0158CC64
```

guest-write trace의 `24000000` HLE event도 `0x0158CC64` writer를 확인합니다.
`PUSH FS`는 x64에서 fault 없이 host stack에 실행되므로 guest stack HLE를 벗어난
새 최초 불일치입니다.

### 검증

* Linux x64 `repiu` 빌드: 통과
* Linux x64 core probe: `25/25`, failures `0`
* 실제 target-filter 실행: direct-edge/address-map/fallthrough lookup 아님을 확인
* 실제 segment trace: `0x010F9212 PUSH FS`와 유지된 guest ESP 확인

게임은 아직 정상 실행되지 않습니다. 다음 작업은 처리된 HLE 뒤의 연속
HLE-sensitive 명령을 같은 예외 안에서 제한적으로 소진하여, `PUSH FS`가 host
stack에 실행되기 전에 guest stack 의미로 처리하는 것입니다.

## English

Extended the existing `REPIU_AOT_TRANSFER_TARGET_TRACE` through all cache
breakpoint reverse lookups: direct edge, address map, and block fallthrough.
The direct-edge and block-fallthrough helpers can optionally return their guest
source, and the related probe checks direct-edge target and source together.
Segment HLE trace now includes six guest bytes immediately after the handled
instruction. With diagnostics unset, output and execution are unchanged.

The real run produced no breakpoint-lookup record for target `0x010F1D74`.
Instead it established the consecutive boundary shown above. The guest-write
trace's `24000000` HLE event independently confirms the writer at
`0x0158CC64`. Since `PUSH FS` is valid on x64, it executes on host stack without
a fault and is the next first guest-stack mismatch.

### Verification

* Linux x64 `repiu` build: passed
* Linux x64 core probe: `25/25`, failures `0`
* Real target-filter run: excluded direct-edge/address-map/fallthrough lookup
* Real segment trace: confirmed `PUSH FS` at `0x010F9212` and unchanged guest ESP

The game still does not run normally. The next task is to drain a bounded run
of consecutive HLE-sensitive instructions in the same exception, before
`PUSH FS` can execute on the host stack.
