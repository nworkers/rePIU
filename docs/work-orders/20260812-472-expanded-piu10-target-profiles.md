# 작업 지시: PIU10 타깃 프로파일 추가

## 범위

1. `pumpite` 중복 없이 `pumpitpr`, `pumpitpx`, `pumpit8`, `pumpitp2`, `pumpipx2`, `pumpitp3`, `pumpipx3`을 내장 registry에 추가합니다.
2. 각 프로파일에 공용 CHD 경로, `piu_common`, JAMMA, PIU10, CAT702 capability를 설정합니다.
3. profile probe가 전체 PIU 계열의 등록값을 검증하도록 확장합니다.
4. README와 아키텍처의 지원 목록을 갱신합니다.
5. Debug/Release 빌드, probe 및 신규 id analyzer 선택을 검증합니다.
6. 결과를 작업 로그에 기록하고 커밋합니다.

## 완료 조건

- 요청한 8개 이름이 모두 lookup되며 `pumpite`는 한 번만 존재합니다.
- 신규 7개 프로파일의 id, 경로, ROM-set, capability와 latency가 계약과 일치합니다.
- 자산이 없을 때도 unknown profile이 아니라 해당 ROM-set의 구체적인 자산 오류를 반환합니다.

---

# Work order: expanded PIU10 target profiles

## Scope

1. Add `pumpitpr`, `pumpitpx`, `pumpit8`, `pumpitp2`, `pumpipx2`, `pumpitp3`, and `pumpipx3` to the built-in registry without duplicating `pumpite`.
2. Configure shared CHD paths, `piu_common`, and JAMMA, PIU10, and CAT702 capabilities.
3. Extend the profile probe to validate the complete PIU family.
4. Update supported-profile lists in the README and architecture.
5. Verify Debug/Release builds, probes, and analyzer selection for every new id.
6. Record the result in a work log and commit it.

## Completion criteria

- All eight requested names resolve and `pumpite` occurs exactly once.
- Every new profile matches the id, path, ROM-set, capability, and latency contract.
- Missing assets produce a ROM-set-specific diagnostic rather than an unknown-profile error.
