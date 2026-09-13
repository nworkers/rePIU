# Task 665 작업 지시 — Linux x64 AOT incoming fixup 추적

## 한국어

### 작업 범위

1. Task 664의 AOT/HLE ESP 결과를 기준으로 `0x010F0232` incoming transfer 후보를 정적 map에서 추적합니다.
2. `TraceAotGuestMap`에 target-side incoming fixup 출력을 추가합니다.
3. source-side fixup과 구분되는 bounded trace prefix를 사용합니다.
4. Linux x64 Debug `repiu`와 `repiu_core_probe`를 빌드합니다.
5. `0x010F0232` map context를 실행하고 동적 trace와 비교합니다.
6. analysis와 작업 로그를 갱신하고 하나의 Git 커밋으로 남깁니다.

### 구현 순서

1. [x] Task 664의 AOT/HLE ESP 결과와 `TraceAotGuestMap` 구현을 확인합니다.
2. [x] incoming fixup bounded trace를 구현합니다.
3. [x] Linux x64 Debug `repiu`와 `repiu_core_probe`를 빌드합니다.
4. [x] `0x010F0232` map trace를 실행합니다.
5. [x] 정적 후보와 동적 실행 증거를 analysis/work-log에 구분하여 기록합니다.
6. [x] 작업 단위 커밋을 남깁니다.

### 완료 조건

- incoming fixup은 source-side fixup과 다른 prefix로 출력됩니다.
- 출력은 최대 32개로 제한됩니다.
- trace가 없을 때 기존 map 출력과 guest semantics가 유지됩니다.
- Debug 빌드와 core probe가 통과합니다.
- incoming fixup은 동적 증거가 아닌 정적 후보로 문서화됩니다.

## English

### Scope

1. Use Task 664's AOT/HLE ESP result to inspect incoming transfer candidates for `0x010F0232` in the static map.
2. Add target-side incoming-fixup output to `TraceAotGuestMap`.
3. Use a bounded prefix distinct from source-side fixup output.
4. Build Linux x64 Debug `repiu` and `repiu_core_probe`.
5. Run the `0x010F0232` map trace and compare it with dynamic traces.
6. Update analysis and the work log, then leave one Git commit.

### Implementation order

1. [x] Inspect Task 664's AOT/HLE ESP result and `TraceAotGuestMap`.
2. [x] Implement bounded incoming-fixup tracing.
3. [x] Build Linux x64 Debug `repiu` and `repiu_core_probe`.
4. [x] Run the `0x010F0232` map trace.
5. [x] Distinguish static candidates from dynamic evidence in analysis/work log.
6. [x] Leave one Git commit for the task unit.

### Completion criteria

- Incoming fixups use a prefix distinct from source-side fixups.
- Output is capped at 32 entries.
- No trace configuration preserves existing map output and guest semantics.
- Debug build and core probe pass.
- Incoming fixups are documented as static candidates, not dynamic proof.
