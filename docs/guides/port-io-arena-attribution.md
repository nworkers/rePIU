# Port I/O / arena 실행 귀속 측정 가이드 / Measuring the Port I/O and Arena-Execution Axis

pumpit3 wall clock의 약 절반이 port I/O 예외이며, 그 원인 추적에 쓰는 계측 묶음의
반복 절차입니다. 근거는 [pumpit3 bring-up](../analysis/pumpit3-bring-up.md)과
Tasks [405](../work-logs/20260803-405-port-io-address-census.md) ~
[409](../work-logs/20260803-409-arena-entry-predecessor-histogram.md) 작업 로그,
구조는 `ARCHITECTURE.md`의 "Port I/O 주소 census와 arena 진입 추적"에 있습니다.

## 1. 언제 쓰는가

* port I/O 예외 비중이 다시 커졌는지 확인할 때
* 어떤 게스트 주소가 예외를 내는지, 그 코드가 캐시인지 arena인지 가를 때
* arena 실행으로 빠지는 진입 기전을 추적할 때

## 2. 실행

Release 빌드를 쓰고 EEPROM을 **매 실행 격리**합니다. 격리하지 않으면 영속 상태가
실행 간에 새어 결과가 무효가 됩니다(Task 403에서 실제로 겪음).

```
set REPIU_EXECUTION_BACKEND=aot-dbt
set REPIU_EXECUTION_TIMEOUT_MS=45000
set REPIU_EXECUTION_TIME_PROFILE=1
set REPIU_EEPROM_PATH=<실행별 사본>
build\win32_x86_debug\Release\repiu_loader_win32.exe pumpit3 > run.txt 2>&1
```

매핑 존재 여부까지 보려면 다음을 추가합니다. **이 실행의 wall·프레임은 인용하지
않습니다**(호출당 `FindAotCacheAddress`가 붙어 약 5.8% 느려짐).

```
set REPIU_PORT_IO_CENSUS_MAPPING=1
```

## 3. 읽을 줄

```
Win32 port I/O address census entries/overflow/total: <항목>/<초과>/<합계>
Win32 port I/O address #N guest/count/cache/arena/mapped/reentry: ...
Win32 port I/O address #N entry count/prev-code/prev-eip/flags: ...
Win32 port I/O address #N entry prev step/bp/av/other: ...
Win32 arena port I/O entry trace total/shown: ...
Win32 arena port I/O entry #N guest/prev-code/prev-eip/prev-in-cache/tf/reentry/legacy/step: ...
```

`flags` 비트는 0 prev-in-cache, 1 TF, 2 reentry, 3 legacy, 4 single-step trace입니다.
예외 코드는 `0x80000003` breakpoint, `0x80000004` single-step, `0xC0000005` access
violation, `0xC0000096` privileged instruction입니다.

## 4. 판정

| 관측 | 해석 |
|---|---|
| `cache`가 0 | 그 코드는 AOT 캐시가 아니라 arena에서 실행 중 |
| `mapped`가 `count`에 가까움 | 번역은 있는데 복귀하지 않음 |
| `reentry`가 0 | 복귀가 **예약조차** 안 됨 (자유 실행) |
| `reentry`가 count의 대부분 | 복귀를 시도하다 격리에 막힘 |
| 진입 = count | 매 실행이 캐시↔arena 왕복 |
| 진입 ≪ count | 한 번 들어가면 오래 머묾 |

## 5. 반드시 함께 확인할 것

* `Win32 AOT generation publishes/quarantines: .../N` — **N이 0이 아니면 격리 실행**
  입니다. 격리·정상 두 모드는 재진입 거동이 정반대이므로 **섞어서 평균 내지 않습니다.**
* `exception census single-step/breakpoint/access-violation/other/total` — 진입
  히스토그램의 분류 합이 이 총수를 넘을 수 없습니다. **분류 수가 해당 예외 총수보다
  크면 해석이 틀린 것입니다**(Task 408이 이 검산을 빠뜨려 결론을 과하게 냈습니다).
* profiled `kPortIoDevice` count는 과대 계상이므로 **횟수는 census를 씁니다.**

## 6. 한계

* census 용량은 32항목입니다. pumpit1은 이를 넘겨 `overflow`가 발생하므로 pumpit1
  분석에는 용량을 늘려야 합니다. pumpit3는 29~30항목으로 충분합니다.
* 진입 표본은 주소당 첫 1건만 보관합니다. 구체 상태는 그 1건에 대해서만 성립하며,
  모집단은 히스토그램으로 읽어야 합니다.
* 실행 간 편차가 큽니다. `0x0301DB22`의 진입 횟수는 1회부터 3,124회까지 관측됐으므로
  **단일 실행으로 판정하지 않습니다.**

---

# Measuring the Port I/O and Arena-Execution Axis

About half of pumpit3's wall clock is port I/O exceptions; this is the repeatable procedure for
the instrumentation cluster used to attribute them. Evidence is in
[pumpit3 bring-up](../analysis/pumpit3-bring-up.md) and the Tasks 405-409 work logs; the
structure is in `ARCHITECTURE.md` under "Port I/O address census and arena entry tracing".

## When to use it

To check whether the port I/O share has grown again, to tell which guest addresses fault and
whether that code runs from the cache or the arena, and to trace how execution falls into
arena mode.

## Running

Use the Release build and **isolate the EEPROM per run** — without that, persistent state leaks
between runs and invalidates the result, as happened in Task 403. Set
`REPIU_EXECUTION_BACKEND=aot-dbt`, `REPIU_EXECUTION_TIMEOUT_MS=45000`,
`REPIU_EXECUTION_TIME_PROFILE=1`, and a per-run `REPIU_EEPROM_PATH`. Add
`REPIU_PORT_IO_CENSUS_MAPPING=1` to see whether a translation exists, but **do not quote that
run's wall time or frame count** — the per-call `FindAotCacheAddress` costs about 5.8%.

## Lines to read

The census total, then per address `guest/count/cache/arena/mapped/reentry`, its
`entry count/prev-code/prev-eip/flags`, and its `entry prev step/bp/av/other`; plus the global
`arena port I/O entry trace`. Flag bits are prev-in-cache, trap flag, re-entry pending, legacy
fallback, and single-step trace, from bit 0. Exception codes are `0x80000003` breakpoint,
`0x80000004` single step, `0xC0000005` access violation, and `0xC0000096` privileged
instruction.

## Interpreting

`cache` at zero means the code runs in the arena, not the cache. `mapped` near `count` means a
translation exists but is never returned to. `reentry` at zero means the return is not even
scheduled, while `reentry` near `count` means returns are attempted and refused by a quarantine.
Entries equal to `count` mean every execution crosses between cache and arena; entries far below
`count` mean long arena residencies.

## Always cross-check

Read `AOT generation publishes/quarantines` first — **a nonzero quarantine count marks a
quarantined run**, and the two modes behave oppositely on re-entry, so they must never be
averaged together. Check the entry histogram against
`exception census single-step/breakpoint/access-violation/other/total`: a class count cannot
exceed that exception's total, and **if it does, the reading is wrong** — Task 408 skipped this
check and overstated its conclusion. And use the census, not the profiled `kPortIoDevice`
count, for how many port I/O operations occurred.

## Limits

The census holds 32 addresses; pumpit1 overflows it and would need a larger table, while
pumpit3 fits in 29-30. The entry sample keeps only the first transition per address, so
concrete state applies to that one alone and the population must be read from the histogram.
Run-to-run variation is large — entries at `0x0301DB22` ranged from 1 to 3,124 — so **never
decide from a single run**.
