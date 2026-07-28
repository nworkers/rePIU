# 20260728-331 작업 로그: 성능 기준의 Release 이전과 append 재귀속 / Work log

설계: [20260728-331-release-baseline-migration.md](../design/20260728-331-release-baseline-migration.md)

작업 지시: [20260728-331-release-baseline-migration.md](../work-orders/20260728-331-release-baseline-migration.md)

## 한국어

### 결론 요약

**gate G4가 성립했습니다. Release에서는 append의 어느 단계도 50%에 이르지 못합니다.**
그리고 그보다 중요한 결과가 나왔습니다. **Release로 옮기는 것만으로 append 자체가
약 11.2배 싸집니다.** 실게임 평균 크기(명령 1,039개)로 환산하면
`65,371,802 → 5,849,960 tick`입니다.

즉 Tasks 322~329가 추적해 온 **동적 번역 사슬은 Release에서 더 이상 지배 병목이
아닐 가능성이 큽니다.** 다음 최적화 대상은 append 내부가 아니라 **Release 기준
전체 실행 재귀속**입니다.

### 사전 등록 gate 판정

| gate | 조건 | 관측(Release small) | 판정 |
|---|---|---:|---|
| G1 | `placement` >= 50% | 43.37% | 기각 |
| G2 | `plan_build` >= 50% | 34.29% | 기각 |
| G3 | `image_emit` >= 50% | 21.53% | 기각 |
| **G4** | **어느 단계도 50% 미만** | **최대 43.37%** | **성립** |
| G5 | `placement` 고정 절편 >= placement의 70% | 56.83% | 기각 |

### 측정 값

probe는 arena를 예약해 이미지를 그 주소로 재배치한 뒤 실제
`AppendWin32DynamicAotTranslation`을 구동하고, 제품 코드의
`Win32AotWorkerTimingProfile` 단계를 그대로 읽습니다. 두 구성 모두 같은 입력에서
small 286 명령, large 47,750 명령으로 동일한 크기를 다뤘습니다.

#### 한 번의 append (최소 표본, tick)

| 단계 | Debug small | Release small | Debug large | Release large |
|---|---:|---:|---:|---:|
| `plan_build` | 7,036,234 | 597,677 | 1,282,774,768 | 117,000,063 |
| `placement` | 6,469,968 | 755,823 | 1,108,071,793 | 54,937,933 |
| `image_emit` | 2,978,460 | 375,287 | 522,927,599 | 65,971,456 |
| `validate` | 132,729 | 12,874 | 89,265,690 | 11,521,052 |
| `arena_snapshot` | 4,019 | 61 | 3,424 | 391 |
| **합계** | **16,646,952** | **1,742,618** | **3,003,067,995** | **249,434,339** |

#### 단계 순위가 구성과 크기 양쪽에 따라 바뀝니다

| 단계 | Debug small | Release small | Debug large | Release large |
|---|---:|---:|---:|---:|
| `plan_build` | **42.26%** | 34.29% | **42.71%** | **46.91%** |
| `placement` | 38.86% | **43.37%** | 36.90% | 22.02% |
| `image_emit` | 17.89% | 21.53% | 17.41% | 26.45% |
| `validate` | 0.79% | 0.73% | 2.97% | 4.62% |

#### 명령당 비용과 append당 고정 비용 (1차 적합)

| 단계 | Debug 명령당 | Release 명령당 | Debug 계수 | Release 고정 |
|---|---:|---:|---:|---:|
| `plan_build` | 26,878 | 2,452 | 10.96배 | 0 |
| `image_emit` | 10,954 | 1,382 | 7.93배 | 0 |
| `placement` | 23,209 | 1,141 | **20.34배** | **429,497** |
| `validate` | 1,877 | 242 | 7.76배 | 0 |

**확인됨:** Task 330이 `plan_build`에서 관찰한 "Debug 계수가 단계마다 다르다"가
append 전체로 확장됩니다. 계수는 `image_emit` 7.93배에서 `placement` 명령당
20.34배까지 벌어집니다.

