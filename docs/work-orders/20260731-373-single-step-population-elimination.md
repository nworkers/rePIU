# 작업 지시: single-step 모집단 폴백 사유 계측 / Work order: attribute the single-step fallback

Task 373 **1단계 — 계측만.** 설계:
[20260731-373](../design/20260731-373-single-step-population-elimination.md)

## 한국어

### 목표

single-step 예외가 **왜** 발생하는지를 사유별로 귀속한다. 제거 구현은 이 단계
결과가 사전 등록 gate를 넘긴 뒤에 별도 작업으로 한다.

### 사전 등록 gate

| 등급 | 기준 | 행동 |
|---|---|---|
| A | 제거 상한 wall 10% 이상 | 2단계 구현 진행 |
| B | 5 ~ 10% | 상위 1~2개 사유만 표적 |
| C | 5% 미만 | 구현하지 않고 축을 닫음 |

**미달 시 구현하지 않습니다.** Task 368의 규율을 그대로 따릅니다.

### 단계

1. **폴백 지점 열거**
   * `execution_trampoline.cpp`에서 trap flag를 세우는 모든 지점을 찾아 사유를
     명명한다(확인된 두 곳: HLE 경계 후 AOT 재진입 실패, 네이티브 진입 실패).
   * 각 지점에 사유 enum을 부여한다.

2. **사유별 계수 추가**
   * `ThreadContext`에 사유별 카운터.
   * 기존 `AotDbtDispatchFallbackReason`과 겹치면 **재사용**한다. 새 열거를 만들기
     전에 연결 가능성을 먼저 확인할 것.
   * **hot path에 clock read 추가 금지.** 카운터 증가만.

3. **연속 길이 분포**
   * `veh_single_step_run_length`가 이미 있으므로 히스토그램(1, 2~4, 5~16, 17~64,
     65+)으로 요약한다. 긴 연속과 짧은 연속은 대책이 다르다.

4. **기존 네이티브 경로와의 대조**
   * `native_fast_path` / `native_linear_span` / `REPIU_NATIVE_REGION` 카운터를 같은
     실행에서 함께 읽어, 이미 있는 경로가 이 모집단을 왜 못 잡는지 확인한다.

5. **요약 출력**
   * `Win32 single-step fallback reasons ...`
   * `Win32 single-step run length histogram ...`

6. **probe 추가**
   * 사유 귀속 누적, 히스토그램 경계, `nullptr` 무해.

7. **측정**
   * **`REPIU_GLIDE_SWAP_INTERVAL=0` 고정**, gameplay 장면 캡처.
   * **wall cycle과 프레임 수를 함께 기록**(1초 watchdog 조기 종료 대비).
   * 자동 장면은 보조로만 쓴다.

8. **판정 및 문서**
   * 상위 사유의 제거 상한을 wall 비중으로 환산해 gate와 대조.
   * 작업 로그, `docs/analysis/` 갱신.

### 완료 조건

* single-step 발생이 사유별로 100% 귀속됨(미분류 0).
* 연속 길이 분포가 출력됨.
* gate 대조 결과가 A/B/C 중 하나로 기록됨.
* probe 통과, 양 구성 빌드 성공, hot path clock read 추가 0회.

### 비범위

* 제거 구현은 이 작업에 포함하지 않는다.
* rendezvous 왕복 제거, Task 365 batch 2는 별건.

---

## English

Stage one is measurement only: attribute **why** single-step exceptions occur.
Implementation follows in a separate task and only if this stage clears the
pre-registered gate — above 10% of wall available, implement; 5 to 10%, target the
top one or two reasons; below 5%, close the axis. Task 368's discipline applies:
missing the gate means not implementing.

Enumerate every point that re-arms the trap flag and name its reason, then add
per-reason counters, reusing `AotDbtDispatchFallbackReason` if the paths line up
rather than inventing a parallel enumeration. Summarise `veh_single_step_run_length`
as a histogram, since long and short runs imply different remedies, and read the
existing `native_fast_path`, `native_linear_span`, and `REPIU_NATIVE_REGION` counters
in the same run to establish why machinery that already exists is not covering this
population. No clock read may enter the hot path — counter increments only. Add
summary lines and a probe, then measure on a gameplay capture with
`REPIU_GLIDE_SWAP_INTERVAL=0` pinned, recording wall cycles alongside frames because
of the one-second no-progress watchdog. Finish by converting the top reasons into a
share of wall, recording the gate verdict, and updating the analysis topics.

Done when single-step occurrences are fully attributed with nothing unclassified,
the run-length distribution is reported, the verdict is recorded, the probe passes,
both configurations build, and no clock read was added to the hot path.
