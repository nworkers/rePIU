# Task 525 — 게스트 소유 INT3 핸들러를 체인 끝으로

작업 지시: [20260829-525](../work-orders/20260829-525-breakpoint-chain-order.md) ·
작업 로그: [20260829-525](../work-logs/20260829-525-breakpoint-chain-order.md) ·
선행: [Task 522](20260829-522-guest-owned-breakpoint.md)

## 배경 — 문서와 코드가 어긋나 있었습니다

[Task 522](20260829-522-guest-owned-breakpoint.md)의 설계와 작업 지시는 둘 다
**"체인의 맨 끝에 두십시오"**라고 적었습니다. 작업 지시는 이유까지 적었습니다 —
앞에 두면 엔진이 심은 INT3를 가로채 그 INT3가 존재하는 이유인 작업이 사라진다고.

그런데 코드는 `HandleAotIndirectTransfer` 바로 뒤에 있었고, **그 뒤에 다섯 핸들러가 더**
있었습니다.

| 뒤에 있던 핸들러 |
|---|
| `HandleAotConditionalTransfer` |
| `HandleAotReturnTransfer` |
| `HandleGlideGateBoundary` |
| `HandleTimerInterruptChainBoundary` |
| `HandleLinexeFarTransferBoundary` |

그 자리의 주석은 **"after every handler that could own an INT3 has declined"**이라고
적혀 있었습니다. 사실이 아니었습니다.

## 지금은 왜 터지지 않았는가

우연이 아니라 두 가지가 막아 주고 있었습니다.

* 엔진이 심는 INT3는 **전부 AOT 코드 캐시 오프셋**입니다(`aot_code_cache.cpp` 13곳,
  `aot_page_coherence.cpp` 1곳). `IsAotCacheAddress`가 걸러냅니다.
* Glide 게이트가 쓰는 것은 `0xCC`가 아니라 **`UD2`(`0F 0B`)**입니다. 바이트 검사가 걸러냅니다.

**그래서 현재 동작은 옳았고, 성능도 문제가 아니었습니다** — [Task 524](../work-logs/20260829-524-wslg-baseline-remeasure.md)에서
측정으로 확인했습니다. 이것은 잠재 결함이지 현행 결함이 아닙니다.

## 그래도 고치는 이유

**한 줄의 거짓 주석이 다음 사람의 판단을 오염시킵니다.** "모든 핸들러가 거절한 뒤"라고 적힌
자리를 읽은 사람은 그 불변식을 믿고 코드를 짭니다. 게스트 이미지 주소에 `0xCC`를 심는
핸들러가 나중에 그 아래에 추가되면, 이 함수가 조용히 가로채고 증상은 한참 뒤 엉뚱한 곳에서
나타납니다 — 이번 세션이 내내 다룬 실패 방식입니다.

## 결정 — 진짜 마지막 자리

`HandleSingleStepTrace` **뒤**, HLE 디코드 체인 **앞**.

두 경계 모두 의미가 있습니다.

* **뒤여야 하는 이유**: `HandleSingleStepTrace`가 추적 센티널을 소유합니다.
* **앞이어야 하는 이유**: 그 아래는 `Eip`의 명령을 디코드합니다. 게스트가 놓은 `0xCC`는
  거기서 "지원하지 않는 명령"으로 보고되고 실행이 끝납니다. **소유권을 주장할 마지막
  기회입니다.**

`aot_fault`는 `fault`의 복사본이므로 AOT 블록 밖에서 `fault`를 그대로 넘깁니다.

## 센티널 검사는 남깁니다

옮기고 나면 중복처럼 보이지만 아닙니다. `HandleSingleStepTrace`는 조건부로만 불립니다.

```cpp
fault.kind == kSingleStep ||
(context->aot_reentry_pending && fault.kind == kBreakpoint)
```

**센티널이 재진입 대기 없는 평범한 breakpoint로 발화하면 그 호출을 건너뛰고 이 함수에
도달합니다.** 검사는 여전히 일을 합니다. 주석을 그렇게 고칩니다.

## 검증

* 양쪽 호스트 빌드.
* WSLg 3회 측정으로 동작·성능 확인 (이동은 중립이어야 합니다).
* VM은 이번에 접속 불가라 실제 Ubuntu 재확인은 하지 못했습니다.
