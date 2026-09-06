# Task 619 작업 지시서: Linux x64 일반 AOT stack writer 추적

## 한국어

### 작업

1. Task 618의 shared stack-write emitter를 일반 AOT `PUSH`에도 재사용하고,
   512개 ring으로 terminal 실행 이력을 보존한다.
2. 계획 record의 mnemonic과 실제 successful lowering을 기준으로 일반 push만
   기록한다.
3. zero return-frame 출력에서 direct-call과 guest-push record를 구분한다.
4. Linux x64 빌드, core probe, `pumpit2a` terminal reproduction을 실행한다.
5. 결과를 누적 분석과 작업 로그에 남기고 커밋한다.

### 안전 제약

* `REPIU_LINUX_X64_STACK_TRACE`가 없으면 AOT bytes는 바뀌지 않는다.
* trace는 host flags를 보존하고 guest `R15D` 외의 guest state를 변경하지
  않는다.
* trace 결과만으로 zero return target을 복구하지 않는다.

### 완료 조건

* `core_probe_failures=0`.
* consumed slot match 로그에 writer 종류가 표시된다.
* direct-call 이후 일반 push 또는 일반 push 부재가 재현 로그로 확인된다.

## English

### Work

1. Reuse Task 618's shared stack-write emitter for ordinary AOT `PUSH`, with a
   512-record ring that retains the terminal run's history.
2. Trace ordinary pushes based on the plan mnemonic and successful lowering.
3. Distinguish direct-call and guest-push records in zero return-frame output.
4. Build Linux x64, run the core probe, and reproduce the terminal `pumpit2a`
   event.
5. Record the result in the cumulative analysis and work log, then commit it.

### Safety constraints

* Without `REPIU_LINUX_X64_STACK_TRACE`, emitted AOT bytes are unchanged.
* The trace preserves host flags and changes no guest state other than the
  existing guest `R15D` behavior.
* Do not repair a zero return target based only on trace output.

### Done criteria

* `core_probe_failures=0`.
* Consumed-slot matches identify the writer kind.
* The reproduction establishes whether an ordinary push follows the direct-call
  writes.
