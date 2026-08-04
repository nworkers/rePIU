# 실행 정지 지점 EIP census 가이드 / Capturing an EIP census for an execution stall

이 문서는 **크래시 없이 진행만 멈춘 실행**에서 게스트가 실제로 어떤 코드를 돌고
있는지 전수 기록하는 절차입니다. 로그의 `last_eip`는 1초에 한 번 찍히는 단일 표본이라
"뜨거운 루프" 하나만 보여 주고, 그보다 두 자릿수 드물게 도는 바깥 루프는 보이지
않습니다. 이 census는 그 차이를 메웁니다.

## 1. 언제 쓰는가

- 종료하지 않지만 화면이 진행하지 않을 때
- `last_eip`가 계속 같은 좁은 구간에 있는데 그 구간이 원인인지 증상인지 모를 때
- 특정 함수가 "호출되고 있는지 아닌지" 자체를 확정해야 할 때

## 2. 실행

```
set REPIU_EXECUTION_BACKEND=aot-dbt
set REPIU_EXECUTION_TIMEOUT_MS=45000
set REPIU_SINGLE_STEP_HOTSPOT_PROFILE=1
set REPIU_SINGLE_STEP_HOTSPOT_DUMP=1
build\Release\repiu_loader_win32.exe pumpit3 > repiu_log.txt 2>&1
```

`REPIU_EXECUTION_TIMEOUT_MS`를 45000처럼 지정하면 그 시간 뒤 스스로 종료하므로 창을
닫지 않아도 됩니다. dump는 게스트 스레드가 멈춘 직후, Glide close와 worker 종료보다
**먼저** 기록되므로 이후 teardown이 지연되거나 멈춰도 census는 남습니다(Task 401).
작업 관리자로 프로세스를 강제 종료하면 기록되지 않습니다.

`REPIU_SINGLE_STEP_HOTSPOT_DUMP`는 `1`이면 `build/single_step_hotspot.txt`에 쓰고,
다른 값을 주면 그 값을 경로로 사용합니다. 비우면 dump하지 않습니다.

## 3. 결과 확인

로그에서:

```
Win32 single-step hotspot enabled/total/distinct/overflow: true/<표본>/<주소 수>/0
Win32 single-step hotspot dump written/entries/path: true/<항목 수>/build\single_step_hotspot.txt
```

`overflow`가 0이 아니면 8,192개 표 용량을 넘긴 것이므로 census가 불완전합니다.

dump 파일 형식은 표본 수 내림차순 한 줄에 한 주소입니다.

```
# rePIU single-step hotspot dump
# total_samples=... distinct=... overflow=... total_cycles=...
# guest_address sample_count total_cycles max_cycles hle timer native tf
0x0301DB22 2912345 ... ... ... ... ... ...
```

## 4. 판정

| 관측 | 해석 |
|---|---|
| `distinct`가 수십 개이고 전부 한 함수 범위 안 | 게스트가 그 함수에 갇혀 있음. 바깥 루프의 조건을 찾아야 함 |
| `distinct`가 수백~수천이고 뜨거운 루프 밖에도 표본이 넓게 퍼짐 | 게스트는 여러 코드를 돌고 있음. 정지는 특정 조건 대기이지 실행 정지가 아님 |
| 뜨거운 루프 다음 순위 주소들이 한 함수에 모임 | 그 함수가 대기 루프 후보. 해당 주소를 원본 실행 파일에서 역어셈블해 조건을 읽음 |

주소를 원본 파일 offset으로 바꾸려면 로그의
`Relocated exception byte base`와 byte window로 확인한 object delta를 씁니다.
pumpit3 object 2의 delta는 `0x02FF4E00`입니다(guest = file + delta).

## 4b. 캐시 실행까지 보려면 — 게스트 위치 census (Task 411)

위 census는 single-step 경계에서만 표본하므로 **예외 없이 AOT 캐시에서 도는 대기
루프를 보지 못합니다.** 그 경우 시간 기준 census를 씁니다.

```
set REPIU_EXECUTION_BACKEND=aot-dbt
set REPIU_EXECUTION_TIMEOUT_MS=60000
set REPIU_GUEST_POSITION_CENSUS=1
set REPIU_GUEST_POSITION_CENSUS_MS=10
set REPIU_GUEST_POSITION_CENSUS_DUMP=build\guest_position_census.txt
cmd /c "build\win32_x86_debug\Release\repiu_loader_win32.exe pumpit3 > run.txt 2>&1"
```

