# Task 517 작업 로그 — direct dispatch는 돕니다. 그런데도 트랩이 붙습니다

설계: [20260829-517](../design/20260829-517-direct-dispatch-census.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260829-516](20260829-516-aot-boundary-linking.md)

## 결과 — 516의 함의를 반증합니다

```
[repiu-live-gdd] #8 patched=172 verified=172 entry=338929 success=338928 miss=0 terminal=0 per_reentry=0.8699
```

**Linux에서 Glide direct dispatch는 정상 작동합니다.** 사이트 **172곳** 패치(Windows와 같은
수), 검증 172, 진입 338,929, 성공 338,928, target-miss 0, terminal 0. 재진입의 **87%**가
이 경로로 갑니다.

516은 `bp/reentry = 1.010`을 보고 "이 경로가 안 도는 것"이라고 함의했습니다. **틀렸습니다.**

## 그런데 산술이 더 강한 것을 말합니다

| | gdd_entry | reentry | bp | gdd/reentry | bp/reentry |
|---|---:|---:|---:|---:|---:|
| Linux (3회) | 338,929 ~ 351,941 | 389,614 ~ 403,468 | 392,670 ~ 406,445 | **0.870** | **1.008** |
| Windows | 8,719,788 | 9,171,551 | 396,073 | 0.951 | 0.043 |

**Windows**: 트랩 396,073은 **비-gdd 재진입 451,763의 88%**입니다. 즉 direct dispatch로 간
재진입은 트랩을 내지 않습니다.

**Linux**: 비-gdd 재진입이 **50,685뿐인데 트랩이 392,670**입니다 — 그것을 **7.7배
초과**합니다. 트랩이 비-gdd 재진입 수를 넘을 수 있는 경우는 하나뿐입니다.

**성공한 direct dispatch도 Linux에서는 트랩을 냅니다.**

세 실행에서 `bp/reentry` 1.0078 · 1.0079 · 1.0074, `gdd/reentry` 0.8699 · 0.8713 · 0.8723으로
재현됩니다.

## thunk의 `int3`은 출처가 아닙니다

Linux GAS thunk 다섯 중 이것만 매크로 마지막 인자가 `trap`이라 의심했지만, 그 인자 이름은
`refusal`이고 주석이 "the site has nowhere to continue"라고 적어 둡니다 — **거절 경로**입니다.
그리고 진입 338,929 중 실패는 **1건**입니다. 거절이 트랩 39만 개의 출처일 수 없습니다.

## 다음 — 성공한 direct dispatch가 왜 트랩을 치르는가

축이 한 단계 더 좁혀졌습니다. 이제 질문은 **패치된 사이트에 어떻게 도착하는가**입니다.
Windows는 캐시에서 패치된 지점으로 직접 뜁니다. Linux는 같은 패치를 하고 같은 thunk를 쓰는데
그 앞에 트랩이 하나 더 있습니다.

볼 자리는 **패치가 무엇을 써 넣는가**입니다. Windows 요약에는
`resolved-target/relinked-cache: 148/574`가 함께 나오는데 517의 줄은 그 둘을 싣지 않았습니다 —
다음 단위가 그것을 더해 **Linux에서 relink가 이루어지는지**를 봅니다. relink가 0이면 사이트는
패치됐어도 캐시가 그 지점으로 직접 뛰지 못하고 트랩을 거쳐 도착하는 것입니다.

## 방법에 대해

이번에도 **함의를 측정으로 바꾼 것이 결론을 뒤집었습니다.** 516의 함의는 그럴듯했고
(1.010이면 안 도는 것처럼 보입니다) 틀렸습니다. 이 세션에서 같은 일이 세 번째입니다 —
515의 `native_fast_path` 카운터, 516의 `residency_mean=0`, 그리고 이번.
