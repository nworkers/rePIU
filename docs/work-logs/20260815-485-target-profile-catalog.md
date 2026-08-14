# MAME PIU target profile 카탈로그 정비 작업 로그

설계: [20260815-485-target-profile-catalog.md](../design/20260815-485-target-profile-catalog.md)

작업 지시: [20260815-485-target-profile-catalog.md](../work-orders/20260815-485-target-profile-catalog.md)

## 결과

- 기존 14개 MAME profile을 공식 `GAME` 순서의 22개 전체 카탈로그로 확장했습니다.
- `pumpit2a`, `pumpit3a`, `pumpitpru`, `pumpitea`, `pumpipx2p`, `pumpitp3a`,
  `pumpipx3a`, `pumpipx3b`를 추가했습니다.
- 모든 display name을 상세 버전·연도·날짜 정보로 교체하고 `Pump It Up` 표기로
  통일했습니다.
- 공통 PIU ROM-set profile 경로, reservation과 capability 생성을
  `MakePiuTargetProfile()` factory로 정리했습니다.
- `piu_1st` profile을 삭제하고 실행기, supervisor, analyzer의 기본 target을
  `pumpit1`로 변경했습니다.
- setup/test의 현재 게임 asset 기준을 `roms/pumpit1.zip`과 `roms/pumpit1/*.chd`로
  변경했습니다.
- README와 ARCHITECTURE의 현재 target 목록과 사용법을 갱신했습니다.

## 검증

- Debug/Release `repiu_aot_probe`, `repiu`, `repiu_supervisor_win32`,
  `repiu_exe_analyzer`: 빌드 성공
- Debug/Release `pumpipx3` 전체 probe:
  `piu10_target_profiles=true`, `jamma_target_profiles=true`,
  `dos_date_probe=pass`, `coherence_all=true`, 종료 코드 0
- 무인자 Debug analyzer: `Target: pumpit1`, 상세 display name, 종료 코드 0
- `setup_test_environment.ps1`, `test_all.ps1`: PowerShell parser 오류 없음
- 기존 C4819와 LNK4217 경고만 있었으며 신규 컴파일/링크 오류는 없습니다.
- 실제 GUI game 실행을 포함하는 `test_all.ps1` 전체 실행은 이 작업에서 수행하지 않았습니다.

---

# MAME PIU Target Profile Catalog Work Log

Design: [20260815-485-target-profile-catalog.md](../design/20260815-485-target-profile-catalog.md)

Work order: [20260815-485-target-profile-catalog.md](../work-orders/20260815-485-target-profile-catalog.md)

## Result

- Expanded the previous 14 MAME profiles to the complete 22-entry official `GAME` order.
- Added `pumpit2a`, `pumpit3a`, `pumpitpru`, `pumpitea`, `pumpipx2p`, `pumpitp3a`,
  `pumpipx3a`, and `pumpipx3b`.
- Replaced every display name with detailed version/year/date information and normalized the
  brand spelling to `Pump It Up`.
- Consolidated common PIU ROM-set profile paths, reservations, and capabilities in the
  `MakePiuTargetProfile()` factory.
- Removed `piu_1st` and changed the runner, supervisor, and analyzer defaults to `pumpit1`.
- Changed current setup/test game assets to `roms/pumpit1.zip` and `roms/pumpit1/*.chd`.
- Updated README and ARCHITECTURE with the current target catalog and usage.

## Verification

- Debug/Release `repiu_aot_probe`, `repiu`, `repiu_supervisor_win32`, and
  `repiu_exe_analyzer`: build passed.
- Debug/Release full `pumpipx3` probe:
  `piu10_target_profiles=true`, `jamma_target_profiles=true`,
  `dos_date_probe=pass`, `coherence_all=true`, exit code zero.
- No-argument Debug analyzer: selected `Target: pumpit1`, printed the detailed display name,
  and exited with code zero.
- `setup_test_environment.ps1` and `test_all.ps1`: PowerShell parser reported no errors.
- Only pre-existing C4819 and LNK4217 warnings were emitted; no new compile or link errors.
- The complete `test_all.ps1`, which launches the GUI game path, was not run in this task.
