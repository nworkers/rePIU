# return dispatch site 조회 인덱스 작업 지시

1. Win32 전용 return dispatch site index header/source를 추가합니다.
2. `Win32AotCodeCachePlacement`에 index 상태를 추가합니다.
3. `ResolveAotDbtReturnMissFrame`의 `FindDispatchSite`를 index 우선, 기존 scan fallback으로
   변경합니다.
4. lookup, fallback scan, rebuild 카운터를 live telemetry와 종료 요약에 연결합니다.
5. 기존 선형 탐색을 oracle로 쓰는 probe를 추가하고 CMake에 등록합니다.
6. Win32 x86 Debug `repiu_aot_probe`와 `repiu`를 빌드하고 pumpit8 probe를 실행합니다.
7. 분석 문서와 frontier, 작업 로그를 갱신하고 하나의 커밋으로 정리합니다.

## 완료 조건

* 신규 probe와 기존 inline-cache/DBT return probe가 모두 통과합니다.
* 인덱스가 stale이면 기존 scan으로 안전하게 내려갑니다.
* 정상 사용자 실행에서 index scan 카운터가 0에 가깝고 return fallback이 0입니다.

---

# Return Dispatch Site Lookup Index Work Order

1. Add a Win32-specific return dispatch site index header/source pair.
2. Add the index state to `Win32AotCodeCachePlacement`.
3. Change `FindDispatchSite` in `ResolveAotDbtReturnMissFrame` to prefer the
   index and retain the original scan as fallback.
4. Connect lookup, fallback-scan, and rebuild counters to live telemetry and the
   final summary.
5. Add a probe using the old scan as its oracle and register it in CMake.
6. Build Win32 x86 Debug `repiu_aot_probe` and `repiu`, then run the pumpit8
   probe.
7. Update analysis/frontier and the work log, then commit the task as one unit.

## Completion criteria

All new and existing inline-cache/DBT-return probes pass; stale indexes safely
fall back to the original scan; and a live run reports near-zero index scans and
zero return fallback.