**확인됨:** `placement`에는 **번역량과 무관한 고정 비용 약 `429,497 tick`
(2.5GHz 기준 약 172us)** 이 있습니다. 원인은 append마다 캐시 **전체 16MB**에 대해
`VirtualProtect`를 두 번 호출하고 `FlushInstructionCache`를 부르는 구조입니다.
이 비용은 syscall이므로 구성과 무관하며, Debug에서는 small placement의 6.6%에
불과해 **보이지 않다가** Release에서 56.83%가 됩니다. Debug만 봤다면 놓쳤을
항목입니다.

#### 실게임 평균 크기(1,039 명령)로 환산

| 단계 | Debug 환산 | Release 환산 | Release 비중 |
|---|---:|---:|---:|
| `plan_build` | 27,926,242 | 2,547,628 | **43.55%** |
| `placement` | 24,114,151 | 1,614,996 | 27.61% |
| `image_emit` | 11,381,206 | 1,435,898 | 24.55% |
| `validate` | 1,950,203 | 251,438 | 4.30% |
| **합계** | **65,371,802** | **5,849,960** | — |

**대표성 확인:** Debug 환산 `65,371,802`은 Task 329가 실게임 60초에서 측정한 회당
`67,367,429`과 **3.0% 차이**입니다. Task 330의 3.6%에 이어 이 probe도 in-situ
비용을 잘 대표합니다.

**따라서 Release/Debug 비는 실게임 크기에서 11.2배입니다.**

### 이것이 다음 대상을 바꿉니다

Release에서 append 1회는 약 `5,849,960 tick`, 2.5GHz 기준 약 **2.34ms**입니다.
Task 326이 측정한 번역 빈도는 60초에 230회(초당 3.8회)였습니다. 그 빈도라면 Release
번역 총비용은 60초 중 약 0.54초, **약 0.9%** 입니다. 빈도가 10배로 늘어도 약 8.9%
입니다.

**따라서 Tasks 322~329가 추적한 동적 번역 사슬은 Release에서 지배 병목이 아닐
가능성이 큽니다(추정 — 번역 빈도는 Release에서 재측정하지 않았습니다).** 어느 한
단계를 고쳐도 append 상한은 약 1.8배이고, append 자체의 전체 기여가 위 추정대로면
상한은 그보다 훨씬 작습니다.

### 실게임 60초 A/B (`pumpit1`, `aot-dbt`, `REPIU_EXECUTION_TIME_PROFILE=1`)

**미확정으로 남겼던 항목을 직접 실행해 해소했습니다.** 두 구성 모두 60초 timeout에
정상 도달했고 `malformed 0`, `original fatal halt reached: false`,
`Glide implementation issues 0/0/0/0/0/0`으로 동등했습니다. (EEPROM SHA-256은 이
구간에서 출력되지 않아 비교 항목에서 제외했습니다.)

| 항목 | Debug | Release | 비 |
|---|---:|---:|---:|
| progress | 50,826 | 64,347 | 1.27배 |
| heartbeat | 646,008 | 814,287 | 1.26배 |
| `grBufferSwap` (프레임) | 134 | 275 | **2.05배** |
| 번역 횟수 | 239 | 240 | 1.00배 |
| 번역 1회 append | 71,054,606 | 6,811,483 | **1/10.4** |

**확인됨: probe의 예측이 실측과 일치합니다.** probe는 append가 Release에서 11.2배
싸진다고 예측했고 실측은 10.4배였습니다. append 단계 분포도 다음과 같이 맞습니다.

| 단계 | Debug 실측 | Debug probe 예측 | Release 실측 | Release probe 예측 |
|---|---:|---:|---:|---:|
| `plan_build` | 43.31% | 42.71% | 40.88% | 43.55% |
| `placement` | 36.63% | 36.90% | 33.03% | 27.61% |
| `image_emit` | 18.64% | 17.41% | 24.15% | 24.55% |
| `validate` | 1.34% | 2.98% | 1.83% | 4.30% |

**확인됨: Release 전체 실행 축이 완전히 다른 그림입니다.** 중첩은 해석 문제가
아니라 **포함 관계**였습니다. `guest-run = veh + AOT 캐시 실행`이고
`veh = glide-gate + port-io + dos + veh-exclusive`입니다.

