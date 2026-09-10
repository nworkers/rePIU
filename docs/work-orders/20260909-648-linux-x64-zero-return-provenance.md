# 20260909-648 Linux x64 unresolved return provenance 작업 지시

## 한국어

### 작업

1. Task 648 설계에 따라 x64 transfer failure provenance의 enum, 고정 크기
   record, 이름 변환 및 생성 helper를 추가합니다.
2. `ThreadContext`에 record를 추가하고 x64 resolver 진입 때 이전 값을
   지웁니다.
3. `ResolveAotTransferTarget()` 실패 시 producer tag, target, guest ESP,
   실패 이유를 record에 저장합니다. producer가 유효하지 않으면 invalid로
   둡니다.
4. `RecordFaultExit`에 record의 valid/kind/producer/target/ESP/failure를
   출력합니다.
5. 공용 core probe로 RET zero target, indirect-call, invalid 상태와 이름을
   검증합니다.
6. WSL Linux x64 Debug에서 core probe와 `repiu`를 빌드하고 실제
   `pumpit2a` fault trace로 producer/target 연결을 확인합니다.
7. 구현 결과를 `ARCHITECTURE.md`, `docs/analysis/linux-port-frontier.md`,
   작업 로그에 기록합니다.

### 제한

* resolver의 target 계산·fallback·`return 0` 정책을 변경하지 않습니다.
* unresolved thunk의 `INT3/UD2` 제어 흐름을 변경하지 않습니다.
* 원본 guest executable bytes, guest register, guest stack을 수정하지
  않습니다.
* fault signal path에서 문자열 할당이나 동적 컨테이너를 추가하지
  않습니다.
* zero target만으로 stack writer를 추측하거나 수정하지 않습니다.

### 완료 기준

* `core_probe_failures=0`.
* 실제 `[repiu-exit]`에 `0x010F1E56` RET와 zero target, 실패 이유가
  표시됩니다.
* 기존 unresolved `INT3/UD2` 경계가 유지됩니다.
* 설계·분석·작업 로그가 한국어/영어로 갱신되고 관련 변경이 커밋됩니다.

## English

### Work

1. Add the x64 transfer-failure enums, fixed-size record, name conversion, and
   factory helper from the Task 648 design.
2. Add the record to `ThreadContext` and clear the previous value at x64
   resolver entry.
3. On `ResolveAotTransferTarget()` failure, store the producer tag, target,
   guest ESP, and failure reason. Leave the record invalid when the producer is
   not valid.
4. Print valid/kind/producer/target/ESP/failure in `RecordFaultExit`.
5. Verify RET zero target, indirect-call, invalid state, and name conversion
   with a shared core probe.
6. Build the Linux x64 Debug core probe and `repiu` in WSL, then confirm the
   producer/target connection in the real `pumpit2a` fault trace.
7. Record the result in `ARCHITECTURE.md`,
   `docs/analysis/linux-port-frontier.md`, and the work log.

### Limits

* Do not change resolver target calculation, fallback, or `return 0` policy.
* Do not change the unresolved thunk's `INT3/UD2` control flow.
* Do not modify original guest executable bytes, guest registers, or guest
  stack.
* Do not add string allocation or dynamic containers to the fault signal path.
* Do not infer or modify a stack writer from the zero target alone.

### Done criteria

* `core_probe_failures=0`.
* The real `[repiu-exit]` line shows the `0x010F1E56` RET, zero target, and
  failure reason.
* The existing unresolved `INT3/UD2` boundary remains unchanged.
* Design, analysis, and work log are updated in Korean/English and the related
  changes are committed.
