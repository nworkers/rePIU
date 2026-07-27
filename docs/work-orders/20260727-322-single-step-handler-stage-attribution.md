# 20260727-322 작업 지시: Single-step handler 단계별 비용 귀속 / Work order

설계: [docs/design/20260727-322-single-step-handler-stage-attribution.md](../design/20260727-322-single-step-handler-stage-attribution.md)

## 한국어

### 목표

`HandleSingleStepTrace` 내부를 상호 배타적인 5개 단계로 나누어 count와 TSC tick을
전역 및 guest EIP별로 기록합니다. Task 309가 미확정으로 남긴 "HLE tick 84.82%가
emulate 본체인지 AOT 재진입인지"를 판정합니다. 관측 전용이며 실행 의미를 바꾸지
않습니다.

### 구현 항목

1. `include/repiu/platform/win32/single_step_hotspot_profile.h`
   - `SingleStepProfileStage` enum과 `kSingleStepProfileStageCount` 추가.
   - `Win32SingleStepHotspotEntry`, `Win32SingleStepHotspotProfile`,
     `Win32SingleStepHotspotSample`, `Win32SingleStepHotspotProfileSnapshot`에
     `stage_counts`/`stage_cycles` 배열 추가.
   - `SingleStepHotspotCycleScope`에 단계 누적 API 추가.
   - RAII `SingleStepHotspotStageScope` 추가.
2. `src/platform/win32/execution/single_step_hotspot_profile.cpp`
   - `RecordSingleStepHotspot`이 단계 배열을 함께 누적하도록 확장.
   - `MakeSample`과 snapshot이 단계 배열을 전달하도록 확장.
   - 부모 scope 비활성 시 `__rdtsc`를 호출하지 않는 단계 scope 구현.
3. `src/platform/win32/execution/execution_trampoline.cpp`
   - `HandleSingleStepTrace`의 다섯 구간을 각각 단계 scope로 감쌈.
   - prologue, HLE dispatch, AOT resume, interrupt injection, native entry.
4. `src/tools/aot_probe/single_step_hotspot_profile_probe.cpp`
   - 단계 누적, 전역/entry 일치, `sum(stage_cycles) <= total_cycles`,
     비활성 시 무누적을 검증.
5. `src/host/win32/main.cpp`
   - 종료 summary에 전역 단계별 count/cycle과 residual을 출력.
   - cycle hotspot 항목에 단계별 cycle을 출력.

### 안전 조건

- guest에게 보이는 실행 순서, EIP, EFLAGS, 반환값을 바꾸지 않습니다.
- 기존 outcome 분류와 그 회계를 바꾸지 않습니다.
- 단계 scope는 heap 할당, 문자열 포맷, 파일 I/O를 하지 않습니다.
- profile 기본값은 OFF를 유지합니다.
- 단계 구간은 중첩하지 않고 순차로만 배치해 이중 계상을 막습니다.
- `residual`은 저장하지 않고 보고 시점에 파생하며 음수가 될 수 없습니다.

### 검증

1. `powershell -File scripts/build_win32_x86.ps1` (Win32 x86 Debug 전체 빌드)
2. `build/win32_x86_debug/Debug/repiu_aot_probe.exe` 전체 통과
3. `REPIU_SINGLE_STEP_HOTSPOT_PROFILE=1` 60초 `aot-dbt` 실행에서 단계 분포 확보
4. profile OFF 실행과 EEPROM hash 일치, fatal 0 확인

진행도(progress) 비교는 판정 근거로 사용하지 않습니다.

---

## English

### Goal

Split `HandleSingleStepTrace` into five mutually exclusive stages and record count
and TSC ticks per stage, globally and per guest EIP, to settle whether the 84.82%
HLE tick share found in Task 309 belongs to the emulation body or the AOT re-entry
path. Observation only; execution semantics are unchanged.

### Implementation

1. Add `SingleStepProfileStage`, stage arrays on the entry/profile/sample/snapshot
   structures, a stage accumulation API on `SingleStepHotspotCycleScope`, and a RAII
   `SingleStepHotspotStageScope` in the profile header.
2. Extend `RecordSingleStepHotspot`, `MakeSample`, and the snapshot to carry stage
   arrays, and implement the stage scope so an inactive parent issues no `__rdtsc`.
3. Wrap the five regions of `HandleSingleStepTrace` in stage scopes.
4. Extend `single_step_hotspot_profile_probe.cpp` to verify stage accumulation,
   entry/global agreement, `sum(stage_cycles) <= total_cycles`, and no accumulation
   when disabled.
5. Print global stage counts/cycles plus residual in the loader summary, and
   per-stage cycles on cycle hotspot rows.

### Safety

Do not change guest-visible ordering, EIP, EFLAGS, or return values; do not change
the existing outcome classification or its accounting; perform no allocation, string
formatting, or file I/O inside stage scopes; keep the profile off by default; keep
stages sequential rather than nested so no interval is double counted; derive
`residual` at report time, where it can never be negative.

### Verification

Run the full Win32 x86 Debug build, pass `repiu_aot_probe`, capture the stage
distribution from a 60-second `aot-dbt` run with the profile enabled, and confirm a
matching EEPROM hash and zero fatal events against a profile-off run. Progress is not
used as evidence.
