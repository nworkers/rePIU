# Task 516 작업 로그 — 재진입마다 트랩이 하나씩 붙습니다

설계: [20260829-516](../design/20260829-516-aot-boundary-linking.md) ·
작업 지시: [20260829-516](../work-orders/20260829-516-aot-boundary-linking.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
측정 절차: [execution-frame-rate-measurement](../guides/execution-frame-rate-measurement.md)

## 결과 — 재진입 비율은 같습니다. 트랩이 필요한지가 다릅니다

설계는 `boundary`가 부풀어 있을 것이라 보고 사유별 다섯을 준비했습니다. **틀렸습니다** —
`boundary`는 프레임당 13.78개로 Windows(6.69)의 2.1배뿐이고, 515가 센 breakpoint는 프레임당
167.80개입니다. **경계는 트랩의 출처가 아니었습니다.**

출처는 **재진입**입니다.

| | Windows | Linux | 배율 |
|---|---:|---:|---:|
| 프레임당 boundary | 6.69 | 13.78 | 2.1x |
| 프레임당 reentry | 160.63 | ~154 | **0.90x** |
| **reentry당 breakpoint** | **0.0432** | **1.010** | **23x** |

**Linux는 캐시 재진입마다 트랩을 정확히 하나 냅니다.** 세 실행에서 1.009 · 1.010 · 1.010으로
소수점 세 자리까지 재현됩니다. Windows는 23번에 한 번입니다.

**재진입 자체는 Linux가 오히려 10% 적습니다.** 그러므로 원인은 "더 자주 나온다"가 아니라
**"나올 때마다 트랩을 낸다"**입니다. 그리고 이 23배가 515의 breakpoint 24.2배와 그대로
맞습니다 — 분해가 닫힙니다.

## 트랩을 피하는 경로의 이름

Windows 요약이 그것을 말합니다.

```
Win32 Glide direct dispatch patched/verified/resolved-target/relinked-cache/entry/success/...:
    172/172/148/574/8719788/8719787/0/0
Win32 AOT entry/boundary/reentry/fallback: 1/382237/9171551/0
```

**진입 8,719,788회는 전체 재진입 9,171,551회의 95.1%이고, 실패는 1건입니다.** Windows가
트랩 없이 처리하는 96%가 정확히 이 경로입니다 — Glide 게이트 사이트 172곳을 패치해 캐시에서
호스트로 직접 뛰는 것.

**그 thunk는 Linux에도 있습니다.** Task 503d-12가 GAS로 옮긴 다섯 중 하나이고
(`AotDbtGlideGateDispatchThunk`), 활성화 술어의 기본값은 양쪽 다 켜짐입니다
(`ResolveWin32GlideGateDirectDispatchEnabled(nullptr) == true`).

**그런데 Linux에서 그것이 실제로 패치하는지는 측정되지 않았습니다.** 그 카운터는
`ReadWin32GlideGateDirectDispatchStats()`로 나가고, 그것을 찍는 곳은 `main.cpp` 요약뿐입니다 —
**네 번째로 같은 벽입니다.**

`bp/reentry = 1.010`이 "거의 작동하지 않는다"를 **함의**하지만, 함의는 측정이 아닙니다. 이
세션이 세운 규칙이 그것이고(515에서 카운터를 보지 않았으면 약한 결론에 그쳤습니다), 다음
작업이 그 카운터를 읽습니다.

## 설계가 틀린 자리와, 그것이 값이었던 이유

설계는 `direct`가 부풀어 있으면 direct edge 연결 실패라고 적었습니다. 실제 분포는 이렇습니다.

```
ret=0 ind=172 dir=0 cond=0 oth=31894   (sum_ok=1)
```

`other`가 99.5%이고 `direct`는 0입니다. **예상한 다섯 갈래 중 어느 것도 아니었고**, 그
때문에 축이 boundary가 아니라 reentry라는 것이 드러났습니다. 사유 분포를 찍지 않았다면
`boundary`가 작다는 사실만 보고 막혔을 것입니다.

## 계측의 구멍 — `residency_mean=0`

`residency_mean`이 세 실행 모두 **0**입니다. 이것을 "직선 명령이 0개"로 읽으면 안 됩니다 —
`aot_residency_sample_count`가 이 경로에서 **채워지지 않는다**는 뜻입니다.

**값이 채워졌는지 먼저 보라**는 절차가 세 번째로 값을 했습니다. 0을 소견으로 읽었다면 캐시가
아무것도 실행하지 않는다는 결론에 갔을 것입니다.

## 구현

* `[repiu-live-aot]` 세 번째 줄. entry/boundary/사유 다섯/reentry/fallback/residency와,
  **다섯의 합이 boundary와 맞는지를 줄 안에 찍는 `sum_ok`**.
* 카운터는 `ThreadContext`가 아니라 **`Win32LiveAotCounters` 평범한 구조체**로 넘깁니다.
  보고기가 실행 엔진 타입에 의존하면 계층이 역전됩니다.
* 읽기는 relaxed load입니다 — 쓰는 쪽이 같은 스레드입니다.

## 검증

| 항목 | 결과 |
|---|---|
| Linux Release 빌드 | 성공 (첫 시도는 문자열 리터럴 안 `\n`이 실제 줄바꿈이 되어 실패, 수정) |
| **`sum_ok`** | 세 실행 모두 **1** |
| 재현 | `bp/reentry` 1.009 · 1.010 · 1.010 |
| Windows 대조 | 기존 요약 로그에서 그대로 (재실행 불필요) |

## 다음

**Linux에서 Glide direct dispatch가 패치하는지 세는 것**입니다. `patched/verified/entry/
success/target-miss/terminal` — 지금은 `main.cpp` 요약으로만 나갑니다. 512·515·516과 같은
모양으로 live 줄에 실으면 됩니다.

`other`(접근 위반) 10.7배와 핸들러 본문 3.3배는 여전히 별도로 남습니다.
