# 20260802-396 pumpit3 ZIP+CHD 프로필 추가 작업 로그 / pumpit3 ZIP+CHD Profile Addition Work Log

## 한국어

### 작업 요약

`roms/pumpit3.zip` 및 `roms/pumpit3/20000508 the o-b-g.chd` 자산을 기반으로 타겟 런타임 디렉터리를 구축할 수 있도록 내장 프로필 목록에 `pumpit3` 프로필을 추가하였습니다.

### 변경 내용

1. `src/target/target_profile.cpp`:
   - `BuiltInTargetProfiles()`에 `pumpit3` 타겟 프로필 정의 추가.
   - `rom_set_id`: `"pumpit3"`, `id`: `"pumpit3"`, `name`: `"Pump It Up The O.B.G: The 3rd Dance Floor (MAME CHD)"`.
2. `docs/design/20260802-396-pumpit3-zip-chd-profile.md`:
   - `pumpit3` 자산 구조 분석 결과 및 공용 mount 파이프라인 활용 설계 기록.
3. `docs/work-orders/20260802-396-pumpit3-zip-chd-profile.md`:
   - 작업 계획 수립.

### 검증 결과

- `exe_analyzer --target pumpit3` 실행: `roms/pumpit3.zip`과 `roms/pumpit3/20000508 the o-b-g.chd`에서 디스크 65번 트랙(MODE2_RAW)을 통해 PVD bias 보정 및 파일 시스템 추출이 완료되어 `build/runtime_mounts/pumpit3/PIU/PIU.EXE`가 정상적으로 materialized 되었음을 확인.
- `repiu_host --target pumpit3 --smoke-test-frames 120` 실행: 호스트 런타임 호환성, MSCDEX 오디오 64트랙 매핑, YMZ280B 샘플 ROM 접근 및 프레임 실행 확인.
- 기존 `pumpit1`, `pumpit2` 타겟 회귀 없음 확인.

---

## English

### Summary

Added the `pumpit3` built-in target profile to construct the runtime directory using `roms/pumpit3.zip` and `roms/pumpit3/20000508 the o-b-g.chd`.

### Changes

1. `src/target/target_profile.cpp`:
   - Added `pumpit3` target profile definition to `BuiltInTargetProfiles()`.
   - `rom_set_id`: `"pumpit3"`, `id`: `"pumpit3"`, `name`: `"Pump It Up The O.B.G: The 3rd Dance Floor (MAME CHD)"`.
2. `docs/design/20260802-396-pumpit3-zip-chd-profile.md`:
   - Recorded asset analysis and shared mount pipeline reuse design.
3. `docs/work-orders/20260802-396-pumpit3-zip-chd-profile.md`:
   - Documented task work order.

### Verification Results

- Ran `exe_analyzer --target pumpit3`: Verified `roms/pumpit3.zip` and `roms/pumpit3/20000508 the o-b-g.chd` (Track 65 MODE2_RAW) materialized `build/runtime_mounts/pumpit3/PIU/PIU.EXE` with PVD bias calibration.
- Ran `repiu_host --target pumpit3 --smoke-test-frames 120`: Confirmed host runtime initialization, MSCDEX audio 64-track mapping, YMZ280B sample ROM loading, and clean frame progression.
- Confirmed no regressions on `pumpit1` and `pumpit2` targets.

### 2026-08-02 정정 / Correction

`pumpit3` 프로필의 `name`을 `"Pump It Up The O.B.G. 3rd SE (MAME CHD)"`에서
`"Pump It Up The O.B.G: The 3rd Dance Floor (MAME CHD)"`로 정정했습니다. 최초 기재한
"3rd SE"는 잘못된 판본 표기였습니다. 표시 문자열만 바뀌며 `id`, `rom_set_id`, 경로,
mount 동작은 그대로입니다. 위 본문의 `name` 기재도 정정된 값으로 갱신했습니다.

Corrected the `pumpit3` profile `name` from `"Pump It Up The O.B.G. 3rd SE (MAME CHD)"`
to `"Pump It Up The O.B.G: The 3rd Dance Floor (MAME CHD)"`; the original "3rd SE" was
the wrong revision label. Only the display string changes — `id`, `rom_set_id`, paths,
and mount behavior are unaffected. The `name` values quoted above were updated to match.
