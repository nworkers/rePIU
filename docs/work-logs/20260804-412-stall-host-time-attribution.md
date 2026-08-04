# Task 412 작업 로그 — 멈춤의 host 시간 귀속

설계: [20260804-412](../design/20260804-412-stall-host-time-attribution.md) ·
작업 지시: [20260804-412](../work-orders/20260804-412-stall-host-time-attribution.md)

## 1. 한 줄 결과

게스트 스레드는 **막혀 있지 않고 바쁩니다**(CPU 82.42%). host 시간의 정체는 단일
대기 지점이 아니라 **우리 VEH 경로의 작업이 흩어진 것**이며, Task 411이 이름 없이
남긴 62%는 이제 **함수 이름으로 분해**됩니다.

## 2. 구현

| 파일 | 변경 |
|---|---|
| `include/repiu/platform/win32/guest_position_census.h` | 스레드 CPU 시간, host 호출 지점 표(1,024), 스캔 검산 counter, 모듈·심볼 필드 |
| `src/platform/win32/telemetry/guest_position_census.cpp` | 위 구현 + `GetModuleHandleExA` 모듈 해석 + `dbghelp` 심볼화 |
| `src/platform/win32/native_phase_sampler.h/.cpp` | 정지 중 얕은 스택 훑기(SEH, C++ 객체 없는 함수). 모듈 범위 0이면 기존 동작 그대로 |
| `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | 로더 모듈 범위 전달, `GetThreadTimes` 기록, host 표본의 호출 지점 기록 |
| `src/host/win32/main.cpp` | CPU 시간·스캔 검산·모듈·호출 지점 로그 |
| `CMakeLists.txt` | Release에 `/Zi` + `/DEBUG`(코드 생성 불변), 로더에 `dbghelp` 링크 |

**빌드 낭비를 막은 방법:** 편집한 번역 단위를 먼저 `cl /Zs`로 문법 검사했습니다. 이
저장소의 전체 Release 빌드는 40분이 넘으므로, 오타 한 번의 비용이 그만큼입니다.

## 3. 측정 (pumpit3 60초, census 10 ms, 실행시간 프로파일 동시)

검산이 먼저입니다 — `sited + no-site + failed == host 표본`이 **성립**했고
(1,835 + 850 + 0 = 2,685), `overflow`와 `capture-failures`는 **0**입니다.

**확인됨 1 — 바쁨입니다.** kernel 25,641 ms + user 23,797 ms / wall 59,984 ms =
**CPU 82.42%**. 커널과 사용자 시간이 거의 같습니다. **대기 가설은 폐기**합니다.

**확인됨 2 — host 시간은 우리 코드의 여러 경로입니다.** sited 1,835건의 상위:

| # | 심볼 | 비중 |
|---:|---|---:|
| 1 | `WriteGuestBytes+0x6D` | 13.62% |
| 2 | `FindAotCacheAddress+0x95` | 12.70% |
| 3 | `ReResolveWin32AotSegmentOverrides+0x5B6` | 7.25% |
| 4 | `RequestAotInlineCachePatch+0x75` | 6.92% |
| 5 | `ReResolveWin32AotSegmentOverrides+0x7D` | 5.61% |
| 6~ | `RefreshJammaSnapshot`(+0x17E/+0x130/+0xC3/+0x84/+0xDF) | 합계 약 **10.4%** |
| 8 | `ReResolveAotSegmentOverrides+0x135` | 2.23% |

세그먼트 override 재해석은 세 항목 합계 **약 15.1%로 최대 인구**이고, JAMMA 스냅샷
갱신이 약 10.4%입니다. **`no-site` 850건(31.7%)** 은 그 표본의 `ESP`가 로더 스택이
아니었다는 뜻으로, 게스트 스택 위에서 예외가 잡힌 경우입니다.

**부수 확인 — 멈춤에는 두 모드가 있습니다.** 이 실행은 격리 모드였습니다
(`publishes/quarantines` 69/1, single-step 예외 **523,362**, tick 주입 9,239/10,693 =
86%). Task 411이 측정한 실행은 비격리 모드였습니다(single-step 300, 주입 2,275/9,951 =
23%). **두 모드 모두 6개 파일에서 멈추고 프레임이 0~1입니다.** 따라서 지배 비용의
종류는 모드마다 다르고(격리는 single-step gap 44%, 비격리는 breakpoint gap 62%),
**멈춤 자체는 둘 중 어느 비용에도 단독으로 귀속되지 않습니다.**

## 4. 미확정

* **왜 게스트 주 흐름이 진행하지 않는가.** 스레드가 82% 바쁘고 그 대부분이 타이머
  ISR 뒷일이라는 것까지는 확인됐지만, 주 흐름이 남은 시간에도 전진하지 못하는 기전은
  아직 이름이 없습니다. 후보는 **tick 전달률 붕괴로 게스트 시간이 25배 느려지는 것**
  (비격리 모드에서 due 9,951 대 injected 2,275)이며 미측정입니다.
* **`no-site` 31.7%의 정체.** 얕은 훑기의 구조적 한계이므로, 더 좁히려면 정식 스택
  워크가 필요합니다.
* 세그먼트 override 재해석이 왜 그렇게 자주 도는지(호출 빈도 미측정).

## 5. 회고

* **`GetThreadTimes` 한 줄이 하루를 아꼈습니다.** "막힘"이었다면 조사 방향이 완전히
  달랐을 텐데, 82%라는 숫자가 그 갈래를 즉시 닫았습니다.
* `/Zi`를 켜 둔 것이 이 과제의 실질적 산출입니다. 이제 **어떤 host 표본이든 함수
  이름으로 읽힙니다.**
* Task 411의 census만으로는 "ntdll 85%"에서 멈췄을 것입니다. **한 점(EIP)이 아니라
  들어간 경로(호출 지점)를 기록해야 이름이 붙습니다.**

---

# Task 412 Work Log — attributing the stall's host time

## 1. Result in one line

The guest thread is **busy, not blocked** (82.42% CPU), and its host time is not one wait
but **our own VEH-path work spread across several functions** — the 62% Task 411 left
unnamed now decomposes into symbols.

## 2. Implementation

The census gained thread CPU time, a 1,024-entry host call-site table with reconciliation
counters, module resolution through `GetModuleHandleExA`, and `dbghelp` symbolisation; the
native phase sampler gained a suspended-thread shallow stack scan in an SEH function with
no C++ objects, inert when no module range is passed; the poll loop passes the loader's
image range, records `GetThreadTimes`, and records a call site per host sample; `main.cpp`
prints all of it; and Release gained `/Zi` with `/DEBUG` for the PDB while keeping every
optimisation flag. **Every edited translation unit was syntax-checked with `cl /Zs` first**,
because a full Release build here costs over forty minutes.

## 3. Measurement (pumpit3, 60 s, census at 10 ms, execution-time profile on)

The gate held first: `sited + no-site + failed` equals the host sample count
(1,835 + 850 + 0 = 2,685), with zero overflow and zero capture failures.

**Confirmed 1 — busy.** Kernel 25,641 ms plus user 23,797 ms against 59,984 ms of wall is
**82.42% CPU**, with kernel and user time nearly equal. **The blocked hypothesis is
retired.**

**Confirmed 2 — the host time is our own code, in several places.** Of the 1,835 sited
samples: `WriteGuestBytes+0x6D` 13.62%, `FindAotCacheAddress+0x95` 12.70%,
`ReResolveWin32AotSegmentOverrides+0x5B6` 7.25%, `RequestAotInlineCachePatch+0x75` 6.92%,
`ReResolveWin32AotSegmentOverrides+0x7D` 5.61%, five `RefreshJammaSnapshot` offsets summing
to about 10.4%, and `ReResolveAotSegmentOverrides+0x135` 2.23%. Segment-override
re-resolution is the **largest population at about 15.1%** across its three entries, with
the JAMMA snapshot refresh next at about 10.4%. The **850 `no-site` samples (31.7%)** are
those whose `ESP` was not the loader stack — exceptions taken on the guest stack.

**Also — the stall has two modes.** This run was the quarantined one (69 publishes with one
quarantine, **523,362** single-step exceptions, 9,239 of 10,693 ticks injected = 86%), while
Task 411 measured the unquarantined mode (300 single steps, 2,275 of 9,951 = 23%). **Both
stop at six files with zero or one frame**, so the dominant cost differs by mode —
single-step gap at 44% under quarantine against breakpoint gap at 62% without it — and **the
stall itself attributes to neither cost alone.**

## 4. Unresolved

Why the main flow makes no progress at all: the thread being 82% busy on timer-ISR
aftermath is confirmed, but the mechanism that keeps the main flow from advancing in the
remaining time is unnamed — the leading candidate, unmeasured, is that tick delivery
collapse slows guest time about 25-fold (9,951 due against 2,275 injected in the
unquarantined mode). What the 31.7% `no-site` share hides, which needs a real stack walk.
And how often segment-override re-resolution actually runs.

## 5. Retrospective

One `GetThreadTimes` call saved a day: had it read "blocked", the whole investigation would
have gone elsewhere, and 82% closed that branch immediately. Leaving `/Zi` on is the
durable deliverable — **any host sample now reads as a function name.** And Task 411's
census alone would have stopped at "85% ntdll": naming requires recording the path *into*
the code, not just the point.