반복 실행과 멈춤/정상 판정은 `scripts/task411_guest_position_census.ps1`이 자동으로
합니다(EEPROM 실행별 격리 포함).

읽는 줄:

```
Win32 guest position census enabled/total/distinct/overflow/capture-failures/interval-ms:
Win32 guest position origin arena/cache-mapped/cache-unmapped/host/sum-matches-total:
Win32 guest position top #N address/count/share/arena/cache/cache-unmapped/host:
```

**두 검산을 먼저 봅니다.** `sum-matches-total`이 `true`가 아니거나 `overflow`가 0이
아니면 분포로 읽지 않습니다.

| 관측 | 해석 |
|---|---|
| 상위 주소가 한 함수 범위에 모임 | 그 함수가 대기 루프. `repiu_aot_probe --dump`(주소 `-0x02000000`)로 탈출 조건 확인 |
| 상위가 지연 루프(`0x0301DB1F`~`0x0301DB2A`)뿐 | 타이머 ISR만 본 것. 간격을 tick 주기와 서로소로 바꿔 재측정 |
| `host` 비중이 지배적 | 게스트가 아니라 호스트에서 대기 중 |

host 표본이 많으면 **Task 412가 더한 세 줄**을 이어서 읽습니다.

```
Win32 guest position thread time valid/kernel-ms/user-ms/wall-ms/cpu-share:
Win32 guest position host scan samples/sited/no-site/failed/distinct/overflow/parts-match:
Win32 guest position host site #N address/count/share-of-sited/module/offset/symbol:
```

| 관측 | 해석 |
|---|---|
| `cpu-share` ≥ 90% | **바쁨.** host 시간은 커널 예외 dispatch. 축은 예외 가격·횟수 |
| `cpu-share` ≤ 50% | **막힘.** 축은 대기 지점이며 `host site`가 그 지점을 가리킴 |
| `parts-match`가 false | 스캔 계상이 깨진 것. **분포로 읽지 않습니다** |
| `no-site` 비중이 큼 | 그 표본들의 `ESP`가 로더 스택이 아님(게스트 스택 위 예외). 그 자체가 자료 |
| `host site`가 흩어짐 | 얕은 훑기의 한계. 정식 스택 워크나 사이트별 scope 계측으로 넘어감 |

**이 census를 켠 실행의 wall·프레임은 인용하지 않습니다**(suspend/resume 비용이 붙음).
`REPIU_NATIVE_SAMPLING`의 `[repiu-sample]` 줄과 혼동하지 마십시오 — 그쪽은 예외
dispatch가 1초간 조용해야 발화하므로 멈춘 실행에서는 나오지 않습니다.

## 5. 한계 (해석 시 반드시 고려)

이 census는 **single-step 경계에서만** 표본을 남깁니다. AOT cache 안에서 트랩 없이
실행되는 구간은 과소 대표됩니다. 따라서

- 어떤 주소가 census에 **있으면** 그 코드는 확실히 실행됐습니다.
- 어떤 주소가 census에 **없다고** 실행되지 않았다고 단정할 수는 없습니다.

**census의 `total_cycles`를 비용 근거로 쓰지 마십시오.** 이 값은 single-step 핸들러
scope만 재며, 실제 실행에서 wall clock의 몇 퍼센트에 불과합니다(Task 402 측정에서
2.04%). "census의 95%"는 "비용의 95%"가 아닙니다. 비용 판정에는
`REPIU_EXECUTION_TIME_PROFILE`의 `Win32 execution time cycles ... guest-run` 대비 버킷
비중을 쓰십시오. Task 401이 이 구분을 놓쳐 잘못된 대상을 지목했고 Task 402가
정정했습니다.

로그의 `single_step`과 `heartbeat` 비율로 census가 전체 경계의 몇 퍼센트를 덮는지
먼저 확인하고 해석하십시오.

---

# Capturing an EIP census for an execution stall

This is the procedure for recording, exhaustively, which guest code is actually running in
a run that **stalls without crashing**. The `last_eip` in the log is a single sample taken
once per second, so it shows only the hottest loop and hides an outer loop running two
orders of magnitude less often. This census closes that gap.

## 1. When to use it

- The run does not terminate but the screen does not advance.
- `last_eip` stays in one narrow range and it is unclear whether that range is the cause or
  a symptom.
- You need to establish whether a particular function is being called at all.

## 2. Run

