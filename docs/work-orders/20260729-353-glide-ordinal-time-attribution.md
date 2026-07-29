# 20260729-353 Glide ordinal 시간 귀속 작업 지시 / Work order

* 설계: [20260729-353-glide-ordinal-time-attribution.md](../design/20260729-353-glide-ordinal-time-attribution.md)

## 한국어

### 목표

현재 wall-clock의 약 22.7%인 Glide gate를 ordinal별 전체 시간과
queue/wake/work/complete로 분해해 다음 HLE 최적화 대상을 확정합니다.

### 구현

1. 기본 OFF인 고정 256-entry Glide ordinal timing profile을 추가합니다.
2. decoded gate의 RAII finalizer가 기존 global scope의 completed cycle을 재사용하고,
   backend가 기존 handoff timestamp를 현재 ordinal에 직접 기록합니다.
3. `ThreadContext`, 최종 execution attempt, loader summary에 정렬된 전체 ordinal
   profile을 연결합니다.
4. 합성 AOT probe에 정책, 집계, delta, clamp, capacity 검증을 추가합니다.
5. Task 347 wrapper를 재사용하는 Release 3회 측정·CSV wrapper를 추가합니다.

### 완료 조건

* 설계의 G1~G6를 코드와 wrapper가 검증합니다.
* Win32 x86 Release loader와 AOT probe가 빌드되고 전체 probe가 exit 0입니다.
* 10초 smoke에서 count·global cycle coverage와 backend delta 보존을 확인합니다.
* 동일 바이너리 control과 비교한 Release 60초 3회가 semantic invariant를 통과합니다.
* 상위 ordinal을 원본 API 의미와 backend 동작으로 분류하고 다음 구현 축을
  `docs/analysis/current-execution-frontier.md`에 반영합니다.
* 아키텍처와 작업 로그를 갱신하고 하나의 Git 커밋으로 마무리합니다.

---

## English

### Objective and implementation

Split the Glide gate, currently about 22.7% of wall time, by exact ordinal and
by queue/wake/work/complete timing so the next HLE optimization target is
evidence-based.

Add a disabled-by-default fixed 256-entry ordinal timing profile; use an RAII
finalizer to reuse each successfully decoded gate's existing global cycle
measurement and directly attribute existing backend handoff timestamps; connect sorted complete
rows through `ThreadContext`, the final attempt, and loader reporting; add
synthetic policy/aggregation/delta/clamp/capacity coverage; and add a
three-run Release wrapper around Task 347.

### Completion

The implementation must enforce design gates G1--G6, build the Win32 x86
Release loader and probe, pass the complete probe, prove count/global-cycle
coverage and backend-delta preservation in a ten-second smoke, and complete
three semantically equivalent 60-second runs against the same-binary control.
Classify leading ordinals by original API and backend behavior, update the
architecture, current frontier, and work log, then commit the task as one
unit.
