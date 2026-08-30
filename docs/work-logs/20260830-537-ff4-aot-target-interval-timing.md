# 20260830-537 FF4 AOT 대상 구간 계측 작업 로그

## 결과 요약

FF4 resolved target을 기존 single-step hotspot과 다른 전용 경계로 계측하는 데
성공했습니다. 결과는 순수 target instruction cycle이 아니라 AOT target interval로
기록했습니다.

## 수행 내용

* 설계 문서와 작업지시서를 먼저 작성했습니다.
* `AotFfTargetTimingProfile`과 16개 고정 aggregate 슬롯을 추가했습니다.
* `REPIU_AOT_FF_TARGET_TIMING=1` 환경변수로 cycle counter 계측을 opt-in 처리했습니다.
* FF4 target sampler에서 후보를 만들고, `HandleAotReentry`의 matching resolved path에서
  구간을 시작하도록 연결했습니다.
* `DispatchGuestFault` 공통 진입점에서 다음 exception 기준으로 구간을 종료했습니다.
* live reporter에 `[repiu-live-ff-target]` 독립 라인을 추가했습니다.
* 기존 AOT probe에 candidate/match/100-cycle aggregate 검증을 추가했습니다.
* 원본 EXE와 guest code는 변경하지 않았습니다.

## 검증

* Windows x86 Debug: `cmake --build build\\win32_x86_debug --config Debug --target repiu_aot_probe --parallel 1` 성공.
* Linux i386 Release: `wsl.exe -d Ubuntu-24.04 -- cmake --build /mnt/e/MYWORK/Projects/rePIU/build/linux_i386 --parallel 2` 성공.
* 합성 probe: `aot_ff_boundary_target_attribution=true`,
  `aot_ff_target_timing=true`, `aot_boundary_opcode_census_all=true`.
* 두 실측은 모두 timeout cleanup으로 종료되었으며 shutdown 상태는
  `attempts=40 answered=1 recovered=0 stopped=0 failure=0`입니다.

## 실측 조건

두 title에 다음 공통 환경을 적용했습니다.

```text
REPIU_STALL_TIMEOUT_MS=0
REPIU_EXECUTION_TIMEOUT_MS=60000
REPIU_GLIDE_SWAP_INTERVAL=0
REPIU_GLIDE_FRAME_RATE_LOG=1
REPIU_EXECUTION_TIME_PROFILE=1
REPIU_LIVE_PROFILE_INTERVAL_MS=10000
REPIU_AOT_FF_TARGET_TIMING=1
```

표준 오류 스트림 short write를 피하기 위해 WSL 내부 파일 redirection을 사용했습니다.

## 실측 결과

| title | FF4 resolved | started/completed | mismatch | active | overflow | total cycles |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `pumpipx3` | 15,811 | 15,799 / 15,799 | 12 | 0 | 0 | 785,219,364 |
| `pumpit1` | 90 | 90 / 90 | 0 | 0 | 0 | 144,273,952 |

`pumpipx3` 주요 key:

* `0x010EF6DE -> 0x010EF8E9`, EDX index 7: n=15,616,
  sum=762,449,236, avg≈48,824.87, min=10,730, max=14,939,564.
* `0x010EF6DE -> 0x010EF6E6`, EDX index 0: n=61,
  sum=781,696, avg≈12,814.69.

`pumpit1` 주요 key:

* `0x010F1DD7 -> 0x010F1CFD`, EAX index 3: n=41,
  sum=18,193,868, avg≈443,752.88, min=33,781, max=5,934,652.
* `0x010F1DD7 -> 0x010F1CF4`, EAX index 2: n=49,
  sum=126,080,084, avg≈2,573,062.94, min=26,862, max=31,553,082.

## 해석과 미해결점

`pumpipx3`의 12 mismatch를 제외하면 FF4 resolved sample과 AOT target interval이
연결되었고, 두 title 모두 active interval이나 슬롯 overflow가 남지 않았습니다. 하지만
interval total은 guest-run cycle의 `pumpipx3≈0.3886%`, `pumpit1≈0.0730%`에 불과합니다.
따라서 이 계측만으로 pumpipx3 후반의 낮은 FPS나 높은 VEH 비율을 설명할 수 없습니다.

