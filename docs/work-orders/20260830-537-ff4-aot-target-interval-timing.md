# 20260830-537 FF4 AOT 대상 구간 계측 작업 지시서

## 목표

FF4 resolved target에 대한 순수 target cycle 질문을 바로 단정하지 않고, 현재
dispatch 구조에서 안정적으로 잡을 수 있는 `AOT target interval`을 계측합니다.
구간은 resolved target의 AOT cache 재진입부터 다음 `DispatchGuestFault` 공통
진입점까지입니다.

## 범위

* `aot_ff_target_timing.h/.cpp`를 추가하고 FF4 attribution에 고정 용량 profile을
  연결합니다.
* FF4 target sample에서 source/target/index 후보를 저장하고, AOT single-step
  resolved path에서 matching interval을 시작합니다.
* `DispatchGuestFault` 초기에 interval을 종료합니다.
* env toggle, live reporter 한 줄, 합성 probe 검증을 추가합니다.
* 원본 EXE, guest code, AOT translation semantics는 변경하지 않습니다.
* 실제 값은 pure target cycle이 아닌 혼합 interval로 해석합니다.

## 실행 순서

1. Task 536 설계와 현재 FF4 attribution 경계를 확인합니다.
2. 설계에 맞춰 timing profile 자료구조와 고정 슬롯 aggregate를 구현합니다.
3. FF4 sample, AOT resolve, 공통 fault dispatcher에 경계를 연결합니다.
4. live reporter와 합성 probe를 연결합니다.
5. Windows Debug 빌드와 Linux i386 빌드를 수행하고 합성 probe를 실행합니다.
6. `pumpipx3`, `pumpit1`을 동일 조건으로 각 60초 실행합니다.
7. 완료/active/mismatch/overflow 및 key별 통계를 분석 문서에 기록합니다.
8. 작업 로그와 관련 analysis 문서를 갱신하고 Task 537 변경만 커밋합니다.

## 검증 기준

* timing profile이 꺼진 기본 실행에서 동작 의미가 바뀌지 않아야 합니다.
* 합성 probe가 후보 match와 100 cycle aggregate를 true로 보고해야 합니다.
* 두 플랫폼 빌드가 성공해야 합니다.
* 두 타이틀의 live output에서 timing line을 추출할 수 있어야 합니다.
* active interval과 target-resolution failure를 구분해야 합니다.
* 결과를 pure target cycle로 표현하지 않아야 합니다.

---

# 20260830-537 Work Order: FF4 AOT Target-Interval Timing

## Goal

Measure the stable interval available in the current dispatch structure without claiming a
pure target-cycle result. The interval runs from AOT-cache reentry for a resolved FF4 target
to the next common entry into `DispatchGuestFault`.

## Scope

* Add `aot_ff_target_timing.h/.cpp` and attach a fixed-capacity profile to FF4 attribution.
* Save source/target/index candidate metadata from FF4 target sampling and begin a matching
  interval on the AOT single-step resolved path.
* Complete the interval at the start of `DispatchGuestFault`.
* Add the environment toggle, one live-report line, and synthetic probe coverage.
* Do not change the original executable, guest code, or AOT translation semantics.
* Interpret measurements as mixed intervals, never as pure target cycles.

## Execution order

1. Confirm the Task 536 design and current FF4 attribution boundary.
2. Implement the timing profile and fixed-slot aggregates.
3. Connect the FF4 sampler, AOT resolve path, and common fault dispatcher.
4. Connect live reporting and the synthetic probe.
5. Build Windows Debug and Linux i386, then run the synthetic probe.
6. Run pumpipx3 and pumpit1 for 60 seconds under the same conditions.
7. Record completed/active/mismatch/overflow and per-key statistics in analysis.
8. Update the work log and related analysis documents, then commit only Task 537 files.

## Acceptance criteria

* Default execution with timing disabled has unchanged behavior semantics.
* The synthetic probe reports a matching candidate and a true 100-cycle aggregate.
* Both platform builds succeed.
* Live output contains an extractable timing line for both titles.
* Active intervals are distinguished from target-resolution failures.
* The result is not described as pure target cycles.