```
set REPIU_EXECUTION_BACKEND=aot-dbt
set REPIU_EXECUTION_TIMEOUT_MS=45000
set REPIU_SINGLE_STEP_HOTSPOT_PROFILE=1
set REPIU_SINGLE_STEP_HOTSPOT_DUMP=1
build\Release\repiu_loader_win32.exe pumpit3 > repiu_log.txt 2>&1
```

Setting `REPIU_EXECUTION_TIMEOUT_MS` (45000, say) makes the run end by itself, so closing
the window is optional. The dump is written immediately after the guest thread stops and
**before** Glide close and worker shutdown, so the census survives a slow or hung teardown
(Task 401). Killing the process from Task Manager still skips it.

`REPIU_SINGLE_STEP_HOTSPOT_DUMP=1` writes `build/single_step_hotspot.txt`; any other value
is used as the path; empty disables the dump.

## 3. Check the result

In the log:

```
Win32 single-step hotspot enabled/total/distinct/overflow: true/<samples>/<addresses>/0
Win32 single-step hotspot dump written/entries/path: true/<entries>/build\single_step_hotspot.txt
```

A non-zero `overflow` means the 8,192-entry table filled and the census is incomplete.

The dump lists one address per line, ordered by sample count descending.

## 4. Reading it

| Observation | Interpretation |
|---|---|
| `distinct` is a few dozen, all inside one function | The guest is confined there; find the outer loop's condition |
| `distinct` is hundreds or thousands, spread beyond the hot loop | The guest is running broadly; the stall is a wait on a condition, not stopped execution |
| The next-ranked addresses cluster in one function | That function is the wait-loop candidate; disassemble those addresses in the original executable |

To convert a guest address to a file offset, use the object delta confirmed from the log's
`Relocated exception byte base` and byte window. For pumpit3 object 2 the delta is
`0x02FF4E00` (guest = file + delta).

## 4b. To see cache execution too — the guest position census (Task 411)

The census above samples only at single-step boundaries, so **a wait loop running in the
AOT cache without faulting is invisible to it**. For that case use the time-based census:
set `REPIU_GUEST_POSITION_CENSUS=1`, optionally `REPIU_GUEST_POSITION_CENSUS_MS` (default
10) and `REPIU_GUEST_POSITION_CENSUS_DUMP`, and run through `cmd /c` as above.
`scripts/task411_guest_position_census.ps1` repeats the runs, isolates the EEPROM per run,
and classifies each run stalled or healthy.

Read the three log lines `Win32 guest position census …`, `Win32 guest position origin …`,
and `Win32 guest position top #N …`. **Check the two gates first**: if
`sum-matches-total` is not `true`, or `overflow` is not zero, the table is not read as a
distribution. Top addresses clustered in one function name the wait loop, and
`repiu_aot_probe --dump` at `address - 0x02000000` settles its exit condition; a top made
only of the delay loop `0x0301DB1F`-`0x0301DB2A` means the sampler is seeing the timer ISR
and the interval must be changed to something coprime with the tick; a dominant `host`
share means the wait is on our side. When the host share is large, read Task 412's three
extra lines next — thread time, host scan, and host sites. A `cpu-share` at or above 90%
means **busy** in kernel exception dispatch and moves the axis to exception price and
count; at or below 50% it means **blocked**, and the host sites name where. A false
`parts-match` means the scan accounting broke and the distribution is not read at all; a
large `no-site` share means those samples' `ESP` was not the loader stack (an exception
taken on the guest stack), which is itself evidence; and scattered host sites mean the
shallow scan has reached its limit, so the next step is a real stack walk or per-site
scopes rather than a conclusion. **Runs with this census enabled are not quotable for
wall time or frames.** Do not confuse it with `REPIU_NATIVE_SAMPLING`'s `[repiu-sample]`
lines, which require a full second with no exception dispatch and therefore never appear in
a stalled run.

## 5. Limits (always apply when interpreting)

The census samples **only at single-step boundaries**, so code running inside the AOT cache
without trapping is under-represented. Therefore:

- If an address **appears**, that code definitely ran.
- If an address **does not appear**, you cannot conclude it did not run.

**Never use the census `total_cycles` as a cost claim.** It measures only the single-step
handler scope, which is a small fraction of wall clock (2.04% in the Task 402 measurement).
"95% of the census" is not "95% of the cost". For cost, use
`REPIU_EXECUTION_TIME_PROFILE` and compare buckets against
`Win32 execution time cycles ... guest-run`. Task 401 missed this distinction and named the
wrong target; Task 402 corrected it.

Check the `single_step` versus `heartbeat` ratio in the log first to know what fraction of
boundaries the census covers.
