# Task 515 — 초과 경계의 종류

작업 지시: [20260828-515](../work-orders/20260828-515-boundary-class-excess.md) ·
작업 로그: [20260828-515](../work-logs/20260828-515-boundary-class-excess.md) ·
frontier: [linux-port-frontier](../analysis/linux-port-frontier.md) ·
선행: [20260828-512](20260828-512-signal-delivery-count-or-price.md)

## 배경

Task 512가 `veh` 42.6배를 갈랐습니다 — **프레임당 경계 13.6배 × 핸들러 본문 3.3배**이고,
커널 전달 경로는 Linux가 오히려 0.44배로 **쌉니다.** 지배 인자는 **횟수**입니다.

512는 그 초과분에 대해 한 가지를 이미 배제했습니다: **single-step이 아닙니다.**
`gap_ss_count`가 Linux 배달의 **3.6%**인데 Windows는 **24.4%**입니다.

남은 것은 breakpoint와 other이고, **그 둘의 칸이 이미 채워지고 있습니다.**

## 결정 — 나머지 두 칸을 찍습니다. 그게 전부입니다

`Win32ExecutionTimeProfile::veh_gap_counts`와 `veh_gap_cycles`는 세 칸
(`kSingleStep`·`kBreakpoint`·`kOther`)짜리 배열이고 양쪽 호스트에서 채워집니다. 512가 그중
single-step 하나만 실었습니다.

**새로 세는 것이 없습니다.** 512와 같은 모양의 변경이고, 그것이 이 작업을 작게 만드는 이유입니다.

## 무엇을 읽게 되는가

Windows 기준값은 이미 있습니다(512의 대조 실행).

| 클래스 | Windows 배달 중 비율 |
|---|---:|
| single-step | 23.9% |
| breakpoint | 38.7% |
| other | 37.4% |

Linux의 single-step은 3.6%로 확인됐고, 나머지 96.4%가 두 칸 중 어디로 가는지가 이 작업의
답입니다.

| 결과 | 뜻 |
|---|---|
| **breakpoint**가 지배 | 엔진이 심은 INT3을 훨씬 자주 밟습니다 — 경계를 만드는 정책 쪽 |
| **other**가 지배 | 접근 위반 계열입니다 — 페이지 보호·포트 I/O·write watch 쪽 |

**두 답은 다음 작업이 완전히 달라집니다.** 그래서 고치기 전에 이것부터 봅니다.

## 유력 후보와, 이 측정이 그것을 어떻게 시험하는가

frontier 6절이 적어 둔 것 — Linux 사용자 공간이 하드웨어 디버그 레지스터를 쓸 수 없어
`native_fast_path`·`native_region`·`native_linear_span` **셋이 모두 차단**되어 있습니다.
그 셋은 정확히 **트랩을 피하려고** 있는 경로입니다.

**그것이 원인이라면 breakpoint 쪽이 커야 합니다** — 그 경로들이 없애 주는 것이 복귀
브레이크포인트이기 때문입니다. other가 크면 후보가 틀린 것이고, 그것도 소득입니다.

## 이 작업이 하지 않는 것

* **고치지 않습니다.** 차단된 세 경로를 켜 보는 것조차 하지 않습니다 — Linux에서 그 셋은
  하드웨어가 없어 막힌 것이고, 켜면 3d-23이 겪은 9초 정지로 돌아갑니다.
* 핸들러 본문 3.3배는 별도입니다(하위 버킷이 가릅니다).
* `dos`의 433.9배도 별도입니다.