| bucket | Debug | Release |
|---|---:|---:|
| **Glide gate (VEH 내부)** | 26.86% | **60.78%** |
| VEH-exclusive (AOT transfer 등) | 53.23% | 20.43% |
| AOT 캐시 내 guest 실행 | 19.42% | 18.03% |
| DOS service | 0.18% | 0.62% |
| port I/O | 0.31% | 0.14% |
| (참고) 동적 번역 | 10.48% | **1.04%** |

**확인됨: 동적 번역은 Release에서 전체의 1.04%입니다.** 이 작업이 추정으로 낸
"약 0.9%"가 실측으로 확인됐고, **Tasks 322~329가 추적한 사슬은 종결됐습니다.**
지금 그 경로를 완전히 없애도 상한은 약 1.01배입니다.

**확인됨: 다음 병목은 Glide gate입니다.** Release에서 gate 진입 21,381회가
98,941,888,040 tick을 소비하며 **호출당 약 4.63M tick(2.5GHz 기준 약 1.85ms)** 입니다.
60초에 프레임은 275개뿐이므로 프레임당 gate 진입은 약 78회입니다.

**미확정:** 그 1.85ms가 host CPU 작업(OpenGL/텍스처 변환)인지 rendezvous 대기인지
구분하지 않았습니다. Release의 호출당 비용이 Debug(약 2.97M tick)보다 **오히려
크다**는 점은 CPU 작업이 아니라 대기일 가능성을 시사하지만, Task 327이 번역
rendezvous에서 했던 것과 같은 계측이 있어야 확정할 수 있습니다. 이것이 다음 Task의
질문입니다.

### 부수 관찰 — 재배치 base에 따라 정적 emit이 실패합니다

probe를 만들며 확인한 사실입니다. arena base를 `0x05920000`으로 잡으면 전체 이미지
plan과 entry plan 모두 `BuildAotCodeCacheImage`가
`direct control-flow target is outside the cache`로 **실패**합니다. `0x01000000`
에서는 성공합니다. 즉 **emit 성공 여부가 재배치 base에 의존합니다.**

로더는 `SelectAndReserveRelocatedImageBase`로 base를 동적으로 고르므로, 특정 base를
잡은 실행에서는 정적 AOT 캐시 구성이 실패할 수 있습니다. **미확정:** 실제로 그런
base가 로더 후보에 있는지, 실패 시 어떤 경로로 떨어지는지는 확인하지 않았습니다.
probe는 base 후보를 순회해 entry 번역이 emit되는 base를 고르는 방식으로 회피했습니다.

### 검증 결과

1. `scripts/build_win32_x86.ps1` (Debug) 전체 빌드 통과.
2. `scripts/build_win32_x86_release.ps1` (Release) 전체 빌드 통과.
3. `repiu_aot_probe`가 **두 구성 모두 exit 0**, 신규 `append_bench_*` 포함 전 그룹
   통과. `append_bench_snapshot_removed=true`로 Task 329의 스냅샷 제거가 유지됨을,
   `append_bench_partitioned=true`로 다섯 단계가 호출을 분할함을(residual은 Release
   small에서 896 tick, 0.05%) 확인했습니다.
4. `append_bench_clamped_samples=0` — 역행 TSC 표본 없음.
5. Release 3회 실행의 편차는 총합 1.3%, `placement` 고정 절편
   `429,497 / 442,518 / 475,433`(약 ±5%)이었습니다. 단계 비중 순위는 세 번 모두
   동일했습니다.
6. `scripts/test_all.ps1 -SkipSetup`은 빌드와 `dos4gw_hello`까지 통과하고 `piu_1st`
   단계에서 실패합니다. 원인은 이 환경에 `MASTER/PIU_1ST` 자산이 없어
   `Failed to read MASTER/PIU_1ST/PIU/PIU.EXE: failed to open file`로 로더가 종료되기
   때문이며, **이번 변경과 무관한 환경 조건**입니다. 이번 변경은 스크립트 인자와
   probe 추가뿐이고 로더 코드는 건드리지 않았습니다.

### 확인됨 / Confirmed

* Release 전체 빌드가 통과하고 두 구성의 probe suite가 모두 통과합니다.
* Release 로더는 60초 `pumpit1` 실행을 malformed 0, fatal 0, Glide 공백 0으로
  완료하며 Debug 대비 progress 1.27배, 프레임 2.05배입니다.
