# 작업 로그: 커널 예외 전달 비용 계측 / Work log: measuring kernel exception-delivery cost

Task 372. 설계 [20260731-372](../design/20260731-372-kernel-exception-delivery-cost.md),
작업 지시 [20260731-372](../work-orders/20260731-372-kernel-exception-delivery-cost.md)

## 한국어

### 결과: 예외 기구가 wall의 40.5~48.7%

interval 0 고정, 자동 장면 70초, wall 259,096,642,075 cycle, 예외 2,081,859건.

| gap 분류 | 총 cycles | 건수 | 평균 |
|---|---:|---:|---:|
| single-step | 29,419,740,817 | 926,025 | **31,769** |
| breakpoint | 167,210,644,267 | 1,081,430 | 154,619 |
| other | 2,296,832,875 | 74,403 | 30,870 |
| **최소 gap** | | | **21,534** |

`unclassified=0`, `clamped=0` — 모든 gap이 분류됐고 시각 역전이 없습니다.

**single-step 평균 31,769 cycle(8.6 µs)이 커널 왕복 추정치입니다.** 연속된 두
single-step 사이에서 게스트는 명령 1개만 실행하므로 그 gap은 사실상 왕복 그
자체입니다.

| 추정 기준 | 왕복 총량 | wall 비중 |
|---|---:|---:|
| 평균 31,769 × 2,081,859 | 66.14e9 | **25.5%** |
| 최소 21,534 × 2,081,859 (보수적) | 44.83e9 | **17.3%** |

VEH 핸들러 본체가 23.18%이므로 **예외 기구 총계는 wall의 40.5% ~ 48.7%**입니다.

### 교차 검증 3건이 모두 일치합니다

1. **합성 캘리브레이션과의 대조.** Task 340대의
   `exception_transition_calibration_probe`는 최소 핸들러로 single-step 25,855 /
   INT3 21,347 cycle을 잽니다. 실측 평균 31,769는 그보다 23% 높은데, 실제 핸들러가
   캐시·TLB를 더 더럽히므로 예상되는 방향입니다.
2. **최소 gap 21,534 vs 합성 INT3 21,347 — 0.9% 차이.** 서로 다른 방법으로 잰
   왕복 하한이 거의 정확히 만납니다.
3. **구성 간 안정성.** single-step 평균이 interval 1에서 31,761, interval 0에서
   31,769로 **8 cycle(0.03%) 차이**입니다. 장면·구성과 무관한 고정 비용이라는 뜻이며,
   이것이 이 측정을 신뢰할 수 있는 가장 강한 근거입니다.

### 정정: Task 368의 판정은 유효하며, 이 측정이 그것을 **뒷받침**합니다

**작성 당시 이 절은 "Task 368의 종결 판정을 철회한다"고 썼습니다. 그것은
오류였습니다.**

Task 368은 prologue 2,768 cycle만 본 것이 아닙니다. 작업 로그의 비용표를 보면 커널
전이를 Task 336 가격으로 **호출당 34,521 cycle** 별도 계상하고 있습니다. 이번
실측치 31,769와 **8% 차이**로, 이 측정은 368을 뒤집는 것이 아니라 **확인**합니다.

368이 Glide gate에서 예외 제거의 이득을 3.25%로 판정한 이유는 커널 전이를 못 봐서가
아니라, **gate 본체가 호출당 약 235,000 cycle이고 예외를 없애도 그것이 그대로 남기
때문**입니다. 예외로 도달하는 비용은 그 위의 얇은 층입니다. **Glide gate에 대한 368의
종결은 유효합니다.**

### 그렇다면 Task 372가 더한 것은 무엇인가

**모집단입니다.** 368은 Glide gate 하나를 평가했고, 이 측정은 **모든 예외**를 덮습니다.

사용자 캡처(623,056건) 기준 구성입니다.