최종 live window에서 AOT boundary는 pumpipx3 390,483회, pumpit1 27,431회였고 FF4
resolved sample은 각각 15,811회와 90회였습니다. 이는 AOT boundary churn이 더 큰
분석 축이라는 상관 증거입니다. 12 mismatch의 구체적인 경로와 boundary churn의
producer/exception causality는 다음 작업으로 남겼습니다.

## 산출물

* 설계: `docs/design/20260830-537-ff4-aot-target-interval-timing.md`
* 작업지시: `docs/work-orders/20260830-537-ff4-aot-target-interval-timing.md`
* 누적 분석: `docs/analysis/current-execution-frontier.md`,
  `docs/analysis/runtime-aot-dynamic-translation.md`

---

# 20260830-537 Work Log: FF4 AOT Target-Interval Timing

## Summary

The resolved FF4 target was measured through a dedicated boundary distinct from the existing
single-step hotspot. The result is recorded as an AOT target interval, not as pure target
instruction cycles.

## Work performed

* Wrote the design and work-order documents before implementation.
* Added `AotFfTargetTimingProfile` with 16 fixed aggregate slots.
* Made cycle-counter instrumentation opt-in through `REPIU_AOT_FF_TARGET_TIMING=1`.
* Created candidates in the FF4 target sampler and began matching intervals in the resolved
  `HandleAotReentry` path.
* Completed intervals at the common `DispatchGuestFault` entry on the following exception.
* Added a separate `[repiu-live-ff-target]` live-report line.
* Extended the existing AOT probe to verify candidate matching and a 100-cycle aggregate.
* Did not modify the original executable or guest code.

## Verification

* Windows x86 Debug build for `repiu_aot_probe`: passed.
* Linux i386 Release build: passed.
* Synthetic probe: `aot_ff_boundary_target_attribution=true`,
  `aot_ff_target_timing=true`, and `aot_boundary_opcode_census_all=true`.
* Both runtime measurements ended at the known timeout cleanup boundary:
  `attempts=40 answered=1 recovered=0 stopped=0 failure=0`.

## Measurements

Both titles used the Task 536 conditions with only `REPIU_AOT_FF_TARGET_TIMING=1` added. WSL
internal file redirection preserved complete live lines after the first pipe-based attempt
showed a short-write truncation.

Pumpipx3 produced 15,811 resolved FF4 samples and 15,799 started/completed intervals, with
12 mismatches, no active interval, no discard, and no slot overflow. Its dominant index-7
pair `0x010EF6DE -> 0x010EF8E9` contributed 15,616 intervals and 762,449,236 cycles
(`avg≈48,824.87`). Its index-0 target contributed 61 intervals and 781,696 cycles
(`avg≈12,814.69`).

Pumpit1 produced 90 resolved and completed intervals with no mismatch, active interval,
discard, or overflow. Its index-3 pair contributed 41 intervals and 18,193,868 cycles
(`avg≈443,752.88`); its index-2 pair contributed 49 intervals and 126,080,084 cycles
(`avg≈2,573,062.94`).

The interval totals were approximately 0.3886% and 0.0730% of the respective guest-run
cycles, so this boundary alone does not explain pumpipx3's late low FPS or high VEH share.
The final live windows still show the larger difference in AOT boundary volume (390,483 versus
27,431) and FF4 sample volume (15,811 versus 90). This is correlation only. The 12 mismatches
and the producer/exception causality behind the boundary churn remain open.

## Artifacts

* Design: `docs/design/20260830-537-ff4-aot-target-interval-timing.md`
* Work order: `docs/work-orders/20260830-537-ff4-aot-target-interval-timing.md`
* Analysis: `docs/analysis/current-execution-frontier.md` and
  `docs/analysis/runtime-aot-dynamic-translation.md`
