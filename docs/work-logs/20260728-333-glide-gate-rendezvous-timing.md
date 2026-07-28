# 20260728-333 작업 로그: Glide gate rendezvous 분해와 제거 / Work log

설계: [20260728-333-glide-gate-rendezvous-timing.md](../design/20260728-333-glide-gate-rendezvous-timing.md)

작업 지시: [20260728-333-glide-gate-rendezvous-timing.md](../work-orders/20260728-333-glide-gate-rendezvous-timing.md)

## 한국어

### 결론 요약

**gate G1이 압도적으로 성립했습니다. Glide gate 비용의 95.67%는 host thread를 기다리는
시간이고, host의 실제 작업은 1.83%입니다.**

원인은 host poll loop가 매 iteration 끝에서 `Sleep(1)` 했다는 것입니다. guest가 게시한
command는 **다음 pump까지** 방치됐고, 그 대기가 rendezvous 1회당 평균
`4,114,686 tick`(약 1.65ms)이었습니다.

`Sleep(1)`을 **같은 condition variable에 대한 1ms 상한 대기**로 바꾸자 rendezvous
1회 비용이 `4,300,882 → 192,482 tick`(**1/22.3**)이 되고, 60초 프레임이
**277 → 876(3.16배)**, progress가 **64,794 → 84,855(1.31배)** 가 됐습니다.

### 사전 등록 gate 판정

| gate | 조건 | 관측 | 판정 |
|---|---|---:|---|
| **G1** | `wake` >= 60% | **95.67%** | **성립** |
| G2 | `work` >= 60% | 1.83% | 기각 |
| G3 | `queue` >= 20% | 0.07% | 기각 |
| G4 | `complete` >= 20% | 2.43% | 기각 |
| G5 | gate 1회당 rendezvous >= 2 | 1.92 | 기각(경계값) |
| G6 | 어느 구간도 60% 미만 | `wake` 95.67% | 기각 |

**G5는 문턱값 2.0에 대해 1.92로 기각입니다.** 다만 gate 진입 11,824회에 rendezvous가
22,684회라는 사실 자체는 남습니다. gate 진입 직후의 `PumpEvents()`가 guest thread에서
별도 rendezvous를 만들기 때문이며, 대기가 싸진 지금은 우선순위가 낮습니다.

### 측정 값 — 수정 전 (`REPIU_GLIDE_HOST_WAIT=0`)

rendezvous 22,684회, 총 `97,561,214,884 tick`.

| 구간 | 비중 | 회당 tick |
|---|---:|---:|
| `queue` (앞 command 대기) | 0.07% | — |
| **`wake` (게시 → host가 집어감)** | **95.67%** | **4,114,686** |
| `work` (host의 실제 실행) | 1.83% | 78,804 |
| `complete` (완료 → guest 재개) | 2.43% | — |
| residual | 0.00% | — |
| **합계** | 100% | **4,300,882** |

**확인됨:** 분해 residual이 0.00%이므로 네 구간이 rendezvous를 정확히 분할합니다.

**확인됨:** 회당 `wake` 약 1.65ms는 poll iteration 주기와 일치합니다. 같은 실행에서
poll iteration은 60초에 31,633회, 즉 iteration당 약 1.90ms였습니다. 설계가 코드
읽기로 세운 가설(`Sleep(1)` 주기가 곧 대기)이 그대로 확인됐습니다.

### 측정 값 — 수정 후 (기본값)

| 항목 | 수정 전 | 수정 후 | 비 |
|---|---:|---:|---:|
| progress | 64,794 | 84,855 | **1.31배** |
| heartbeat | 818,298 | 1,153,307 | 1.41배 |
| 프레임(`grBufferSwap`) | 277 | **876** | **3.16배** |
| Glide gate 진입 | 11,824 | 37,145 | 3.14배 |
| rendezvous 1회 총비용 | 4,300,882 | **192,482** | **1/22.3** |
| 그중 `wake` | 4,114,686 | 75,178 | 1/54.7 |
| Glide gate의 wall-clock 비중 | 60.18% | **8.88%** | — |
| poll iterations | 31,633 | 88,696 | 2.80배 |

