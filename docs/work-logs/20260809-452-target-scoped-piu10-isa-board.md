# 20260809-452 타깃 범위 PIU10 ISA 보드 작업 로그 / Target-Scoped PIU10 ISA Board Work Log

설계: [20260809-452-target-scoped-piu10-isa-board.md](../design/20260809-452-target-scoped-piu10-isa-board.md)

작업 지시: [20260809-452-target-scoped-piu10-isa-board.md](../work-orders/20260809-452-target-scoped-piu10-isa-board.md)

## 한국어

### 결과

- `TargetProfile`에 기본값이 false인 `enable_piu10_isa_board` capability를 추가했습니다.
- `pumpito`, `pumpitc`, `pumpitpc`, `pumpite`에서만 capability를 활성화했습니다.
  `dos4gw_hello`, `piu_1st`, `pumpit1`, `pumpit2`, `pumpit3`은 비활성 상태입니다.
- capability를 Win32 host, 실행 trampoline, thread context, port adapter까지 전달했습니다.
- `piu10.u8`과 `<target>.cat702`는 capability가 활성화된 경우에만 추출·초기화하며,
  `0x02D0..0x02DF`도 같은 조건에서만 PIU10 장치로 전달합니다.
- `piu10.u9` YMZ280B sample ROM 초기화는 capability와 분리하여 기존 ROM-set target의
  sound 경로를 유지했습니다.
- AOT probe에 target별 capability assertion을 추가했습니다.

### 검증

1. `cmd /c scripts\\build_win32_x86.bat`: Win32 x86 Debug 전체 빌드 성공.
   첫 실행은 profile header 변경에 따른 광범위 재컴파일 중 120초 실행 제한에 도달했으며,
   남은 증분 빌드를 다시 실행하여 오류 없이 완료했습니다.
2. `repiu_aot_probe.exe MASTER\\PIU_1ST\\PIU\\PIU.EXE`: exit code 0,
   `piu10_target_profiles=true`, `piu10_isa_board_probe=true`, `coherence_all=true`.
3. 1.5초 `pumpit1` 실행: `PIU10 ISA board enabled: false`, PIU10 초기화 로그 없음,
   exception=false, timeout=true.
4. 2.5초 `pumpito` 실행: `PIU10 ISA board enabled: true`, `piu10.u8`과
   `pumpito.cat702` 초기화 확인, exception=false, timeout=true.

PowerShell pipeline의 exit code 1은 시간 제한으로 종료된 실행 결과가 전달된 것이며,
필터된 로그는 두 실행 모두 guest exception 없이 지정 시간까지 진행했음을 보여 줍니다.

## English

### Result

- Added the default-false `TargetProfile::enable_piu10_isa_board` capability.
- Enabled it only for `pumpito`, `pumpitc`, `pumpitpc`, and `pumpite`. It remains disabled for
  `dos4gw_hello`, `piu_1st`, `pumpit1`, `pumpit2`, and `pumpit3`.
- Carried the capability through the Win32 host, execution trampoline, thread context, and port
  adapter.
- `piu10.u8` and `<target>.cat702` are extracted and initialized only when enabled, and
  `0x02D0..0x02DF` is routed to the PIU10 device under the same condition.
- Kept `piu10.u9` YMZ280B sample-ROM setup independent so existing ROM-set sound paths remain
  available.
- Added per-target capability assertions to the AOT probe.

### Verification

1. `cmd /c scripts\\build_win32_x86.bat`: the complete Win32 x86 Debug build passed. The first
   invocation reached its 120-second harness limit during the broad rebuild caused by the profile
   header change; rerunning the remaining incremental build completed without errors.
2. `repiu_aot_probe.exe MASTER\\PIU_1ST\\PIU\\PIU.EXE`: exit code 0 with
   `piu10_target_profiles=true`, `piu10_isa_board_probe=true`, and `coherence_all=true`.
3. A 1.5-second `pumpit1` run reported `PIU10 ISA board enabled: false`, emitted no PIU10
   initialization line, and reached timeout with exception=false.
4. A 2.5-second `pumpito` run reported `PIU10 ISA board enabled: true`, initialized `piu10.u8`
   and `pumpito.cat702`, and reached timeout with exception=false.

The PowerShell pipeline exit code 1 reflects the time-limited process result; the filtered logs
show that both runs progressed to the configured timeout without a guest exception.