| 모집단 | 건수 | 비중 | 예외당 "본체" 비용 |
|---|---:|---:|---|
| Glide gate (breakpoint의 70.5%) | 233,754 | 37.5% | 약 235,000 cycle — **크다** |
| 그 외 breakpoint | 97,849 | 15.7% | 미측정 |
| **single-step** | **264,561** | **42.5%** | **명령 1개 — 거의 0** |
| AV / 기타 | 26,892 | 4.3% | 미측정 |

**결정적 차이는 본체 비용입니다.** Glide gate는 예외가 비싼 본체 위의 얇은 층이라
제거해도 이득이 작습니다. **single-step은 정반대입니다** — 본체가 명령 1개
에뮬레이션이므로 31,003 cycle 왕복이 사실상 비용 전부입니다.

single-step gap 총량만으로 **wall의 9.06%**이고, 여기에 해당 핸들러 본체 시간이
더해집니다. **예외 제거가 실제로 값이 나가는 곳은 Glide gate가 아니라 여기입니다.**

### 목표까지의 거리

Task 371이 잰 프레임당 CPU는 20.3 ms이고, **16.7 ms 아래로 내리면 배포 구성(vsync)이
30 → 60 fps로 넘어갑니다.** 필요한 단축은 약 **18%(1.22배)** 입니다.

예외를 완전히 제거하면 상한이 **1.68 ~ 1.95배**이므로 목표를 여유 있게 넘습니다.
물론 상한이지 달성치는 아니며, 게스트 명령을 예외 없이 실행하는 경로가 어디까지
가능한지가 다음 질문입니다.

### 부수 관측: 번역 코드 실행 구간

breakpoint gap 평균 154,619 cycle에서 왕복 31,769를 빼면 약 **122,850
cycle(33.3 µs)** 가 HLE 경계 사이에서 실제로 실행된 번역 코드입니다.

### 구현

* `Win32ExecutionTimeProfile`에 gap 상태(`veh_last_exit_cycles`,
  `veh_gap_pending_cycles`)와 분류별 누적, min/max, unclassified, clamped 추가.
* `ExecutionTimeScope`의 `kVehTotal` 생성자에서 직전 퇴출 시각과의 차이를 banking,
  소멸자에서 퇴출 시각 저장. `owns_veh_depth_`를 따라 **중첩 예외는 제외**합니다.
* 분류는 `RecordVehExceptionCensus`에서. exception code가 검증된 뒤라 Task 296의
  malformed record 위험이 없습니다.
* **hot path에 clock 읽기 추가 0회** — 이미 찍히는 두 타임스탬프의 차이만 씁니다
  (Task 353 관측자 규칙).

### 검증

* Debug/Release 빌드 성공, `repiu_aot_probe` exit 0, 신규 probe **7개 항목 전부
  true**(첫 프레임 미banking, 분류, 잔여 배수, 극값 추적, 미분류 잔류, 중첩 제외,
  `nullptr` 무해).

### 부수 발견: 1초 무진행 watchdog

