# Task 517 — Linux에서 direct dispatch가 도는가

작업 지시: 이 문서에 포함 · 작업 로그: [20260829-517](../work-logs/20260829-517-direct-dispatch-census.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260829-516](20260829-516-aot-boundary-linking.md)

## 배경

Task 516이 축을 좁혔습니다 — Linux는 캐시 재진입마다 트랩을 하나 내고(1.010) Windows는 23번에
한 번(0.0432)입니다. 그리고 Windows가 트랩을 피하는 경로를 이름 붙였습니다: **Glide direct
dispatch**가 재진입의 95.1%를 처리합니다.

516은 거기서 **함의**를 하나 남겼습니다 — Linux의 1.010은 그 경로가 안 도는 것처럼 보입니다.
**함의는 측정이 아닙니다.** 517이 그것을 잽니다.

## 결정 — 네 번째 줄

`ReadWin32GlideGateDirectDispatchStats()`는 프로세스 전역이고 `main.cpp` 요약으로만 나갑니다.
`[repiu-live-gdd]` 한 줄로 내보냅니다 — 512·515·516과 같은 모양이고, **새로 세는 것은
없습니다.**

읽을 것은 세 갈래입니다.

| 결과 | 뜻 |
|---|---|
| `patched=0` | 패치 자체가 거부됨 |
| `patched>0`, `entry=0` | 패치는 됐는데 그 경로로 들어가지 않음 |
| `entry`가 재진입에 비례 | **경로는 돈다** — 그러면 트랩의 출처가 다른 곳 |

## 하지 않는 것

* 고치지 않습니다.
* `other`(접근 위반) 10.7배, 핸들러 본문 3.3배는 별도입니다.
