# 20260802-400 single-step hotspot 전수 dump 작업 로그 / Single-Step Hotspot Full-Table Dump Work Log

## 한국어

### 작업 요약

pumpit3 진행 정지를 판정하기 위해 single-step hotspot profile의 전체 표를 파일로 남기는
계측을 추가했습니다. 사용자와 합의한 "EIP 히스토그램" 방향입니다.

### 왜 기존 계측으로 부족했는가

`REPIU_SINGLE_STEP_HOTSPOT_PROFILE`은 이미 있었고 주소별 표본 수와 cycle을 기록합니다.
그러나 로그 출력은 count 상위 32개와 cycle 상위 32개뿐입니다
(`kWin32SingleStepHotspotReportCapacity = 32`).

정지 구간의 폴링 루틴은 포트 `0x02A8`을 200회 읽는 I/O 지연을 포함해 초당 수만 표본을
만듭니다. 240Hz로 도는 바깥 루프는 그보다 두 자릿수 드물어 상위 32개에 들어갈 수
없습니다. 즉 기존 출력으로는 "어디에 시간이 쓰였는가"는 답하지만 "그 밖에 무엇이
실행되었는가"는 답하지 못합니다. 정지 진단에 필요한 것은 두 번째 질문입니다.

표 용량 자체는 8,192개이므로 데이터는 이미 수집되고 있었고, 출력만 잘려 있었습니다.

### 변경 내용

1. `include/repiu/platform/win32/single_step_hotspot_profile.h`:
   - snapshot에 `dump_written`, `dump_entry_count`, `dump_path` 추가.
   - `ResolveSingleStepHotspotDumpPath`, `SingleStepHotspotDumpPath`,
     `WriteSingleStepHotspotDump` 선언.
2. `src/platform/win32/execution/single_step_hotspot_profile.cpp`:
   - `REPIU_SINGLE_STEP_HOTSPOT_DUMP` 해석. 미설정/`0`/`off`/`false`는 비활성,
     `1`/`on`/`true`는 `build/single_step_hotspot.txt`, 그 외는 경로로 사용.
     기존 `REPIU_GLIDE_TEX_DUMP` 규약과 같은 형태입니다.
   - 점유된 모든 항목을 표본 수 내림차순으로 기록. 헤더에 총 표본/distinct/overflow/
     총 cycle을, 각 줄에 주소·표본 수·총 cycle·최대 cycle·outcome 4종을 씁니다.
3. `src/platform/win32/telemetry/live_telemetry_snapshot.cpp`:
   - `CopyThreadObservationToAttempt`에서 dump 1회 수행. 이 함수는 중단 경로
     (`execution_trampoline.cpp:4305`)와 정상 경로(`:4381`) 각각에서 실행당 한 번만
     호출되므로 중복 기록이 없습니다.
4. `src/host/win32/main.cpp`:
   - `Win32 single-step hotspot dump written/entries/path` 로그 추가.
5. `docs/guides/execution-stall-eip-census.md`: 사용자 실행 절차, 판정 기준, 한계.

게스트 동작은 바꾸지 않았고 계측만 추가했습니다. dump는 환경변수로 명시적으로 켤 때만
동작합니다.

### 검증 결과

- 빌드: `cmake --build build --config Release --target repiu_loader_win32` 성공,
  신규 경고 없음.
- **실행 검증 대기 중.** 가이드 절차대로 pumpit3를 돌린 로그와
  `build/single_step_hotspot.txt`가 필요합니다.

### 이 계측의 한계 (해석 시 필수)

표본은 single-step 경계에서만 남습니다. AOT cache 안에서 트랩 없이 실행되는 구간은
과소 대표됩니다. 따라서 census에 있는 주소는 확실히 실행된 것이지만, 없는 주소가
실행되지 않았다고 단정할 수는 없습니다. Task 399 실행에서 `single_step`은 약 6.9M,
`heartbeat`는 약 13.9M이었으므로 경계의 약 절반을 덮습니다.

---

## English

### Summary

Added instrumentation that writes the full single-step hotspot table to a file, so the
pumpit3 stall can be judged. This is the "EIP histogram" direction agreed with the user.

### Why the existing instrumentation was not enough

`REPIU_SINGLE_STEP_HOTSPOT_PROFILE` already existed and records per-address sample counts
and cycles, but the log prints only the top 32 by count and the top 32 by cycles
(`kWin32SingleStepHotspotReportCapacity = 32`).

The polling routine in the stalled region includes an I/O delay that reads port `0x02A8` 200
times, producing tens of thousands of samples per second. An outer loop running at 240 Hz is
two orders of magnitude rarer and cannot reach a 32-entry list. The existing output answers
"where is time spent" but not "what else ran at all", and the stall diagnosis needs the
second question. The table itself holds 8,192 entries, so the data was already being
collected — only the reporting was truncated.

### Changes

1. `include/repiu/platform/win32/single_step_hotspot_profile.h`: added `dump_written`,
   `dump_entry_count`, `dump_path` to the snapshot and declared
   `ResolveSingleStepHotspotDumpPath`, `SingleStepHotspotDumpPath`,
   `WriteSingleStepHotspotDump`.
2. `src/platform/win32/execution/single_step_hotspot_profile.cpp`: parse
   `REPIU_SINGLE_STEP_HOTSPOT_DUMP` — unset/`0`/`off`/`false` disables, `1`/`on`/`true`
   selects `build/single_step_hotspot.txt`, anything else is the path, matching the existing
   `REPIU_GLIDE_TEX_DUMP` convention — and write every occupied entry ordered by sample
   count, with totals in the header and address, sample count, total cycles, max cycles, and
   the four outcome counts per line.
3. `src/platform/win32/telemetry/live_telemetry_snapshot.cpp`: perform the dump once inside
   `CopyThreadObservationToAttempt`, which is called exactly once per attempt on the
   interrupted path (`execution_trampoline.cpp:4305`) and the normal path (`:4381`), so
   there is no duplicate write.
4. `src/host/win32/main.cpp`: log `Win32 single-step hotspot dump written/entries/path`.
5. `docs/guides/execution-stall-eip-census.md`: the run procedure, how to read the result,
   and the limits.

Guest behavior is unchanged; this is instrumentation only, active only when the environment
variable is set.

### Verification results

- Build: `cmake --build build --config Release --target repiu_loader_win32` succeeded with
  no new warnings.
- **Runtime verification pending.** It needs a pumpit3 log produced by the guide procedure
  together with `build/single_step_hotspot.txt`.

### Limits of this instrumentation (required when interpreting)

Samples are taken only at single-step boundaries, so code running inside the AOT cache
without trapping is under-represented. An address present in the census definitely ran; an
address absent from it cannot be declared unreached. In the Task 399 run, `single_step` was
about 6.9M against a `heartbeat` of about 13.9M, so roughly half of the boundaries are
covered.