`PollThreadUntilExit`에 설정 타임아웃과 별개로 **1초 무진행 watchdog**이 있어
([live_telemetry_snapshot.cpp:431](../../src/platform/win32/telemetry/live_telemetry_snapshot.cpp#L431)),
조기 종료해도 `timed_out=true`로 보고합니다. 70초로 설정한 실행이 27.1초에 끝난
사례를 관측했습니다.

이 때문에 **프레임 수만으로 실행을 비교하면 안 됩니다.** Task 371의 A/B가 같은 길이
전제였으므로 재검증했고, 두 실행의 wall이 259,137,967,968 / 259,096,642,075로
**0.016% 차이**여서 결론은 유효했습니다(프레임 2,323 → 3,802, **+63.7%**, 최초
+62.1%와 재현). **앞으로 A/B는 wall cycle을 함께 확인합니다.**

### 다음: Task 373

**single-step 모집단 제거**입니다. Glide gate는 368이 이미 닫았고, 남은 큰 모집단 중
본체가 싼 것은 single-step뿐입니다. 설계는
[20260731-373](../design/20260731-373-single-step-population-elimination.md).

---

## English

### Result: exception machinery is 40.5 to 48.7% of wall

Over a 70-second automated scene pinned at swap interval 0 (wall 259,096,642,075
cycles, 2,081,859 exceptions), the single-step gap averages **31,769 cycles**
(8.6 µs) across 926,025 samples, with a minimum observed gap of **21,534**. Because
the guest executes exactly one instruction between two consecutive single steps,
that average is effectively the kernel round trip. Multiplied across every
exception it is 25.5% of wall at the mean and 17.3% at the conservative floor;
adding the 23.18% spent inside handler bodies puts the total exception machinery at
**40.5 to 48.7% of wall**. No gap was left unclassified and no timestamp inverted.

Three independent checks agree. The synthetic calibration probe prices a minimal
handler's single-step round trip at 25,855 cycles, 23% below the live mean, which is
the expected direction for a heavier handler. The measured floor of 21,534 lands
within 0.9% of that probe's INT3 figure of 21,347. And the single-step mean moves by
eight cycles — 0.03% — between swap interval 1 and 0, which is what a fixed
OS-and-hardware cost should do and is the strongest reason to trust the number.

### Correction: Task 368 stands, and this measurement supports it

This section originally claimed Task 368's closure was withdrawn. That was wrong.
Task 368 did not stop at the 2,768-cycle prologue: its cost table prices the kernel
transition separately at 34,521 cycles per call using Task 336's calibration, which
this measurement's 31,769 confirms to within 8%. Task 368 found only 3.25% available
on the Glide gate not because it missed the transition but because the gate body
costs about 235,000 cycles per call and survives the exception's removal. Its
closure of the Glide gate is valid.

What Task 372 adds is the population. Task 368 evaluated one boundary; this covers
every exception. In a 623,056-exception capture the Glide gate is 233,754 (37.5%),
other breakpoints 97,849 (15.7%), single steps 264,561 (42.5%), and the rest 4.3%.
The deciding difference is the body cost behind each: the Glide gate is a thin
exception layer over expensive work, while a single step's body is emulating one
instruction, so its 31,003-cycle round trip is essentially the entire cost. Single
step gaps alone are 9.06% of wall before handler bodies. That, not the Glide gate,
is where removing exceptions pays.

### Distance to the target

Task 371 measured 20.3 ms of CPU per frame, and crossing below 16.7 ms flips the
shipped vsync configuration from 30 to 60 fps — about 18%, or 1.22x. Removing every
exception bounds improvement at 1.68 to 1.95x, but that ceiling is not evenly
distributed: most of the Glide gate's share survives removal because its body is
expensive, while the single-step population's does not. Task 373 takes that
population.

Incidentally, subtracting the round trip from the 154,619-cycle breakpoint gap
leaves about 122,850 cycles — 33.3 µs — of translated code actually executing
between HLE boundaries.

### Implementation and verification

The profile gained gap state and per-class accumulators; the `kVehTotal` scope banks
the interval since the previous exit and stores its own exit, following
`owns_veh_depth_` so nested faults are excluded, and classification happens in
`RecordVehExceptionCensus` after the exception record has been validated. **No clock
read was added to the hot path** — the difference between two existing timestamps is
all it uses. Both configurations build and the probe passes all seven assertions.

### A side finding worth keeping

`PollThreadUntilExit` carries a one-second no-progress watchdog separate from the
configured timeout, and it also reports `timed_out=true`, so a run configured for 70
seconds was observed ending at 27.1. Runs therefore cannot be compared by frame
count alone. Task 371's A/B assumed equal duration, so it was re-verified: the two
runs' wall clocks differ by 0.016% and the conclusion held, with frames going 2,323
to 3,802 — plus 63.7%, reproducing the original 62.1%. Future A/Bs check wall cycles
alongside frames.
