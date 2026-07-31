# 작업 지시: 아레나 밖 single-step 귀속 / Work order: attribute out-of-arena single steps

Task 376 **1단계 — 계측만.** 설계:
[20260801-376](../design/20260801-376-out-of-arena-single-step-attribution.md)

## 한국어

### 목표

`execution_trampoline.cpp:3135`에서 버려지는 single-step 예외를 사유별로 귀속한다.
억제 구현은 이 단계가 사전 등록 게이트를 넘긴 뒤 별도 작업으로 한다.

### 사전 등록 게이트

| 등급 | 기준 | 행동 |
|---|---|---|
| A | 단일 무장 지점이 70% 이상 설명 | 2단계 억제 구현 |
| B | 상위 두 지점이 70% 이상 | 그 둘만 표적 |
| C | 지배적 원인 없음 | 축을 닫고 기록만 |

**미달이면 구현하지 않는다.** Tasks 368·373의 규율을 따른다.

### 단계

1. **귀속 모듈 추가**
   * `include/repiu/platform/win32/out_of_arena_step_census.h`
   * `src/platform/win32/out_of_arena_step_census.cpp`
   * 분류: EIP 위치(AOT 캐시 / 호스트 이미지 / 그 외) × `enable_single_step_trace` ×
     `aot_reentry_pending`.
   * 최초·최후 EIP, 상위 빈도 EIP(소규모 고정 배열).
   * `CMakeLists.txt` 등록.

2. **버리는 지점 연결** (`execution_trampoline.cpp:3135`)
   * `return EXCEPTION_CONTINUE_EXECUTION` **직전**에 기록.
   * **clock read 추가 금지** — 카운터 증가만.
   * 기존 동작(TF 클리어 후 continue)은 **변경하지 않는다**.

3. **요약 출력**
   * `Win32 out-of-arena step total/aot-cache/host-image/other`
   * `Win32 out-of-arena step trace-on/trace-off/reentry-pending`
   * `Win32 out-of-arena step first/last eip`
   * 상위 EIP 몇 개.

4. **probe 추가**
   * `out_of_arena_step_census_probe.{h,cpp}` + `main.cpp` + `CMakeLists.txt`.
   * 검증: 위치 분류, 플래그 조합 분류, 최초/최후 EIP 보존, 상위 EIP 집계와
     오버플로, `nullptr` 무해.

5. **빌드·측정**
   * Debug + Release, probe exit 0.
   * **`REPIU_GLIDE_SWAP_INTERVAL=0` 고정**으로 music select 캡처.
   * **wall cycle과 프레임 수를 함께 기록**(1초 watchdog 대비).
   * 귀속 합계가 `single-step 예외 총계 − single_step_trace_count`와 맞는지 대조.

6. **문서**
   * 작업 로그, `docs/analysis/current-execution-frontier.md`,
     `native-execution-single-step-overhead.md` 갱신.

### 완료 조건

* 버려진 single-step이 **100% 귀속**됨(미분류 0).
* 귀속 합계가 예외 census와 대조되어 맞음.
* 게이트 판정이 A/B/C 중 하나로 기록됨.
* probe 통과, 양 구성 빌드 성공, hot path clock read 추가 0회.

### 비범위

* 억제 구현은 하지 않는다.
* `span-safety` 비용 축소(Task 373에서 게이트 미달)는 별건.

---

## English

Stage one is measurement only: attribute the single-step exceptions discarded at
`execution_trampoline.cpp:3135`, which are 70.1% of the population and 6.5% of wall
in kernel round trips yet carry no counter. Add a census classifying by EIP location
— AOT code cache, host image, other — crossed with `enable_single_step_trace` and
`aot_reentry_pending`, plus first, last, and most frequent discarded EIPs. Record
immediately before the existing `EXCEPTION_CONTINUE_EXECUTION` without changing that
behaviour and without adding any clock read to the hot path. Print the classification,
cover it with a probe, build both configurations, and capture music select with the
swap interval pinned at zero while recording wall cycles alongside frames. Cross-check
that the attributed total equals the exception census minus `single_step_trace_count`.

Implement suppression in a later task only if one arming site explains 70% or more,
or the top two together do; otherwise close the axis with the measurement recorded.
Missing the gate means not implementing, as in Tasks 368 and 373.
