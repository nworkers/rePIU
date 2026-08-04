# Task 411 작업 로그 — 멈춘 실행의 게스트 위치 census

설계: [20260804-411](../design/20260804-411-stall-guest-position-census.md) ·
작업 지시: [20260804-411](../work-orders/20260804-411-stall-guest-position-census.md) ·
증상: [pumpit3 기동 중 멈춤](../analysis/pumpit3-startup-stall.md)

## 1. 한 줄 결과

**멈춤은 게스트의 대기 루프가 아닙니다.** 게스트 스레드는 표본의 **85%를 host/커널
(WOW64 ntdll32)** 에서 보내고, 나머지 15% 중 대부분이 **240 Hz 타이머 ISR의 200회 포트
지연 루프**입니다. 주 실행 흐름은 4배 긴 실행에서도 **전혀 진행하지 않습니다.**

## 2. 코드 변경 없이 끝난 부분 (정적)

이전 "다음 대상 1"(격리 페이지 진입 시 복귀 주소 기록)의 전제를 `repiu_aot_probe`의
`--xref`/`--dump`만으로 **반증**했습니다. 전문은 분석 문서 확인됨 7과
[EXE_DESIGN](../EXE_DESIGN.ko.md)의 타이머 ISR 절에 있습니다.

* 지연 루틴 `0x0301DB10`의 호출처는 **`0x03010BCF` 하나**(절대 주소 참조 0건).
* 그 호출자 `0x03010BA4`는 **타이머 슬롯 0의 콜백**이며, `stage.cfg` 파싱 직후
  `RegisterTimerSlot(0, …)`와 `240.0`(double) 설정으로 등록됩니다.
* 따라서 13,173회는 **대기 횟수가 아니라 tick 수**입니다(60초 기준 약 220 Hz,
  프로그램 값 240 Hz, 차이는 Task 366의 tick 손실 11.9%와 같은 크기).
* `stage.cfg` 파서 `0x03019910`은 EOF에서 `fclose` 후 1을 돌려주는 유한 함수입니다.

## 3. 구현

기존 계측은 **표본 시점이 전부 예외에 묶여** 있어(핫스팟 census는 single-step 구간,
native phase sampler는 예외 dispatch 1초 무음 필요) 캐시에서 예외 없이 도는 코드를
보지 못했습니다. 시간 기준 census를 넣었습니다.

| 파일 | 변경 |
|---|---|
| `include/repiu/platform/win32/guest_position_census.h` (신규) | census 자료구조·분류·스냅샷·덤프 API |
| `src/platform/win32/telemetry/guest_position_census.cpp` (신규) | 4,096-slot open addressing, origin 4종, 덤프 |
| `src/platform/win32/telemetry/live_telemetry_snapshot.cpp` | poll 루프에서 dispatch-quiet **없이** 간격 표본, 스냅샷·덤프 |
| `src/platform/win32/execution/thread_context.h`, `execution_trampoline.cpp` | census 소유·할당 |
| `include/repiu/platform/win32/execution_trampoline.h`, `src/host/win32/main.cpp` | 스냅샷 필드와 로그 출력(총계·origin·상위 16) |
| `scripts/task411_guest_position_census.ps1` (신규) | 반복 실행·판정·검산 자동화 |

캡처는 기존 `CaptureWin32NativePhaseSample`을 재사용하므로 suspend/resume 계약과
역매핑 안전성 근거가 그대로 유지됩니다. **Release 빌드 오류 0**(경고는 기존 C4819뿐).

## 4. 측정 — 이 세션에서 pumpit3는 **11회 중 11회** 멈췄습니다

| 묶음 | 설정 | 결과 |
|---|---|---|
| 대조 1회(45초) | census OFF | 멈춤(traces 6 / frames 1 / publishes 100) |
| 3회(60초) | census OFF | 전부 멈춤(traces 6 / frames 0~1 / publishes 84~100) |
| 6회(60초) | census ON 10 ms | 4회 멈춤 + 2회 "느림"(traces 7 / frames 1~2 / publishes 100~101) |
| 1회(240초) | census ON 10 ms | 멈춤(traces 6 / frames 1) |
| 1회(120초) | census ON + 실행시간 프로파일 | 멈춤(traces 6 / frames 1) |

