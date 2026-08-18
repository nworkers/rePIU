# Task 495 작업 지시: JAMMA history 안전 정리

## 작업 항목

- [x] pending due/active frame 기준 최소 필요 timestamp를 계산합니다.
- [x] cutoff 이하 history를 floor state로 안전하게 compact합니다.
- [x] unsafe overflow와 coverage miss를 분리하고 prune/peak 진단을 추가합니다.
- [x] capacity를 넘는 순차 delivery probe를 추가합니다.
- [x] Win32 Debug 빌드, 기존/전체 probe, bounded guest smoke를 수행합니다.
- [x] architecture, analysis, TODO와 작업 로그를 갱신합니다.

## 완료 조건

장시간 총 edge 수가 256을 넘어도 정상 backlog 범위에서는 overflow와 coverage miss가 0이고
due-time state가 유지되어야 합니다. 120초 사용자 실행에서 1,006 edge, peak 4,
overflow/coverage miss 0으로 충족했습니다.

---

# Task 495 Work Order: Safe JAMMA History Pruning

## Work Items

- [x] Compute the minimum timestamp needed by pending due ticks and active frames.
- [x] Safely compact history at or before the cutoff into the floor state.
- [x] Separate unsafe overflow from coverage miss and add prune/peak diagnostics.
- [x] Add a sequential-delivery probe exceeding nominal capacity.
- [x] Run Win32 Debug builds, existing/complete probes, and a bounded guest smoke run.
- [x] Update architecture, analysis, TODO, and the work log.

## Completion Criteria

Normal backlog operation reports zero overflow and coverage miss even after total session edges
exceed 256, while preserving due-time state. A 120-second user run satisfied this with 1,006 edges,
a peak of four retained entries, and zero overflow or coverage miss.
