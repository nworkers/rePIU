# 부모 ROM 세트 fallback 작업 지시 / Parent ROM-Set Fallback Work Order

설계: [20260815-486-parent-rom-set-fallback](../design/20260815-486-parent-rom-set-fallback.md)

## 한국어

- [x] `pumpitpru` 로그와 ROM ZIP의 CAT702 항목 이름을 확인합니다.
- [x] `TargetProfile`에 MAME parent ROM 세트 ID를 추가합니다.
- [x] 22개 PIU 프로필에 정확한 parent 관계를 등록하고 probe로 검증합니다.
- [x] ZIP 항목 없음 상태를 구조화하여 다른 자산 오류와 구분합니다.
- [x] CAT702을 현재 세트, 현재 ZIP의 부모 이름, 부모 ZIP 순서로 조회합니다.
- [x] 관련 아키텍처와 분석 문서를 갱신합니다.
- [x] Win32 x86 Debug/Release 빌드와 전체 probe를 실행합니다.
- [x] 작업 로그를 작성하고 커밋합니다.

## 완료 조건

`pumpitpru`가 `pumpitpr.cat702`를 사용해 PIU10 보드를 초기화해야 합니다. fallback은
항목 없음에만 적용하고, 손상 또는 CRC 오류는 실패 상태를 유지해야 합니다. 기존
프로필 순서, mount 경로와 non-clone CAT702 로딩은 바뀌지 않아야 합니다.

## English

- [x] Inspect the `pumpitpru` log and CAT702 member name in its ROM ZIP.
- [x] Add the MAME parent ROM-set ID to `TargetProfile`.
- [x] Register and probe the exact parent relationship for all 22 PIU profiles.
- [x] Represent a missing ZIP member separately from other asset failures.
- [x] Resolve CAT702 from the current set, parent name in the current ZIP, then parent ZIP.
- [x] Update related architecture and analysis documentation.
- [x] Run Win32 x86 Debug and Release builds and the complete probe.
- [x] Write the work log and commit.

## Completion Criteria

`pumpitpru` initializes PIU10 with `pumpitpr.cat702`. Fallback applies only to a
missing member; corruption or CRC errors remain failures. Existing profile order,
mount paths, and non-clone CAT702 loading remain unchanged.