* append 단계 순위는 **구성과 번역 크기 양쪽**에 따라 바뀝니다.
* `placement`의 append당 고정 비용은 약 `429,497 tick`이며 구성과 무관합니다.
* Release append는 실측으로 Debug의 **1/10.4**이며 probe 예측(1/11.2)과 일치합니다.
* **동적 번역은 Release 전체의 1.04%입니다.** 해당 최적화 사슬은 종결됐습니다.
* **Release의 지배 병목은 Glide gate 60.78%** 이며 호출당 약 1.85ms입니다.

### 미확정 / Unresolved

* Glide gate의 1.85ms가 **host CPU 작업인지 rendezvous 대기인지** 구분하지
  않았습니다. Release 호출당 비용이 Debug보다 큰 점은 대기 쪽을 시사합니다.
* VEH-exclusive 20.43% 안에서 AOT transfer는 15.60%이고, 그 handler 축은 중첩
  때문에 합이 100%를 넘습니다(reentry 94.44%, return 33.82%). 재분해가 필요합니다.
* EEPROM SHA-256은 이 구간 로그에 출력되지 않아 동등성 비교 항목에서 뺐습니다.
* small 표본은 window를 넓혀도 명령 286개에서 포화합니다(entry 국소 CFG가 작음).
  1,039 명령 수치는 두 점 적합으로 유도한 값입니다.
* 재배치 base에 따른 emit 실패의 실제 영향 범위.
* 실게임 A/B는 구성당 1회 표본입니다.

---

## English

### Summary

Gate G4 holds: in Release no append phase reaches 50%, the largest being `placement` at 43.37% of
a 286-instruction append. The more consequential result is that moving to Release makes the append
itself about 11.2x cheaper — `65,371,802` to `5,849,960` ticks at the 1,039-instruction in-game
mean — so the dynamic-translation chain Tasks 322 through 329 pursued is probably no longer the
dominant bottleneck, and the next target is a Release whole-run attribution rather than anything
inside the append.

### Gate results

G1 is rejected with `placement` at 43.37%, G2 with `plan_build` at 34.29%, G3 with `image_emit` at
21.53%, and G5 with the `placement` intercept at 56.83% of its own phase. G4 holds.

### Measurements

The probe reserves an arena, relocates the image to that address, drives real appends, and reads
the product's own `Win32AotWorkerTimingProfile` phases. Both configurations translated identical
sizes: 286 instructions for the small sample and 47,750 for the large one.

One append cost `16,646,952` ticks in Debug against `1,742,618` in Release at the small size, and
`3,003,067,995` against `249,434,339` at the large size. The phase ranking moves with both the
configuration and the size: `plan_build` leads in Debug at both sizes (42.26% and 42.71%) and in
Release only at the large size (46.91%), while `placement` leads the Release small append at
43.37% and falls to 22.02% at the large one.

The per-instruction fit gives `plan_build` 26,878 ticks in Debug against 2,452 in Release,
`image_emit` 10,954 against 1,382, `placement` 23,209 against 1,141, and `validate` 1,877 against
242. The Debug factor therefore ranges from 7.93x to 20.34x across phases, extending Task 330's
finding from plan building to the whole append.

`placement` carries a fixed cost of about `429,497` ticks per append, roughly 172us at 2.5GHz,
which comes from calling `VirtualProtect` twice over the entire 16MB cache capacity plus
`FlushInstructionCache`. Being syscall cost it is configuration-independent, and it is invisible in
Debug at 6.6% of that configuration's small placement while being 56.83% of Release's — an item
Debug-only measurement would have missed.

Converted to the 1,039-instruction in-game mean, Release spends 43.55% in `plan_build`, 27.61% in
`placement`, 24.55% in `image_emit`, and 4.30% in `validate`, totalling `5,849,960` ticks against
Debug's `65,371,802`. The Debug figure sits 3.0% from the `67,367,429` Task 329 measured live, so
this probe represents in-situ cost as well as Task 330's did at 3.6%.

### Why this changes the target