**확인됨:** 동등성은 유지됩니다. 두 실행 모두 60초 정상 timeout,
`exception dispatch malformed 0`, `original fatal halt reached: false`,
`Glide implementation issues 0/0/0/0/0/0`입니다.

**확인됨:** 전체 축이 다시 움직였습니다. `veh 81.42% → 62.99%`,
`glide-gate 60.18% → 8.88%`, `unaccounted(AOT 캐시 내 guest 실행) 18.58% → 37.01%`.
즉 **해방된 시간이 실제 guest 실행으로 갔습니다.**

**확인됨:** 수정 후 rendezvous 분포는 `wake 39.06% / work 28.30% / complete 31.68%`로
평탄해졌습니다. 남은 `wake`는 pump 주기가 아니라 스케줄러 기상 지연입니다(2코어에
스레드 5개).

### 구현

* `GlideOpenGlBackend::WaitAndPumpHostCommands(timeout_ms)` 추가. host thread가
  `host_command_cv_`에서 command를 기다리다가 오면 즉시 실행하고, 없으면 timeout으로
  깨어나 기존 poll 주기를 유지합니다. lock은 대기 구간에서만 잡고 command 실행은
  `PumpHostCommands()`가 lock 밖에서 수행합니다.
* poll loop 말미의 `Sleep(1)`을 그 호출로 교체했습니다. `REPIU_GLIDE_HOST_WAIT=0`이면
  기존 `Sleep(1)`으로 되돌아가며, 이 A/B는 그 스위치로 수행했습니다.
* 계측은 새 환경변수를 만들지 않고 기존 `REPIU_EXECUTION_TIME_PROFILE`을 씁니다.
  atomic을 추가하지 않았고, 기존 mutex/condition_variable의 happens-before에
  의존합니다. in-flight command가 1개이므로 handoff 타임스탬프는 스칼라 3개입니다.

### 검증 결과

1. Release 전체 빌드 통과, Debug 전체 빌드 통과.
2. `repiu_aot_probe` 두 구성 exit 0. 신규 `glide_gate_timing_*` 5개 항목 전부 통과
   (구간 배정, 누적과 최댓값, direct 축 분리, 역행 TSC clamp 5회, null profile 무동작).
3. Release 60초 A/B 2회(OFF/ON) 모두 정상 timeout, malformed 0, fatal 0, Glide 공백 0.

### 관찰 — 재현되지 않은 조기 crash

OFF 첫 실행이 약 7초 뒤 `0xC0000005`로 종료했습니다. 예외 주소 `0x10082750`은
**host 이미지(base `0x10000000`) 내부**이고 faulting 명령은 `movups xmm0,[eax]`에
`EAX=0`이었습니다. 같은 설정의 재실행은 정상 60초를 완주했고(progress 64,794),
수정 전 baseline(Task 331, progress 64,347)과도 일치합니다.

**미확정:** 이 crash는 재현되지 않았고 이번 변경과의 인과도 확인되지 않았습니다.
host 코드에서 null을 역참조하는 간헐 경로가 있다는 사실만 기록합니다.

### 확인됨 / Confirmed

* Glide gate 비용의 95.67%는 host pump 주기 대기였고, host 작업은 1.83%였습니다.
* `Sleep(1)` 제거로 rendezvous 1회가 22.3배 싸지고 프레임이 3.16배, progress가
  1.31배가 됐습니다.
* 동등성(malformed/fatal/Glide 공백)은 유지됩니다.

### 미확정 / Unresolved

* 수정 후 남은 병목: `unaccounted 37.01%`(AOT 캐시 내 guest 실행)와
  `veh-exclusive`입니다. 다음 재귀속 대상입니다.
* gate 진입 1회당 rendezvous 1.92회. `PumpEvents()`를 gate 경로에서 빼면 줄지만
  대기가 싸진 지금은 이득이 작습니다(미측정).
* 위 조기 crash.
* A/B는 조건당 1회 표본입니다.

---

## English

### Summary

