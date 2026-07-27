# 20260728-330 작업 로그: plan build 귀속과 Debug 왜곡 분리 / Work log

설계: [20260728-330-plan-build-attribution.md](../design/20260728-330-plan-build-attribution.md)

작업 지시: [20260728-330-plan-build-attribution.md](../work-orders/20260728-330-plan-build-attribution.md)

## 한국어

### 결론 요약

**gate B1이 성립했습니다. `plan_build`는 Debug 왜곡이 지배합니다.** 같은 코드·같은
입력으로 명령당 `24,512 tick`(Debug) 대 `2,162 tick`(Release), 비율 **1/11.34**입니다.

그리고 더 중요한 결과가 나왔습니다. **단계 순위가 두 구성에서 뒤집힙니다.**

| 단계 | Debug 비중 | Release 비중 | 명령당 Debug 계수 |
|---|---:|---:|---:|
| `decode` | 10.37% | **44.02%** | 2.67배 |
| `classify` | **40.71%** | 16.07% | 28.7배 |
| `walk` | 24.52% | 18.81% | 14.8배 |
| `record_build` | 11.44% | 8.92% | 14.6배 |
| `sweep` | 0.68% | 0.68% | 11.2배 |
| `decoder_init` | 0.00% | 0.00% | — |
| residual | 12.28% | 11.51% | — |

즉 **Debug만 보고 최적화 대상을 골랐다면 틀린 곳을 골랐을 것입니다.** Debug에서는
`classify`+`walk`가 65.23%로 보이지만, Release에서는 `decode`가 44.02%로 최대입니다.
Debug 계수가 단계마다 2.67배에서 28.7배까지 다르기 때문입니다. C로 작성된 Zydis decode는
덜 부풀고, 작은 술어 함수와 STL 컨테이너가 얽힌 `classify`는 가장 크게 부풉니다.

**부수 확인:** 오래 유지돼 온 전제 "명령당 32us(→10.2us)는 Zydis decode치고 크다"는
**전제부터 틀렸습니다.** Debug 기준 decode는 `plan_build`의 10.37%뿐입니다.

### 사전 등록 gate 판정

| gate | 조건 | 관측 | 판정 |
|---|---|---:|---|
| A1 | Debug `decode` >= 50% | 10.37% | 기각 |
| A2 | Debug `record_build` >= 30% | 11.44% | 기각 |
| A3 | Debug `sweep` >= 20% | 0.68% | 기각 |
| A4 | 어느 단계도 30% 미만 | `classify` 40.71% | 기각 |
| **B1** | **Release/Debug <= 1/10** | **1/11.34** | **성립** |
| B2 | Release/Debug >= 1/3 | 1/11.34 | 기각 |

**사전 등록의 공백을 기록합니다.** Part A gate 네 개가 모두 기각됐습니다. 설계가
`classify`가 지배하는 경우를 gate로 넣지 않았기 때문이며, 이는 측정 실패가 아니라
**gate 설계의 누락**입니다. 관측값은 그대로 유효합니다.

### F5 해소 — sweep 패스 수

지금까지 미측정이던 jump-table sweep의 패스 수가 확인됐습니다. **패스 1회,
record 방문 47,692회**(= 명령 수)로, 재분류에 의한 추가 순회는 이 entry에서 발생하지
않았습니다. 비용도 두 구성 모두 0.68%입니다. **sweep 재순회 구조는 문제가 아닙니다.**

### 측정 값

probe는 실제 PIU 이미지 entry로 plan을 5회 빌드하고 최소 표본을 보고합니다.
두 구성 모두 명령 47,692개, 블록 11,599개로 동일한 plan을 만들었습니다.

| 항목 | Debug | Release |
|---|---:|---:|
| 최소 총 tick | 1,169,034,997 | 103,112,986 |
| 명령당 tick | **24,512** | **2,162** |
| decode 명령당 | 2,543 | 951 |
| record_build 명령당 | 2,804 | 192 |

**대표성 확인:** 실게임 60초에서 관측된 명령당 `25,433 tick`과 probe의 Debug
`24,512 tick`은 3.6% 차이입니다. 즉 이 벤치마크는 in-situ 비용을 잘 대표합니다.
(그래서 사용자 확인 아래 게임 실행 없이 마무리했습니다.)

Release 기준 절대값은 명령당 약 `2,162 tick`(2.5GHz 기준 약 **865ns**)이고, 번역 1회
(명령 1,058개)로 환산하면 약 `2.3M tick`, 약 **0.9ms**입니다. Debug에서 관측된 회당
`26.9M tick`의 약 1/11입니다.

### 검증 결과

1. Win32 x86 Debug 전체 빌드 통과.
2. `cmake --build build/win32_x86_debug --config Release --target repiu_aot_probe` 통과.
3. `repiu_aot_probe`가 **두 구성 모두 exit 0**, 신규 `plan_build_bench_*` 포함 전 그룹
   통과. `plan_build_bench_plan_unchanged=true`로 계측 ON/OFF의 plan 동일성을,
   `arena_view_all=true`로 Task 329의 동등성을 함께 확인했습니다.