One Release append costs about 2.34ms at 2.5GHz. At the 230 translations per 60 seconds Task 326
measured, that is about 0.54 seconds, or 0.9% of wall clock; even a tenfold higher frequency gives
8.9%. The dynamic-translation chain is therefore probably not the dominant cost in Release —
inferred, since the translation frequency was not re-measured there — and fixing any single phase
bounds the append at about 1.8x.

### The 60-second in-game A/B

Both configurations reached the 60-second timeout normally on `pumpit1` with `aot-dbt`, with zero
malformed dispatch, `original fatal halt reached: false`, and no Glide implementation gap, so the
configurations are equivalent on every checked axis. (The EEPROM SHA-256 is not printed in this
phase and was dropped from the comparison.) Release reached progress 64,347 against Debug's 50,826
(1.27x), heartbeat 814,287 against 646,008 (1.26x), and 275 `grBufferSwap` frames against 134
(2.05x), with both runs performing essentially the same number of translations (240 and 239).

The probe's predictions hold in situ. Per-translation append cost measured 71,054,606 ticks in
Debug against 6,811,483 in Release, a factor of 10.4 against the probe's predicted 11.2, and the
phase shares match: Debug 43.31/36.63/18.64/1.34 for plan build, placement, image emit, and
validate against the predicted 42.71/36.90/17.41/2.98, and Release 40.88/33.03/24.15/1.83 against
43.55/27.61/24.55/4.30.

The whole-run picture is different in Release, and the bucket overlap turns out to be containment
rather than an interpretation problem: `guest-run` is the VEH plus AOT cache execution, and the VEH
is the Glide gate plus port I/O plus DOS plus what remains. The Glide gate holds 60.78% of guest
wall clock in Release against 26.86% in Debug, the VEH-exclusive remainder 20.43% against 53.23%,
AOT cache execution 18.03% against 19.42%, DOS 0.62%, and port I/O 0.14%. Dynamic translation is
1.04% of Release wall clock against 10.48% in Debug, confirming this task's 0.9% estimate and
closing the chain Tasks 322 through 329 pursued: removing that path entirely would now bound
improvement at about 1.01x.

The next bottleneck is the Glide gate, whose 21,381 entries consume 98,941,888,040 ticks, about
4.63M ticks or 1.85ms each, against only 275 frames in 60 seconds, roughly 78 gate entries per
frame. Unresolved: whether that 1.85ms is host CPU work such as OpenGL and texture conversion or
rendezvous waiting. Release costs more per call than Debug's 2.97M ticks, which points at waiting
rather than CPU work, but settling it needs the instrumentation Task 327 applied to the translation
rendezvous. That is the next task's question.

### Incidental observation

Whether the static image emits depends on the relocation base: at `0x05920000` both the whole-image
and the entry plan fail with `direct control-flow target is outside the cache`, while `0x01000000`
succeeds. The loader chooses its base dynamically, so a run that lands on such a base could fail to
build its static AOT cache. Unresolved: whether any such base is reachable from the loader's
candidates and what it falls back to. The probe works around it by trying bases until the entry
translation emits.

### Verification

Full Debug and full Release builds passed, and `repiu_aot_probe` exited 0 in both configurations
with every group passing, including `append_bench_snapshot_removed=true`, which re-confirms Task
329's removal, and `append_bench_partitioned=true` with a residual of 896 ticks (0.05%) on the
Release small append. No backwards TSC sample was recorded, and three Release runs differed by
1.3% in total with placement intercepts of `429,497`, `442,518`, and `475,433`, about 5%, while the
phase ranking was identical each time. `scripts/test_all.ps1 -SkipSetup` passes the
build and `dos4gw_hello` steps and fails at `piu_1st` because this environment has no
`MASTER/PIU_1ST` asset tree, which is unrelated to this change: only scripts and a probe were
added, and no loader code was touched.

### Unresolved

The 60-second in-game Release A/B has not been run and needs the user, with
`scripts/build_win32_x86_release.bat` as its entry point; the Release translation frequency was not
re-measured, so the 0.9% estimate reuses Task 326's Debug frequency; the small sample saturates at
286 instructions no matter how wide the window, because the entry's local graph is small, so the
1,039-instruction figures are derived from a two-point fit rather than measured; the whole-run
bucket overlap remains; and the base-dependent emit failure's real reach is unknown.
