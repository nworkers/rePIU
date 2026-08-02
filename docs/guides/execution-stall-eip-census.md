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

## 5. 한계 (해석 시 반드시 고려)

이 census는 **single-step 경계에서만** 표본을 남깁니다. AOT cache 안에서 트랩 없이
실행되는 구간은 과소 대표됩니다. 따라서

- 어떤 주소가 census에 **있으면** 그 코드는 확실히 실행됐습니다.
- 어떤 주소가 census에 **없다고** 실행되지 않았다고 단정할 수는 없습니다.

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

## 5. Limits (always apply when interpreting)

The census samples **only at single-step boundaries**, so code running inside the AOT cache
without trapping is under-represented. Therefore:

- If an address **appears**, that code definitely ran.
- If an address **does not appear**, you cannot conclude it did not run.

Check the `single_step` versus `heartbeat` ratio in the log first to know what fraction of
boundaries the census covers.
