# Task 516 — AOT 캐시가 Linux에서 얼마나 이어 붙이는가

작업 지시: [20260829-516](../work-orders/20260829-516-aot-boundary-linking.md) ·
작업 로그: [20260829-516](../work-logs/20260829-516-aot-boundary-linking.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260828-515](20260828-515-boundary-class-excess.md)

## 배경

Task 515가 초과 경계를 breakpoint로 이름 붙였습니다 — **프레임당 167.80개 대 Windows 6.94개,
24.2배**. 그리고 후보 둘(`native_fast_path`, direct-return table)을 대조군으로 지웠습니다.

breakpoint는 AOT 엔진이 경계에 심는 INT3입니다. **그러니 질문은 하나로 좁혀집니다 — 왜 Linux의
캐시는 더 자주 경계에 닿는가.**

## 확인됨 — 답할 카운터가 이미 다 있습니다

세 번째로 같은 자리입니다. 새로 세는 것이 없고, `main.cpp` 요약으로만 나가 **Linux에서 읽지
못할 뿐**입니다.

| 카운터 | 무엇을 말하나 | 넣은 작업 |
|---|---|---|
| `aot_cache_entry_count` | 캐시로 진입한 횟수 | — |
| `aot_boundary_count` | 경계로 나온 횟수 | — |
| **사유별 다섯** (`return`·`indirect`·`direct`·`conditional`·`other`) | **어떤 게스트 명령이 나가게 했나.** 다섯의 합이 `aot_boundary_count` | Task 262 |
| `aot_residency_instruction_total` / `aot_residency_sample_count` | **진입에서 첫 제어 전이까지 직선 명령 수** | Task 263(b) |
| `aot_reentry_count` · `aot_legacy_fallback_count` | 재진입과 legacy 낙하 | — |

전부 게스트 스레드가 쓰는 atomic입니다. **읽는 쪽도 게스트 스레드**이므로 512·515와 같은
자리에 같은 방식으로 실을 수 있습니다.

## 무엇을 읽게 되는가

두 수가 축을 가릅니다.

**하나. residency 평균.** 진입 한 번에 게스트 명령을 몇 개나 직선으로 도는가. Linux가 훨씬
작으면 **캐시가 더 잘게 쪼개져 있다**는 뜻이고, 그러면 원인은 번역 계획이나 direct edge
연결입니다.

**둘. 경계 사유 분포.** 다섯 중 어디가 부풀었는가.

| 부푼 사유 | 뜻 |
|---|---|
| `return` | 반환이 캐시 안에서 해결되지 못함 |
| `indirect` | inline cache가 안 맞음 |
| `direct` | **direct edge가 연결되지 않음** — 번역된 이웃 블록으로 못 뛰고 매번 나옴 |
| `conditional` | 조건 분기의 양쪽이 이어지지 않음 |
| `other` | 비전이 명령에서의 정지 — 번역 실패 계열 |

**`direct`가 부풀어 있으면 그것이 답입니다.** 그 경우 Linux의 캐시는 블록을 번역해 놓고도
서로 잇지 못하고 있는 것이고, 이어 붙이기는 W^X와 명령 캐시 flush가 걸린 자리입니다 —
Task 506이 Linux에서 새로 만든 유일한 것이 명령 캐시 flush 계층입니다.

## 결정 — 세 번째 줄

`[repiu-live-aot]` 한 줄을 더합니다. 512가 `[repiu-live-veh]`를 더한 것과 같은 이유로 기존
줄에 합치지 않습니다.

```
[repiu-live-aot] #N entry= boundary= per_frame= residency_mean= ret= ind= dir= cond= oth= reentry= fallback=
```

**다섯 사유의 합이 `boundary`와 맞는지 먼저 봅니다.** 맞지 않으면 그 자체가 소견이고, 해석은
그 다음입니다.

## 이 작업이 하지 않는 것

* **고치지 않습니다.** 원인을 이름 붙이는 데까지입니다.
* `other`(접근 위반) 10.7배는 별도입니다 — 이 작업은 breakpoint 쪽을 봅니다.
* 핸들러 본문 3.3배도 별도입니다.
