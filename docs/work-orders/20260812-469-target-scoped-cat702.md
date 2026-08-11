# 20260812-469 TargetProfile별 CAT702 capability 작업 지시 / Target-Scoped CAT702 Capability Work Order

설계: [20260812-469-target-scoped-cat702](../design/20260812-469-target-scoped-cat702.md)

## 한국어

### 목표

CAT702 활성 여부를 PIU10 보드 전체와 분리하여 target profile에서 결정하고, 비활성화 시
원본 보안 검사가 성공하지 못하도록 합니다.

### 작업 항목

- [x] 기존 target profile, PIU10 초기화, CAT702 직렬 경로를 확인합니다.
- [x] CAT702 비활성화의 data-line 및 자산 로딩 정책을 설계합니다.
- [x] `TargetProfile::enable_cat702`와 내장 profile 기본값을 추가합니다.
- [x] capability를 Win32 실행 준비와 PIU10 보드 초기화까지 전달합니다.
- [x] 비활성 상태에서 CAT702만 차단하고 flash, MP3, DAC 기능을 보존합니다.
- [x] 활성/비활성 CAT702 회귀 probe를 추가합니다.
- [x] 관련 아키텍처와 분석 문서를 갱신합니다.
- [x] Win32 x86 Debug 빌드와 전체 probe를 수행합니다.
- [x] 작업 로그를 작성하고 커밋합니다.

## English

### Objective

Control CAT702 independently from the entire PIU10 board through each target profile, ensuring
that the original security check cannot succeed when CAT702 is disabled.

### Work Items

- [x] Inspect existing target-profile, PIU10 initialization, and CAT702 serial paths.
- [x] Design disabled CAT702 data-line and asset-loading policy.
- [x] Add `TargetProfile::enable_cat702` and built-in profile defaults.
- [x] Carry the capability through Win32 setup into PIU10 board initialization.
- [x] Disable only CAT702 while preserving flash, MP3, and DAC behavior.
- [x] Add enabled and disabled CAT702 regression probes.
- [x] Update related architecture and analysis documentation.
- [x] Run the Win32 x86 Debug build and complete probe.
- [x] Write the work log and commit.