**정상 실행은 0회입니다.** 분석 문서가 기록한 재현율 29%(17회 중 5회, v0.0.128)와
다릅니다. 같은 코드에서 재현율이 세션마다 달라진다는 것은 Task 404/405가 이미 관측한
비결정성과 같은 성격이며, **오늘은 그 극단**입니다.

검산은 모든 census 실행에서 통과했습니다 — `sum == total` **true**, `overflow` **0**,
`capture-failures` **0**.

## 5. 확인됨 — 게스트는 진행하지 않고, 게스트 코드도 거의 돌지 않습니다

**5a. 4배 긴 실행에서도 주 흐름의 표본이 늘지 않습니다.** 60초와 240초 census를 같은
축에서 비교하면 ISR 지연 루프만 4배가 되고 나머지는 그대로입니다.

| 게스트 주소 | 성격 | 60초 | 240초 |
|---|---|---:|---:|
| `0x0301DB24`/`0x0301DB22` | ISR 지연 루프(arena) | 102 / 72 | **402 / 289** |
| `0x03021F3A` | 비트스트림 리더(cache) | 32 | **32** |
| `0x0302203C` | 〃 | 18 | 22 |
| `0x030220CE` | 〃 | 16 | 9 |

DOS path trace도 6개로 같고 프레임도 1입니다. **240초는 60초보다 아무것도 더 하지
않았습니다.**

**5b. 시간의 85%는 host/커널입니다.** 120초 실행 기준 origin은
arena 137 / cache-mapped 292 / cache-unmapped 0 / **host 2,475**(85.2%)이고, host의
절반 이상이 단일 주소 `0x77BE33AC`(1,626 = 전체의 56.0%)입니다. 프로세스는 WOW64이며
`repiu_loader_win32.exe`는 `0x10000000`, 32비트 `ntdll`은 `0x77BE0000` 대역이므로
**이 주소는 우리 코드가 아니라 커널 전이 경로**입니다.

**5c. 포트 I/O는 전부 ISR이 만듭니다(항등식).**

| 실행 | port I/O 예외 | INT 8 주입 | 주입당 |
|---|---:|---:|---:|
| 240초 | 459,999 | 2,275 | **202.2** |
| 120초 | 88,214 | 451 | **195.6** |

지연 루프는 200회이므로 **주입 1회당 정확히 루프 한 번**입니다. 주소 census도
`0x0301DB22` 89,601회가 **전부 arena**이고 진입 직전 예외가 ISR 안 `0x0301F7CE`
single-step으로 Task 408의 서명과 같습니다.

**5d. 게스트는 tick을 소화하지 못합니다.** 120초에서 due 2,787 / injected 451 /
coalesced 1,681 / dropped 654입니다. 240초에서도 due 9,951 / injected 2,275 /
coalesced 7,048입니다. **프로그램된 주기를 따라가지 못하고 대부분 합쳐집니다.**

**5e. 게스트 스레드의 cycle은 예외 처리로 소진됩니다.** 120초 실행에서
guest-run 126.4 G cycle 중 **VEH gap 94.6 G(74.9%)** + VEH 본체 31.5 G(24.9%)로
**합이 사실상 100%** 입니다. gap을 예외 종류로 나누면 **breakpoint 78.3 G(62.0%)**,
port I/O 16.3 G(12.9%), single-step 0.07 G입니다. breakpoint 1건당 평균 gap은
**2,280,636 cycle**입니다.

## 6. 추정 — 멈춤은 대기가 아니라 포화입니다

5a~5e를 합치면 **게스트는 타이머 ISR과 그 예외 뒤처리 외에는 거의 아무것도 실행하지
못합니다.** `stage.cfg` 직후가 늘 정지 지점인 것도 이것으로 설명됩니다 — **240 Hz
슬롯이 등록되는 지점이 바로 거기**이고, 그 순간부터 tick마다 200회 포트 fault가
발생합니다.

