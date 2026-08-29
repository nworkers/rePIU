# Task 525 작업 로그 — 핸들러를 체인 끝으로

설계: [20260829-525](../design/20260829-525-breakpoint-chain-order.md) ·
작업 지시: [20260829-525](../work-orders/20260829-525-breakpoint-chain-order.md) ·
선행: [Task 522](20260829-522-guest-owned-breakpoint.md)

## 한 일

`HandleGuestOwnedBreakpoint` 호출을 `HandleAotIndirectTransfer` 뒤에서
**`HandleSingleStepTrace` 뒤, HLE 디코드 체인 앞**으로 옮겼습니다. AOT 블록 밖이므로
`aot_fault` 대신 `fault`를 넘깁니다(전자는 후자의 복사본).

주석 두 곳을 사실에 맞췄습니다.

## 무엇이 어긋나 있었는가

Task 522의 설계와 작업 지시는 "체인의 맨 끝"이라고 적었는데 코드는 그렇지 않았고,
그 자리의 주석은 **"after every handler that could own an INT3 has declined"**이라고
적혀 있었습니다. 뒤에 다섯 핸들러가 더 있었습니다 — `HandleAotConditionalTransfer`,
`HandleAotReturnTransfer`, `HandleGlideGateBoundary`,
`HandleTimerInterruptChainBoundary`, `HandleLinexeFarTransferBoundary`.

## 현행 결함은 아니었습니다

두 가지가 막고 있었습니다.

* 엔진이 심는 INT3는 전부 AOT 캐시 오프셋(`aot_code_cache.cpp` 13곳,
  `aot_page_coherence.cpp` 1곳) → `IsAotCacheAddress`가 걸러냄
* Glide 게이트는 `0xCC`가 아니라 `UD2`(`0F 0B`) → 바이트 검사가 걸러냄

**잠재 결함을 고친 것이지 증상을 고친 것이 아닙니다.**

## 놓칠 뻔한 것 — 센티널 검사는 여전히 일합니다

옮기고 나면 `HandleSingleStepTrace`가 앞서므로 센티널 검사가 중복처럼 보입니다. 아닙니다.
그 호출은 조건부입니다.

```cpp
fault.kind == kSingleStep ||
(context->aot_reentry_pending && fault.kind == kBreakpoint)
```

재진입 대기 없는 평범한 breakpoint로 센티널이 발화하면 그 호출을 건너뛰고 이 함수에
도달합니다. 검사를 남기고 주석에 이유를 적었습니다.

## 측정 — 이동은 성능 중립

WSLg, Release, vsync OFF, `pumpit1` 90초, 3회.

| | fps | 평균 |
|---|---|---:|
| 이동 전 (v0.0.172) | 34.11 · 35.22 · 36.96 | 35.43 |
| **이동 후** | 35.49 · 35.95 · 36.47 | **35.97** |

범위가 겹치므로 차이를 주장하지 않습니다. 프레임 생성 정상(3,127~3,212).

## 검증

* Windows x86 debug: 오류 0, 재링크 확인.
* Linux i386 (WSLg): 오류 0, 재링크 확인, 위 측정.
* **실제 Ubuntu VM에서는 확인하지 못했습니다** — 이 작업 중 SSH 접속이 끊겼습니다.
  Task 522의 게스트 fatal 경로 재현(트랩 1회 + 게스트 메시지)은 다음 VM 접속 때
  다시 확인해야 합니다.
