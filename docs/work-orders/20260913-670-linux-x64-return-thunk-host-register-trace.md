# Task 670 작업 지시서: Linux x64 ReturnThunk host register trace

## 한국어

### 선행 설계

- `docs/design/20260913-670-linux-x64-return-thunk-host-register-trace.md`

### 작업 범위

1. Linux x64 fault handler에 host `R10/R14/R15` 추출을 추가합니다.
2. unhandled fault 진단 line에 해당 값을 bounded하게 출력합니다.
3. `repiu`와 `repiu_core_probe`를 빌드합니다.
4. `pumpit2a` bounded 실행으로 ReturnThunk producer/target을 기록합니다.
5. 결과와 후속 원인 분석을 작업 로그 및 누적 Linux frontier 문서에 반영합니다.

### 비범위

- ReturnThunk 의미 변경
- guest 레지스터 매핑 변경
- 특정 EIP/ESP에 대한 예외처리 추가
- 원본 guest 실행 파일 수정

### 완료 조건

- host register trace가 Linux x64에서만 유효한 값으로 출력됩니다.
- core probe와 `repiu` 빌드가 통과합니다.
- bounded 실행 결과로 다음 분석 대상 producer/target이 확인되거나, 값이 유효하지 않은 이유가 로그에 남습니다.

## English

### Preceding design

- `docs/design/20260913-670-linux-x64-return-thunk-host-register-trace.md`

### Scope

1. Add host `R10/R14/R15` extraction to the Linux x64 fault handler.
2. Print the values in the bounded unhandled-fault diagnostic line.
3. Build `repiu` and `repiu_core_probe`.
4. Run bounded `pumpit2a` to record the ReturnThunk producer and target.
5. Record the result and follow-up causal analysis in the work log and cumulative Linux frontier document.

### Out of scope

- Changing ReturnThunk semantics
- Changing guest register mapping
- Adding an EIP/ESP-specific exception
- Modifying the original guest executable

### Completion criteria

- The host register trace is available only with meaningful Linux x64 context values.
- The core probe and `repiu` build pass.
- The bounded run identifies the next producer/target for analysis, or records why the values are unavailable.
