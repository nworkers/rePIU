# AOT 간접 인라인 캐시 동일 바이너리 A/B 작업 지시

## 한국어

### 작업 범위

1. 플랫폼 공용 AOT code-cache build options에 간접 call/jump entry 수를 추가합니다.
2. 기본 4슬롯과 진단용 1슬롯을 동일 binary에서 선택하게 합니다.
3. 정적 image의 정책을 Win32 placement와 dynamic append까지 전달합니다.
4. EEPROM 경로 override를 추가하여 매 run 상태를 격리합니다.
5. 의미 기반 Glide 이정표와 AOT 경계 지표를 수집하는 PowerShell 벤치마크를 추가합니다.
6. 동일 binary A/B를 실행하고 결과를 분석·아키텍처·작업 로그에 반영합니다.

### 비범위

- return cache 슬롯 수 변경
- 슬롯 수의 자동 적응 또는 LRU
- 원본 실행 파일, 게임 로직, DOS/DPMI/Glide 의미 변경
- `progress`를 게임 진행 지표로 사용하는 비교

### 완료 조건

- 미설정/1/4/잘못된 환경변수 정책이 결정론적으로 검증됨
- 정적·동적 AOT image가 선택한 슬롯 수를 일관되게 사용
- 기본 4슬롯 probe와 1슬롯 layout probe 통과
- Win32 x86 Debug 전체 빌드 성공
- 격리된 EEPROM 사본을 사용하는 동일 바이너리 A/B 결과 생성
- 문서 갱신과 Git 커밋 완료

## English

### Scope

1. Add indirect call/jump entry count to platform-neutral AOT build options.
2. Select default four-entry or diagnostic one-entry mode in the same executable.
3. Propagate the static-image policy through Win32 placement and dynamic append.
4. Add an EEPROM path override for isolated runs.
5. Add a PowerShell benchmark for semantic Glide milestones and AOT boundary metrics.
6. Run the controlled A/B and update analysis, architecture, and the work log.

### Out of scope

- Return-cache entry changes
- Adaptive sizing or LRU
- Original executable, gameplay, DOS/DPMI, or Glide semantic changes
- Treating `progress` as game progress

### Completion criteria

- Unset/1/4/invalid environment policies are deterministic.
- Static and dynamic AOT images consistently use the selected count.
- Default four-entry and diagnostic one-entry probes pass.
- The full Win32 x86 Debug build passes.
- A same-binary A/B result is produced with isolated EEPROM copies.
- Documentation and Git commit are complete.
