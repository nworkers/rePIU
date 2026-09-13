# Task 664 작업 지시: Linux x64 AOT/HLE 재진입 guest ESP 추적

## 한국어

### 작업 범위

1. Task 663 결과를 기준으로 AOT/HLE reentry 경계의 guest ESP를 추적합니다.
2. 기존 `REPIU_AOT_HLE_REENTRY_TRACE` helper가 guest register snapshot을
   출력하도록 확장합니다.
3. HLE dispatcher 전후와 AOT resume 전후의 EIP/ESP를 bounded trace로
   기록합니다.
4. Linux x64 Debug `repiu`와 `repiu_core_probe`를 빌드합니다.
5. `0x010F0232` 실패 frontier 및 `0x010EFEC4` `PUSH ES`를 별도로 실행해
   ESP delta를 비교합니다.
6. analysis와 작업 로그를 갱신하고 하나의 Git 커밋으로 남깁니다.

### 구현 순서

1. [x] 관련 AOT/HLE dispatcher와 기존 reentry trace를 확인합니다.
2. [x] 설계에 따른 before/after guest ESP trace를 구현합니다.
3. [x] Linux x64 Debug `repiu`와 `repiu_core_probe`를 빌드합니다.
4. [x] `0x010F0232` 및 `0x010EFEC4` bounded trace를 실행합니다.
5. [x] 확인 결과와 남은 blind spot을 analysis/work-log에 기록합니다.
6. [x] 작업 단위 커밋을 남깁니다.

### 완료 조건

- trace가 없을 때 기존 guest state와 출력이 유지됩니다.
- HLE dispatcher 전후 guest EIP/ESP가 동일 필터로 관찰됩니다.
- AOT resume 성공 여부와 resume 시점 guest ESP가 관찰됩니다.
- Debug 빌드와 core probe가 통과합니다.
- 4바이트 ESP delta의 AOT/HLE 경계 귀속 여부가 문서에 명시됩니다.

## English

### Scope

1. Trace guest ESP at the AOT/HLE re-entry boundary identified by Task 663.
2. Extend the existing `REPIU_AOT_HLE_REENTRY_TRACE` helper to print a guest
   register snapshot.
3. Record EIP/ESP before and after HLE dispatch and before and after AOT resume
   under the bounded trace.
4. Build Linux x64 Debug `repiu` and `repiu_core_probe`.
5. Run bounded traces separately for the `0x010F0232` failure frontier and the
   `0x010EFEC4` `PUSH ES` path, then compare ESP deltas.
6. Update analysis and the work log, then leave one Git commit.

### Implementation order

1. [x] Inspect the relevant AOT/HLE dispatchers and the existing re-entry trace.
2. [x] Implement before/after guest ESP tracing from the design.
3. [x] Build Linux x64 Debug `repiu` and `repiu_core_probe`.
4. [x] Run bounded traces for `0x010F0232` and `0x010EFEC4`.
5. [x] Record confirmed findings and remaining blind spots in analysis/work log.
6. [x] Leave one Git commit for the task unit.

### Completion criteria

- No trace configuration preserves existing guest state and output.
- Guest EIP/ESP before and after HLE dispatch are observable under one filter.
- AOT resume success and guest ESP at resume are observable.
- Debug build and core probe pass.
- Documentation states whether the four-byte ESP delta belongs to the AOT/HLE boundary.
