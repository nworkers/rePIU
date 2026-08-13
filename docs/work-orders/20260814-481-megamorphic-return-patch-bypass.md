# Megamorphic return patch 우회 작업 지시

설계: [20260814-481-megamorphic-return-patch-bypass.md](../design/20260814-481-megamorphic-return-patch-bypass.md)

1. Win32 전용 return patch policy header/source를 추가합니다.
2. placement에 site별 상태를 추가하고 initial placement와 dynamic append에서 동기화합니다.
3. return dispatch site lookup이 site index를 adapter에 전달하도록 확장합니다.
4. DBT return resolver가 성공적으로 target을 해석한 뒤 정책을 관찰하고, megamorphic으로
   판정된 site에서는 inline-cache patch만 생략합니다.
5. observation, megamorphic site, bypass counter를 snapshot과 종료 로그에 연결합니다.
6. 단일/4-way/8-way target, 임계값, 지속 우회, site 독립성, append 보존 probe를 추가하고
   CMake와 probe runner에 등록합니다.
7. 관련 architecture/analysis 문서를 갱신하고 Win32 x86 Debug probe와 앱을 빌드합니다.
8. 작업 로그에 검증 결과와 사용자 A/B 기준을 남기고 하나의 커밋으로 정리합니다.

## 완료 조건

신규 및 기존 AOT probe가 모두 통과하고, 정책 불가 상태는 기존 patch로 내려가며,
사용자 실행에서 return fallback 0을 유지한 채 megamorphic site와 bypass가 관찰되어야 합니다.

---

# Megamorphic Return Patch Bypass Work Order

Design: [20260814-481-megamorphic-return-patch-bypass.md](../design/20260814-481-megamorphic-return-patch-bypass.md)

1. Add a Win32 return-patch policy header/source.
2. Add per-site state to the placement and synchronize it at initial placement
   and dynamic append.
3. Extend return dispatch lookup to pass the site index to the adapter.
4. After successful target resolution, observe the DBT return policy and skip
   only the inline-cache patch for classified megamorphic sites.
5. Connect observation, megamorphic-site, and bypass counters to the snapshot
   and final log.
6. Add and register probes for one-, four-, and eight-way targets, threshold,
   persistent bypass, site isolation, and append preservation.
7. Update the relevant architecture/analysis documents and build the Win32 x86
   Debug probe and application.
8. Record verification and user A/B criteria in the work log and commit the task
   as one unit.

## Completion criteria

All new and existing AOT probes pass, unavailable policy state retains the old
patch path, and a user run preserves zero return fallback while reporting
classified megamorphic sites and increasing bypasses.
