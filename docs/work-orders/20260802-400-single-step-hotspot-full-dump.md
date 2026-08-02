# 20260802-400 single-step hotspot 전수 dump 계획 / Single-Step Hotspot Full-Table Dump Plan

## 한국어

### 목표

pumpit3 진행 정지의 원인을 판정하기 위해, single-step hotspot profile의 **전체 표**를
파일로 남긴다. 기존 로그는 count/cycle 상위 32개만 출력하는데, 뜨거운 폴링 루프보다 두
자릿수 드물게 도는 바깥 루프는 그 목록에 들어갈 수 없어 정지 진단에 쓸 수 없다.

### 배경

Task 399 로그에서 pumpit3는 크래시 없이 66초를 돌았지만 `last_eip`가 계속
`0x0301DB1F`~`0x0301DB2A`(입력 폴링 루틴 `0x0301DB10`)에 있었고 `progress`는 `7591`에
고정됐다. 폴링은 포트 `0x02A8`을 200회 읽는 I/O 지연을 포함하므로 초당 수만 표본을
만들고, `last_eip` 단일 표본만으로는 이것이 원인인지 증상인지 구분되지 않는다.

### 작업 범위

1. `include/repiu/platform/win32/single_step_hotspot_profile.h`:
   - snapshot에 `dump_written`, `dump_entry_count`, `dump_path` 추가.
   - `ResolveSingleStepHotspotDumpPath`, `SingleStepHotspotDumpPath`,
     `WriteSingleStepHotspotDump` 선언.
2. `src/platform/win32/execution/single_step_hotspot_profile.cpp`:
   - `REPIU_SINGLE_STEP_HOTSPOT_DUMP` 해석과 전수 dump 구현.
     점유된 모든 항목을 표본 수 내림차순으로 기록.
3. `src/platform/win32/telemetry/live_telemetry_snapshot.cpp`:
   - `CopyThreadObservationToAttempt`에서 dump 1회 수행. 이 함수는 중단 경로와 정상
     경로 모두에서 실행당 정확히 한 번 호출된다.
4. `src/host/win32/main.cpp`:
   - `Win32 single-step hotspot dump written/entries/path` 로그 추가.
5. `docs/guides/execution-stall-eip-census.md`: 사용자 실행 절차.
6. 설계·작업 로그 문서.

### 검증 절차

1. 빌드: `cmake --build build --config Release --target repiu_loader_win32`
2. `REPIU_SINGLE_STEP_HOTSPOT_PROFILE=1 REPIU_SINGLE_STEP_HOTSPOT_DUMP=1`로 pumpit3 실행,
   정지 상태를 30초 이상 관측 후 창을 닫아 정상 teardown
3. 로그의 `dump written/entries/path`가 `true`이고 `overflow`가 0인지 확인
4. dump 파일의 주소 분포로 폴링 루틴 밖 실행 여부를 판정

계측만 추가하며 게스트 동작은 바꾸지 않는다.

---

## English

### Objective

Write the **entire** single-step hotspot table to a file so the pumpit3 stall can be
diagnosed. The existing log prints only the top 32 by count and by cycles, and an outer loop
running two orders of magnitude less often than the hot polling loop cannot reach those
lists.

### Background

In the Task 399 log, pumpit3 ran 66 seconds without crashing while `last_eip` stayed within
`0x0301DB1F`-`0x0301DB2A` (the input polling routine at `0x0301DB10`) and `progress` stayed
at `7591`. The poll includes an I/O delay reading port `0x02A8` 200 times, producing tens of
thousands of samples per second, so a single `last_eip` sample cannot separate cause from
symptom.

### Task Scope

1. `include/repiu/platform/win32/single_step_hotspot_profile.h`: add `dump_written`,
   `dump_entry_count`, `dump_path` to the snapshot and declare
   `ResolveSingleStepHotspotDumpPath`, `SingleStepHotspotDumpPath`,
   `WriteSingleStepHotspotDump`.
2. `src/platform/win32/execution/single_step_hotspot_profile.cpp`: parse
   `REPIU_SINGLE_STEP_HOTSPOT_DUMP` and write every occupied entry ordered by sample count.
3. `src/platform/win32/telemetry/live_telemetry_snapshot.cpp`: perform the dump once in
   `CopyThreadObservationToAttempt`, which runs exactly once per attempt on both the
   interrupted and normal teardown paths.
4. `src/host/win32/main.cpp`: log `Win32 single-step hotspot dump written/entries/path`.
5. `docs/guides/execution-stall-eip-census.md`: the procedure the user runs.
6. Design and work-log documents.

### Verification Procedure

1. Build: `cmake --build build --config Release --target repiu_loader_win32`
2. Run pumpit3 with `REPIU_SINGLE_STEP_HOTSPOT_PROFILE=1` and
   `REPIU_SINGLE_STEP_HOTSPOT_DUMP=1`, observe the stall for 30 seconds or more, then close
   the window for a normal teardown.
3. Confirm the log shows `dump written/entries/path` as `true` with `overflow` at zero.
4. Judge from the address distribution whether execution leaves the polling routine.

This adds instrumentation only and does not change guest behavior.
