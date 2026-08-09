# 20260809-453 타깃 범위 JAMMA 보드 작업 로그 / Target-Scoped JAMMA Board Work Log

설계: [20260809-453-target-scoped-jamma-board.md](../design/20260809-453-target-scoped-jamma-board.md)

작업 지시: [20260809-453-target-scoped-jamma-board.md](../work-orders/20260809-453-target-scoped-jamma-board.md)

## 한국어

### 결과

- `TargetProfile`에 기본값이 false인 `enable_piu_jamma_board` capability를 추가했습니다.
- `pumpit1`, `pumpit2`, `pumpit3`, `pumpito`, `pumpitc`, `pumpitpc`, `pumpite`에서만
  capability를 활성화했습니다.
- Win32 host에서 실행 trampoline과 `ThreadContext`까지 capability를 전달했습니다.
- capability가 true일 때만 YMZ280B sample ROM을 초기화하고 `0x02A0..0x02AF`의
  YMZ280B, JAMMA 입력, EEPROM, 관찰 기반 write fallback을 처리합니다.
- 후기 네 target만 활성화되는 PIU10 flash/CAT702 capability는 변경하지 않았습니다.
- AOT probe에 모든 built-in profile의 JAMMA capability assertion을 추가했습니다.

### 검증

1. `cmd /c scripts\\build_win32_x86.bat`: Win32 x86 Debug 전체 빌드 성공. 첫 실행은
   공용 header 변경에 따른 재컴파일 중 180초 실행 제한에 도달했지만 `repiu.exe`까지
   생성되었고, 두 번째 증분 실행에서 남은 probe와 전체 target이 성공했습니다.
2. `repiu_aot_probe.exe MASTER\\PIU_1ST\\PIU\\PIU.EXE`: exit code 0,
   `jamma_target_profiles=true`, `piu10_target_profiles=true`,
   `piu10_isa_board_probe=true`, `coherence_all=true`.
3. 1초 `piu_1st` 실행: JAMMA=false, PIU10=false, YMZ 초기화 로그 없음,
   exception=false, timeout=true.
4. 1초 `pumpit1` 실행: JAMMA=true, PIU10=false, YMZ 초기화 성공,
   exception=false, timeout=true.
5. 1초 `pumpito` 실행: JAMMA=true, PIU10=true, YMZ와 PIU10/CAT702 초기화 성공,
   exception=false, timeout=true.

## English

### Result

- Added the default-false `TargetProfile::enable_piu_jamma_board` capability.
- Enabled it only for `pumpit1`, `pumpit2`, `pumpit3`, `pumpito`, `pumpitc`, `pumpitpc`,
  and `pumpite`.
- Carried the capability from the Win32 host through the execution trampoline into
  `ThreadContext`.
- YMZ280B sample-ROM setup and the YMZ280B, JAMMA input, EEPROM, and observed-write fallback
  paths in `0x02A0..0x02AF` now run only when the capability is true.
- Left the separate PIU10 flash/CAT702 capability, enabled only for the four later targets,
  unchanged.
- Added JAMMA-capability assertions for every built-in profile to the AOT probe.

### Verification

1. `cmd /c scripts\\build_win32_x86.bat`: the complete Win32 x86 Debug build passed. The first
   invocation reached its 180-second harness limit during the broad common-header rebuild after
   producing `repiu.exe`; a second incremental invocation completed every remaining probe and
   target.
2. `repiu_aot_probe.exe MASTER\\PIU_1ST\\PIU\\PIU.EXE`: exit code 0 with
   `jamma_target_profiles=true`, `piu10_target_profiles=true`,
   `piu10_isa_board_probe=true`, and `coherence_all=true`.
3. A one-second `piu_1st` run reported JAMMA=false and PIU10=false, emitted no YMZ setup line,
   and reached timeout with exception=false.
4. A one-second `pumpit1` run reported JAMMA=true and PIU10=false, initialized YMZ, and reached
   timeout with exception=false.
5. A one-second `pumpito` run reported JAMMA=true and PIU10=true, initialized YMZ and
   PIU10/CAT702, and reached timeout with exception=false.
