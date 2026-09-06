# Task 612 — AOT guest 주소 맵 추적 작업 지시

## 한국어

1. `RunExecutionThread` 초기화 경로에 opt-in `REPIU_AOT_GUEST_MAP_TRACE`
   파서를 추가합니다.
2. 각 offset을 relocated guest address로 변환하고 초기 및 최종
   `AotCodeCachePlacement::address_map`을 검색합니다.
3. exact/covering entry, 이전·다음 entry, 관련 fixup과 유효한 emitted bytes를
   진단 로그로 출력합니다.
4. 기본 경로에서는 새 환경 변수에 의한 부작용과 출력을 만들지 않습니다.
5. 설계 문서와 작업 로그를 갱신합니다.
6. Linux x64 `repiu` 및 `repiu_core_probe`를 빌드·실행하고, 기본 및 trace
   `pumpit2a` 결과를 비교합니다.
7. 결과를 `docs/analysis/linux-port-frontier.md`에 누적합니다.

### 완료 조건

* 지정된 allocator/보조 함수 주소 각각에 대해 초기·최종 map coverage가
  관찰됩니다.
* trace가 map/cache/fixup을 변경하지 않습니다.
* `repiu_core_probe` 전체 검사가 통과합니다.
* 기본 실행의 기존 DOS trace, 오류 메시지, 정상 종료 상태가 유지됩니다.
* 이번 단계에서는 memory contract를 추측하여 구현하지 않습니다.

## English

1. Add an opt-in `REPIU_AOT_GUEST_MAP_TRACE` parser to the
   `RunExecutionThread` initialization path.
2. Convert each offset to a relocated guest address and search the initial and
   final `AotCodeCachePlacement::address_map`.
3. Log exact/covering entries, neighboring entries, matching fixups, and valid
   leading emitted bytes.
4. Keep the default path free of new output and behavioral side effects.
5. Update the design document and work log.
6. Build and run Linux x64 `repiu` and `repiu_core_probe`, then compare default
   and trace `pumpit2a` runs.
7. Append the result to `docs/analysis/linux-port-frontier.md`.

### Done criteria

* Initial and final map coverage is observable for each allocator/helper target.
* The trace does not modify the map, cache, or guest state.
* All `repiu_core_probe` checks pass.
* Default DOS trace, guest error, and normal termination remain unchanged.
* No guessed memory-contract behavior is implemented in this step.
