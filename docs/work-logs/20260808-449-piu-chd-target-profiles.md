# 20260808-449 PIU CHD 타깃 프로파일 확장 작업 로그 / PIU CHD Target Profile Expansion Work Log

설계: [20260808-449-piu-chd-target-profiles.md](../design/20260808-449-piu-chd-target-profiles.md)

작업 지시: [20260808-449-piu-chd-target-profiles.md](../work-orders/20260808-449-piu-chd-target-profiles.md)

## 한국어

### 결과

- 내장 레지스트리에 `pumpito`, `pumpitc`, `pumpitpc`, `pumpite`를 요청 순서대로 추가했습니다.
- 모든 프로파일은 같은 ID의 ROM-set, `piu_common` HLE, 공용 PIU CHD mount, 기존 MK3
  runtime reservation을 사용합니다.
- 공용 CHD reader가 여러 data track 중 마지막 track을 선택하도록 일반화했습니다.
  기존 한 data-track 및 audio+data 디스크 동작은 유지됩니다.
- 아키텍처, README, 설계, 분석 색인과 `pumpite` 누적 분석을 갱신했습니다.

### 검증

- `cmake --build build\win32_x86_debug --config Debug`: 성공. 전체 타깃이 빌드됐습니다.
  기존 외부 헤더/소스의 C4819 경고는 있었으나 컴파일·링크 오류는 없었습니다.
- `repiu_exe_analyzer pumpito`: 프로파일 선택 성공 후 `pumpito CHD directory not found`.
- `repiu_exe_analyzer pumpitc`: 프로파일 선택 성공 후 `pumpitc CHD directory not found`.
- `repiu_exe_analyzer pumpitpc`: 프로파일 선택 성공 후 `pumpitpc CHD directory not found`.
- `repiu_exe_analyzer pumpite`: 마지막 data track mount와 파일 추출 후 성공. 실행 파일은
  1,795,551 bytes이고 relocation failure는 0건입니다.
- 회귀: `repiu_exe_analyzer pumpit1`, `pumpit2`, `pumpit3` 모두 exit 0.
- `git diff --check`: 통과.

### 남은 범위

`pumpito`, `pumpitc`, `pumpitpc`는 로컬 CHD가 없어 실제 mount와 guest 실행을 검증하지
못했습니다. `pumpite`도 이번 작업에서는 analyzer까지 확인했으며 guest 기동과 플레이 검증은
별도 bring-up 작업으로 남깁니다.

## English

### Results

- Added `pumpito`, `pumpitc`, `pumpitpc`, and `pumpite` to the built-in registry in the requested
  order.
- Every profile uses the matching ROM-set ID, `piu_common` HLE, shared PIU CHD mount, and existing
  MK3 runtime reservation.
- Generalized the shared CHD reader to select the last of multiple data tracks while preserving
  existing single-data-track and audio-plus-data behavior.
- Updated the architecture, README, design, analysis index, and cumulative `pumpite` analysis.

### Verification

- `cmake --build build\win32_x86_debug --config Debug`: passed for all targets. Existing C4819
  warnings from external/current sources remained, with no compile or link error.
- `repiu_exe_analyzer pumpito`: selected the profile, then reported its missing CHD directory.
- `repiu_exe_analyzer pumpitc`: selected the profile, then reported its missing CHD directory.
- `repiu_exe_analyzer pumpitpc`: selected the profile, then reported its missing CHD directory.
- `repiu_exe_analyzer pumpite`: successfully mounted and extracted the last data track, then
  analyzed the 1,795,551-byte executable with zero relocation failures.
- Regression: `repiu_exe_analyzer pumpit1`, `pumpit2`, and `pumpit3` all exited 0.
- `git diff --check`: passed.

### Remaining Scope

Local CHDs for `pumpito`, `pumpitc`, and `pumpitpc` were unavailable, so their actual mounts and
guest execution remain unverified. This task verified `pumpite` through analyzer completion;
guest startup and gameplay remain a separate bring-up task.