**다만 "ISR 1회가 tick 주기(4.16 ms)를 넘는다"는 것은 아직 계산이지 측정이 아닙니다.**
5e가 가리키는 최대 인구도 port I/O가 아니라 **breakpoint gap(62%)** 이므로, 포화의
가장 큰 항목이 무엇인지는 아직 이름이 없습니다.

## 7. 미확정 — 다음 작업이 겨냥할 것

1. **breakpoint 뒤 2.28 M cycle이 어디서 쓰이는가.** census는 그 시간에 스레드가
   ntdll에 있다고 말하므로 게스트 캐시 실행이 아닙니다. 후보는 AOT 패치 경로의
   `VirtualProtect`/`FlushInstructionCache` 같은 syscall이며 **미측정**입니다.
   현재 census는 EIP 한 점만 보므로 **host 표본에 모듈+offset과 얕은 스택**을 붙이는
   것이 다음 계측입니다.
2. **예외 1건 가격이 세션 간 20배 차이 나는 이유.** Task 336은 전이 34,000 cycle을
   기록했는데 이번 세션은 예외 1건당 gap+본체가 약 950,000 cycle입니다. 이 차이가
   재현율 29% → 100%를 설명하는지 확인해야 합니다.
3. **AOT worker 대기 8.7%**(11.0 G cycle / 150 translate, append가 97.9%, 최대 append
   1.02 G cycle)는 지배 항목은 아니지만 무시할 수 없습니다.
4. **`0x03021F3A` 주변(비트스트림 리더)** 은 멈춤 초기에만 돌고 이후 표본이 늘지
   않습니다. 로딩 경로의 어느 자산인지는 미확인입니다.

## 8. 회고 — 무엇이 시간을 아꼈고 무엇이 틀렸나

* **정적 도구를 먼저 쓴 것이 하루치 계측을 없앴습니다.** 복귀 주소 census를 구현했다면
  "호출자는 타이머 ISR"이라는 같은 답을 몇 시간 뒤에 얻었을 것입니다.
