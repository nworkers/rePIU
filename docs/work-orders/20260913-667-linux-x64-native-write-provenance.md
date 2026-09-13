# Task 667 작업 지시: Linux x64 AOT native write provenance

## 한국어

### 작업 항목

1. 기존 guest write-trace event에 `native-aot` 종류를 추가합니다.
2. Linux x64 AOT emitter에 opt-in native memory-write observer를 추가합니다.
3. observer가 원본 LEGACY_32 명령의 explicit memory operand destination을
   계산하고 기존 watch 주소에 일치할 때만 기록하도록 합니다.
4. caller-saved guest register와 flags를 보존하여 진단 코드가 guest semantics를
   바꾸지 않도록 합니다.
5. native observer가 켜진 경우 선택된 watch page만 AOT write-watch에서 제외하고
   다른 coherence watch는 유지합니다.
6. `REP STOSD` 및 `REP MOVS` HLE handler가 watch 주소를 포함하는 bulk write를
   exact write-trace로 기록하도록 합니다.
7. CMake, 설계 문서, 누적 분석, 작업 로그를 갱신하고 빌드 및 bounded runtime으로
   검증합니다.

### 금지 사항

- `0x010F0232` 또는 다른 특정 guest EIP에 대한 ESP 예외 처리를 추가하지 않습니다.
- zero return을 유효한 주소로 치환하지 않습니다.
- 선택된 watch page 이외의 AOT write-watch protection은 변경하지 않습니다.

### 완료 조건

- trace 비활성 상태에서 기존 빌드 동작이 유지됩니다.
- trace 활성 상태에서 `native-aot` 사건이 기존 ring에 기록됩니다.
- `0x0158C860` watch 실행이 조기 종료되지 않고 마지막 native writer가 관찰됩니다.
- 결과와 남은 원인이 누적 분석 및 작업 로그에 기록됩니다.

### 추가 진단 항목

`verbose` 설정에서 생성된 native-write observer가 실제로 도달하는지와 디코드된
목적지가 watch 주소와 일치하는지를 제한된 횟수로 출력합니다.

watch 주소와 일치하는 쓰기는 원본 명령의 제한된 바이트와 guest 레지스터 snapshot도
출력하여 source 값 문제와 lowering 문제를 구분합니다.

캐시 진입 설치 시 dispatch frame에 context를 기록하여 observer가 현재 실행 중인
`ThreadContext`를 통해 guest runtime 범위를 검증하도록 합니다.

## English

### Work items

1. Add a `native-aot` event kind to the existing guest write trace.
2. Add an opt-in native memory-write observer to the Linux x64 AOT emitter.
3. Have the observer compute the explicit memory operand destination from the
   original LEGACY_32 instruction and record only when it matches the watch address.
4. Preserve caller-saved guest registers and flags so diagnostics cannot change guest
   semantics.
5. Remove only the selected watch page from AOT write-watch protection while the
   native observer is enabled; keep other coherence watches active.
6. Add exact write-trace recording to the shared `REP STOSD` and `REP MOVS` HLE
   handlers for bulk writes containing the watch address.
7. Add a bounded `verbose` mode that reports whether emitted native-write
   observers are reached and whether their decoded destination matches.
8. Print a bounded original-byte and guest-register snapshot for matching
   writes to classify source-value versus lowering errors.
9. Initialize the dispatch frame context at cache-entry installation so the
   observer can validate guest runtime ranges through the active `ThreadContext`.
10. Update CMake, design, cumulative analysis, and work log; verify with a build and a
   bounded runtime.

### Prohibitions

- Do not add an ESP exception for `0x010F0232` or any other specific guest EIP.
- Do not replace a zero return with a fabricated valid address.
- Do not change write-watch protection for pages other than the selected watch page.

### Completion criteria

- Existing behavior remains unchanged when the trace is disabled.
- `native-aot` events appear in the existing ring when enabled.
- The `0x0158C860` watch run does not terminate early and exposes the last native writer.
- Findings and remaining cause are recorded in cumulative analysis and the work log.
