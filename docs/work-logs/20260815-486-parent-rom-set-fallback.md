# 부모 ROM 세트 fallback 작업 로그 / Parent ROM-Set Fallback Work Log

## 한국어

### 결과

- `TargetProfile`에 `parent_rom_set_id`를 추가하고 22개 PIU 프로필에 제공된 MAME
  `GAME` 선언의 직접 parent를 등록했습니다.
- `RomZipEntry::missing`으로 ZIP 항목 없음과 archive/추출/CRC 오류를 구분했습니다.
- CAT702 조회를 현재 이름, 현재 ZIP의 부모 이름, 형제 부모 ZIP의 부모 이름 순서로
  확장했습니다. 앞 단계가 항목 없음일 때만 다음 단계로 진행합니다.
- clone은 계속 자신의 `rom_set_id`로 ZIP/CHD와 실행 파일을 mount합니다.

### 검증

- Win32 x86 Debug `repiu` 및 `repiu_aot_probe` 빌드: 성공
- Debug `repiu_aot_probe.exe --piu10`: 성공
  - `piu10_target_profiles=true`
  - `piu10_isa_board_probe=true`
  - `piu10_cat702_vector=true`
- Win32 x86 Release 전체 빌드: 성공
- Debug/Release `pumpipx3` 전체 AOT probe: 종료 코드 0
  - `piu10_target_profiles=true`
  - `dos_date_probe=pass`
  - `coherence_all=true`
- 실제 `pumpitpru` 5초 제한 실행: 성공적으로 제한 종료
  - `Parent ROM set: pumpitpr`
  - `parent fallback in current archive: extracted 'pumpitpr.cat702'`
  - PIU10 port I/O handled/unhandled: `33/0`
  - Glide 초기화 도달

## English

### Result

- Added `TargetProfile::parent_rom_set_id` and registered the direct parent from
  the supplied MAME `GAME` declaration for all 22 PIU profiles.
- Added `RomZipEntry::missing` to distinguish an absent member from archive,
  extraction, and CRC failures.
- Extended CAT702 lookup through current name, parent name in the current ZIP,
  then parent name in the sibling parent ZIP. Only a missing member advances to
  the next step.
- Clones continue mounting their own ZIP/CHD and executable through `rom_set_id`.

### Verification

- Win32 x86 Debug `repiu` and `repiu_aot_probe` builds: passed
- Debug `repiu_aot_probe.exe --piu10`: passed
  - `piu10_target_profiles=true`
  - `piu10_isa_board_probe=true`
  - `piu10_cat702_vector=true`
- Full Win32 x86 Release build: passed
- Debug/Release full `pumpipx3` AOT probe: exit code zero
  - `piu10_target_profiles=true`
  - `dos_date_probe=pass`
  - `coherence_all=true`
- Real `pumpitpru` five-second bounded run: reached the expected timeout
  - `Parent ROM set: pumpitpr`
  - `parent fallback in current archive: extracted 'pumpitpr.cat702'`
  - PIU10 port I/O handled/unhandled: `33/0`
  - Reached Glide initialization