Gate G1 holds decisively: 95.67% of the Glide gate's cost was waiting for the host thread and only
1.83% was the host executing the command. The cause is that the host poll loop ended each iteration
with `Sleep(1)`, so a command the guest published was left until the next pump — averaging
`4,114,686` ticks, about 1.65ms, per rendezvous. Replacing that sleep with a 1ms-bounded wait on
the same condition variable cut one rendezvous from `4,300,882` to `192,482` ticks (1/22.3), raised
the 60-second frame count from 277 to 876 (3.16x), and progress from 64,794 to 84,855 (1.31x).

### Gate results

G1 holds at 95.67%. G2 is rejected at 1.83%, G3 at 0.07%, G4 at 2.43%, and G6 by G1 holding. G5 is
rejected at 1.92 rendezvous per gate entry against its 2.0 threshold, though the underlying fact
stands: 11,824 gate entries produced 22,684 rendezvous because the gate calls `PumpEvents` on entry,
which is its own rendezvous from the guest thread. With waiting now cheap it is a low priority.

### Measurements

Before the fix, across 22,684 rendezvous totalling `97,561,214,884` ticks, the split was 0.07%
`queue`, 95.67% `wake`, 1.83% `work`, and 2.43% `complete`, with a 0.00% residual confirming the
four intervals partition the rendezvous exactly. The mean `wake` of about 1.65ms matches the poll
loop's own cadence in the same run — 31,633 iterations in 60 seconds, about 1.90ms each — so the
design's code-reading hypothesis is confirmed rather than merely plausible.

After the fix, progress went from 64,794 to 84,855, heartbeat from 818,298 to 1,153,307, frames from
277 to 876, gate entries from 11,824 to 37,145, and the per-rendezvous cost from `4,300,882` to
`192,482` ticks. The Glide gate's share of guest wall clock fell from 60.18% to 8.88% while
unaccounted AOT cache execution rose from 18.58% to 37.01%, so the freed time went into real guest
execution. Both runs reached the 60-second timeout with zero malformed dispatch, no fatal halt, and
no Glide gap. The remaining rendezvous split is flat at 39.06% `wake`, 28.30% `work`, and 31.68%
`complete`, where `wake` is now scheduler latency rather than a pump cadence, with five threads on
two cores.

### Implementation

`GlideOpenGlBackend::WaitAndPumpHostCommands` waits on the existing condition variable for a
command and executes it immediately, or wakes on the timeout to preserve the old poll cadence; the
lock is held only across the wait, and the command still runs outside it in `PumpHostCommands`. The
poll loop's trailing `Sleep(1)` now calls that, with `REPIU_GLIDE_HOST_WAIT=0` restoring the sleep,
which is how this A/B was run. The instrumentation adds no environment variable, reusing
`REPIU_EXECUTION_TIME_PROFILE`, and adds no atomics, relying on the happens-before the existing
mutex and condition variable supply; a single in-flight command makes three scalar handoff
timestamps sufficient.

### Verification

Full Release and Debug builds passed and `repiu_aot_probe` exited 0 in both configurations, with
the new `glide_gate_timing_*` group passing all five checks: interval assignment, accumulation and
maxima, the direct axis staying separate, five clamped backwards-TSC samples, and a null profile
staying inert. Both 60-second Release runs reached their timeout with malformed, fatal, and
Glide-gap counts at zero.

### An early crash that did not reproduce

The first OFF run exited after about seven seconds with `0xC0000005` at `0x10082750`, inside the
host image based at `0x10000000`, faulting on `movups xmm0,[eax]` with `EAX=0`. A repeat run of the
same configuration completed the full 60 seconds at progress 64,794, matching Task 331's pre-change
baseline of 64,347. Unresolved: the crash did not reproduce and no causal link to this change was
established; only the existence of an intermittent null dereference in host code is recorded.

### Unresolved

What now dominates after the fix — 37.01% unaccounted AOT cache execution and the VEH-exclusive
remainder — is the next attribution. The 1.92 rendezvous per gate entry could be reduced by taking
`PumpEvents` off the gate path, but the gain is small now that waiting is cheap and was not
measured. The early crash above stands open, and the A/B is a single sample per condition.
