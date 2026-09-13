# Task 671 작업 지시서: Linux x64 emitted stack-effect audit

## 한국어

### 선행 설계

- `docs/design/20260913-671-linux-x64-emitted-stack-effect-audit.md`

### 작업 범위

1. instruction census에 LONG_64 emitted stack-effect audit를 추가합니다.
2. address-map entry별 guest kind/original bytes/emitted bytes와 stack-effect 명령을 bounded하게 출력합니다.
3. Linux x64 census를 빌드하고 `pumpit2a`에 실행합니다.
4. raw host stack effect의 존재 여부를 기록합니다.

### 비범위

- AOT emitter semantics 변경
- runtime fallback 정책 변경
- 원본 guest bytes 수정
- 특정 EIP/ESP 예외처리 추가

### 완료 조건

- audit가 현재 long-mode image를 LONG_64로 검사합니다.
- 출력이 entry와 원본/emitted 바이트를 연결합니다.
- census 실행 결과와 남은 원인이 문서화됩니다.

## English

### Preceding design

- `docs/design/20260913-671-linux-x64-emitted-stack-effect-audit.md`

### Scope

1. Add a LONG_64 emitted stack-effect audit to the instruction census.
2. Boundedly print each relevant address-map entry's guest kind, original bytes, emitted bytes, and stack-effect instructions.
3. Build the Linux x64 census and run it on `pumpit2a`.
4. Record whether raw host stack effects remain.

### Out of scope

- Changing AOT emitter semantics
- Changing runtime fallback policy
- Modifying original guest bytes
- Adding a specific EIP/ESP exception

### Completion criteria

- The audit checks the current long-mode image in LONG_64.
- Output connects each entry to its original and emitted bytes.
- The census result and remaining cause are documented.