* **첫 배치의 판정이 틀렸습니다.** frames 1~2를 "healthy"로 분류해 두 실행이 같은
  상태라는 것을 가릴 뻔했습니다. 스크립트를 stalled/healthy/**slow** 3분류로 고쳤고,
  판정 기준을 frames로 옮겼습니다.
* **census 자체의 교란은 작지 않습니다.** ON/OFF 사이에 traces 6→7, publishes
  84~100→100~101로 차이가 있습니다. 설계에 적어 둔 대로 **census를 켠 실행의 wall과
  프레임은 인용하지 않습니다.**

---

# Task 411 Work Log — a census of where the guest is during the stall

## 1. Result in one line

**The stall is not a guest wait loop.** The guest thread spends **85% of samples in
host/kernel code** (WOW64 `ntdll32`), and most of the remaining 15% is the **240 Hz timer
ISR's 200-iteration port delay loop**. The main flow makes **no progress at all**, even
over four times the wall clock.

## 2. Settled statically, with no code

`repiu_aot_probe --xref`/`--dump` **refuted the premise** of the previous "next target":
the delay routine `0x0301DB10` has exactly one caller (`0x03010BCF`, no absolute
references), that caller `0x03010BA4` is **timer slot 0's callback**, registered right
after `ParseStageCfg("stage.cfg")` at a programmed **240.0 Hz**, and the parser itself is
finite. So 13,173 is a **tick count, not a wait count** — about 220 Hz measured against
240 programmed, the same margin as Task 366's 11.9% tick loss. Detail is in the analysis
document's Confirmed 7 and the [EXE_DESIGN](../EXE_DESIGN.en.md) timer ISR section.

## 3. Implementation

Every existing instrument samples only at an exception, so code running in the AOT cache
without faulting was invisible. The new census samples on a wall-clock interval: a new
header and source under `telemetry/`, an owning pointer in `ThreadContext`, the sampling
site in the poll loop **without** the dispatch-quiet gate, snapshot and dump wiring, log
output, and a repeat-and-classify script. Capture reuses
`CaptureWin32NativePhaseSample`, so its suspend/resume contract and reverse-map safety
argument carry over unchanged. **The Release build passes with zero errors** (only the
pre-existing C4819 code-page warnings).

## 4. Measurement — pumpit3 stalled in **eleven of eleven runs** this session

One 45-second control and three 60-second runs with the census off, six 60-second and one
240-second run with it on, and one 120-second run with the census plus the execution-time
profile: **not one healthy run**. That differs from the 29% (five of seventeen, v0.0.128)
in the analysis document; run-to-run nondeterminism of the kind Tasks 404/405 already
recorded, at its extreme today. Every census run passed both gates — `sum == total` true,
`overflow` zero, `capture-failures` zero.

## 5. Confirmed — no progress, and almost no guest code either

**5a.** Four times the wall clock adds nothing but ISR samples: `0x0301DB24`/`0x0301DB22`
go 102/72 → **402/289** while the bitstream reader at `0x03021F3A` stays at **32** and its
neighbours barely move; DOS path traces stay at six and frames at one.

**5b.** In the 120-second run the origin split is arena 137, cache-mapped 292,
cache-unmapped 0, **host 2,475 (85.2%)**, and one address — `0x77BE33AC`, 1,626 samples,
**56.0% of everything** — carries most of it. The process is WOW64 with the loader at
`0x10000000` and 32-bit `ntdll` around `0x77BE0000`, so that address is the kernel
transition path, not our code.

**5c.** Port I/O is entirely the ISR's: 459,999 faults against 2,275 INT 8 injections
(**202.2** each) at 240 seconds and 88,214 against 451 (**195.6**) at 120 — one pass of the
200-iteration loop per injection. The address census agrees: 89,601 hits on `0x0301DB22`,
**all in the arena**, entered after a single step at `0x0301F7CE` inside the ISR, matching
Task 408's signature.

**5d.** The guest cannot keep up with its own timer: due 2,787 against 451 injected and
1,681 coalesced at 120 seconds; due 9,951 against 2,275 and 7,048 at 240.

**5e.** Its cycles go to exception handling: of 126.4 G guest-run cycles, the VEH gap is
94.6 G (74.9%) and the handler bodies 31.5 G (24.9%) — **essentially 100% together**. By
class the gap is **breakpoint 78.3 G (62.0%)**, port I/O 16.3 G (12.9%), single-step
0.07 G, with a mean of **2,280,636 cycles** after each breakpoint.

## 6. Inferred — saturation rather than a wait

Taken together, the guest executes almost nothing but the timer ISR and the exception work
behind it, which also explains why the stop is always just after `stage.cfg`: **that is
where the 240 Hz slot is registered**, and from that moment every tick costs 200 port
faults. **But "one ISR pass exceeds the 4.16 ms tick period" is still arithmetic, not
measurement**, and the largest population in 5e is not port I/O but the **breakpoint gap at
62%**, which has no name yet.

## 7. Unresolved — what the next task should target

Where the 2.28 M cycles after each breakpoint go (the census puts the thread in `ntdll`
there, so it is not guest cache execution; syscalls on the AOT patch path such as
`VirtualProtect` or `FlushInstructionCache` are the untested candidates, and the next
instrument is **module plus offset and a shallow stack on host samples**); why one
exception costs about 950,000 cycles here against Task 336's 34,000-cycle transition, and
whether that gap explains 29% versus 100% reproduction; the AOT worker wait at 8.7% (11.0 G
cycles over 150 translations, 97.9% of it in append, one append reaching 1.02 G); and which
asset the bitstream reader around `0x03021F3A` was decoding before it went quiet.

## 8. Retrospective

Reaching for the static tools first removed a day of instrumentation — a return-address
census would have produced the same "the caller is the timer ISR" answer hours later. The
first batch's classifier was wrong, calling one- and two-frame runs "healthy" and nearly
hiding that both classes were the same state; it now separates stalled, healthy, and
**slow**, and judges on frames. And the census is not free: with it on, path traces went
6 → 7 and publishes 84-100 → 100-101, so as the design already required, **runs with the
census enabled are not quotable for wall time or frames.**
