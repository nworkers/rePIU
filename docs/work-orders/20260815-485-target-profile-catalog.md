# MAME PIU target profile 카탈로그 정비 작업 지시

설계: [20260815-485-target-profile-catalog.md](../design/20260815-485-target-profile-catalog.md)

1. 기존 PIU ROM-set profile aggregate의 공통 경로와 capability 구성을 factory로 정리합니다.
2. MAME `GAME` 목록의 22개 short name을 제공 순서로 등록합니다.
3. 누락된 8개 clone/날짜 변형 profile을 추가합니다.
4. display name을 MAME 상세 설명과 연도·날짜 정보로 갱신합니다.
5. 모든 display name의 브랜드 표기를 `Pump It Up`으로 통일합니다.
6. `piu_1st` profile을 제거하고 실행기·supervisor·analyzer 기본값을 `pumpit1`로 바꿉니다.
7. registry probe에 전체 순서, 표시명, 경로, capability와 중복 검사를 추가합니다.
8. README, ARCHITECTURE, 관련 analysis와 작업 로그를 갱신합니다.
9. Win32 x86 Debug/Release 빌드와 전체 AOT probe를 실행합니다.

## 완료 조건

`dos4gw_hello` 다음에 22개 MAME profile이 지정 순서로 한 번씩 존재하고, `piu_1st`는
존재하지 않아야 합니다. 모든 표시명과
공통 경로 계약이 기대값과 일치해야 합니다. 신규 clone은 같은 세대 parent의 hardware
capability를 유지하며 Debug/Release 전체 probe가 통과해야 합니다.

---

# MAME PIU Target Profile Catalog Work Order

Design: [20260815-485-target-profile-catalog.md](../design/20260815-485-target-profile-catalog.md)

1. Consolidate common paths and capabilities of existing PIU ROM-set profile aggregates
   in a factory.
2. Register all 22 short names in the supplied MAME `GAME` order.
3. Add the eight missing clone/date-variant profiles.
4. Update display names with the detailed MAME descriptions and year/date data.
5. Normalize the brand spelling in all display names to `Pump It Up`.
6. Remove `piu_1st` and change runner, supervisor, and analyzer defaults to
   `pumpit1`.
7. Extend the registry probe for full order, display name, path, capability, and
   duplicate validation.
8. Update README, ARCHITECTURE, the relevant analysis, and the work log.
9. Run Win32 x86 Debug and Release builds and the complete AOT probe.

## Completion criteria

All 22 MAME profiles must occur exactly once after `dos4gw_hello`, while
`piu_1st` must be absent. They use the specified order, exact display names, and
common paths. New clones retain
their parent generation's hardware capabilities, and full Debug/Release probes pass.