4. `plan_build_bench_clamped_samples=0` — 역행 TSC 표본 없음.

### 확인됨 / Confirmed

* `plan_build`의 Debug 계수는 **11.34배**이며, 단계별로 2.67~28.7배로 **균일하지
  않습니다.**
* 따라서 **Debug에서 얻은 "어느 단계가 지배하는가"류 결론은 Release에서 뒤집힐 수
  있습니다.** 알고리즘 복잡도(예: Task 323의 O(n) 선형 탐색)나 대역폭·syscall
  비용(Task 329의 스냅샷)처럼 구성과 무관한 결론은 영향받지 않습니다.
* jump-table sweep은 이 entry에서 1패스이며 비용은 0.68%입니다.

### 미확정 / Unresolved

* **게임을 Release로 구동할 수 있는지 확인하지 않았습니다.** 성능 기준을 Release로
  옮기는 것이 이 작업의 자연스러운 후속이지만, 타이밍이 바뀌면 새로운 경쟁 상태가
  드러날 수 있으므로 별도 Task로 다뤄야 합니다.
* residual 11.5~12.3%는 대부분 계측 경계(명령당 timer 4회)의 비용으로 보이나 분리
  측정하지 않았습니다. 두 구성에 같은 방식으로 들어가므로 비율 판정에는 영향이
  작습니다.
* Release 기준 `decode` 명령당 951 tick(약 380ns)이 Zydis 통상값보다 큰지 여부는
  operand 배열 zero-init과 timestamp 비용을 분리하지 않아 확정할 수 없습니다.

---

## English

### Summary

Gate B1 holds: `plan_build` is dominated by Debug distortion, at `24,512` ticks per instruction
in Debug against `2,162` in Release for the same code and input, a ratio of 1/11.34.

The more consequential result is that the stage ranking inverts between configurations. In Debug
`classify` leads at 40.71% with `walk` at 24.52% and `decode` at only 10.37%; in Release `decode`
leads at 44.02% while `classify` falls to 16.07%. The reason is that the Debug factor is not
uniform across stages, ranging from 2.67x for the C-implemented Zydis decode to 28.7x for
`classify`, whose small predicate functions and STL containers inflate most. Choosing an
optimization target from Debug numbers alone would therefore have picked the wrong one.

It also refutes a premise carried since Task 328: describing the per-instruction cost as large
for Zydis decoding assumed decoding dominated, and it does not — 10.37% of `plan_build` in Debug.

### Gate results

A1 (Debug `decode` at or above 50%) is rejected at 10.37%, A2 (`record_build` at or above 30%)
at 11.44%, A3 (`sweep` at or above 20%) at 0.68%, and A4 (no stage reaching 30%) by `classify` at
40.71%. All four Part A gates therefore fail, which records a gap in the pre-registration rather
than a measurement failure: the design did not enumerate a `classify`-dominant case. B1 holds at
1/11.34 and B2 is rejected.

### F5 resolved

The jump-table sweep, whose pass count had never been measured, runs exactly one pass and visits
47,692 records, equal to the instruction count, so no reclassification scheduled another walk at
this entry. It costs 0.68% in both configurations, so its re-walk structure is not a problem.

### Measurements

The probe builds a plan five times from the real PIU image entry and reports the fastest sample.
Both configurations produced the identical plan of 47,692 instructions across 11,599 blocks.
Debug took 1,169,034,997 ticks against Release's 103,112,986, with decode at 2,543 against 951
ticks per instruction and record build at 2,804 against 192. The Debug figure of 24,512 ticks per
instruction sits 3.6% from the 25,433 measured in the live 60-second run, so the benchmark
represents in-situ cost well, which is why this task finished without another game run. In
Release the absolute cost is about 865ns per instruction, or roughly 0.9ms for a 1,058-instruction
translation, about a eleventh of the 26.9M ticks per append observed in Debug.

### Verification

The Debug build, the Release probe build, and the whole probe suite in both configurations passed
with exit 0, including the new `plan_build_bench_*` group. `plan_build_bench_plan_unchanged`
confirms the instrumented and uninstrumented builds agree, `arena_view_all` re-confirms Task 329's
equivalence, and no backwards TSC sample was recorded.

### Confirmed and unresolved

The Debug factor for `plan_build` is 11.34x and is not uniform across stages, so Debug-derived
conclusions of the form "which stage dominates" can invert in Release, while conclusions about
algorithmic complexity such as Task 323's linear scan, or about bandwidth and syscall cost such as
Task 329's snapshot, are unaffected. Left unresolved: whether the game itself can run in a Release
build, which is the natural follow-up but belongs in its own task because changed timing can
expose new races; the composition of the 11.5-12.3% residual, presumed to be the four timer
boundaries per instruction; and whether Release's 951 ticks per decode is above Zydis's usual
cost, since the operand-array zero-fill and the timestamp pair were not separated from it.
