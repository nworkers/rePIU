# 20260808-449 PIU CHD 타깃 프로파일 확장 작업 지시 / PIU CHD Target Profile Expansion Work Order

설계: [20260808-449-piu-chd-target-profiles.md](../design/20260808-449-piu-chd-target-profiles.md)

## 한국어

### 목표

`pumpito`, `pumpitc`, `pumpitpc`, `pumpite`를 이 순서대로 내장 타깃 프로파일에 추가하고
기존 공용 PIU CHD mount 및 `piu_common` HLE 경로에 연결합니다.

### 작업 항목

- [x] `src/target/target_profile.cpp`에 네 프로파일을 지정 순서로 등록합니다.
- [x] 공용 PIU CHD reader가 멀티세션 이미지의 마지막 data track을 선택하도록 일반화합니다.
- [x] `ARCHITECTURE.md`의 현재 내장 타깃 목록을 갱신합니다.
- [x] `README.md`의 MAME CHD 자산 배치 설명을 여러 ROM-set에 적용되도록 갱신합니다.
- [x] `pumpite` 멀티 data-track 확인 결과를 `docs/analysis/`와 색인에 기록합니다.
- [x] Win32 x86 Debug 빌드와 네 ID의 선택 경로를 검증합니다.
- [x] 결과와 자산 제약을 작업 로그에 기록합니다.
- [x] 관련 변경을 하나의 Git 커밋으로 남깁니다.

### 완료 조건

네 ID가 정확한 순서와 경로로 레지스트리에 나타나고, 기존 타깃을 깨지 않으며, 빌드가
성공해야 합니다. 원본 자산이 없는 타깃의 실제 게임 진행은 완료 조건에 포함하지 않습니다.

## English

### Objective

Add `pumpito`, `pumpitc`, `pumpitpc`, and `pumpite`, in that order, to the built-in target
profiles and connect them to the shared PIU CHD mount and `piu_common` HLE paths.

### Work Items

- [x] Register the four profiles in the requested order in `src/target/target_profile.cpp`.
- [x] Generalize the shared PIU CHD reader to select the last data track in a multisession image.
- [x] Update the current built-in target list in `ARCHITECTURE.md`.
- [x] Generalize the MAME CHD asset layout documentation in `README.md` for multiple ROM sets.
- [x] Record the confirmed `pumpite` multi-data-track layout under `docs/analysis/` and its index.
- [x] Verify the Win32 x86 Debug build and target selection for all four IDs.
- [x] Record results and asset constraints in the work log.
- [x] Commit the related changes as one Git task unit.

### Completion Criteria

All four IDs appear in the registry with the requested order and paths, existing targets remain
intact, and the build succeeds. Actual gameplay is not required for targets whose original assets
are unavailable.
