# 20260728-333 작업 지시: Glide gate rendezvous 분해 / Work order

설계: [20260728-333-glide-gate-rendezvous-timing.md](../design/20260728-333-glide-gate-rendezvous-timing.md)

## 한국어

### 목표

Release 기준 1위 병목인 Glide gate(60.78%, 호출당 약 1.85ms)를 `queue`/`wake`/`work`/
`complete`로 분해해 **대기인지 작업인지** 확정하고, gate G1이 성립하면 같은 작업에서
고친다.

### 범위

**포함**

* `include/repiu/platform/win32/glide_gate_timing.h`,
  `src/platform/win32/telemetry/glide_gate_timing.cpp` 신규.
* `GlideOpenGlBackend`에 handoff 타임스탬프 기록 추가(`InvokeOnHostThread`,
  `PumpHostCommands`).
* 로더 종료 summary에 rendezvous 분해 출력.
* `repiu_aot_probe`에 누적·스냅샷 결정론 검증 그룹 추가.
* G1 성립 시: host poll loop의 `Sleep(1)`을 1ms 상한 command 대기로 교체하고 60초 A/B.

**제외**

* GL 작업량 축소, 배치, 렌더링 의미 변경(G2 성립 시 별도 Task).
* OpenGL context 소유권 변경.

### 구현 지침

* 새 환경변수를 만들지 않는다. 기존 `REPIU_EXECUTION_TIME_PROFILE` opt-in을 쓴다.
* atomic을 추가하지 않는다. 대기 시간이 측정 대상이므로 계측이 그것을 바꾸면 안 되며,
  mutex/condition_variable이 이미 happens-before를 준다.
* in-flight command는 1개이므로 handoff 타임스탬프는 스칼라로 둔다.
* 역행 TSC 표본은 0으로 clamp하고 세되 실패로 만들지 않는다.
* probe 통과 조건은 결정론적 사실만 쓴다. 타이밍 값은 보고만 한다.

### 검증 절차

1. Debug/Release 전체 빌드 통과.
2. `repiu_aot_probe` 두 구성 exit 0, 신규 `glide_gate_timing_*` 그룹 통과.
3. Release 60초 `pumpit1` 실행으로 gate 판정.
4. G1 성립 시 수정 후 Release 60초 A/B: progress·프레임 개선, malformed 0, fatal 0,
   Glide 공백 0 유지.

---

## English

### Goal

Split the Glide gate — Release's top bottleneck at 60.78% of wall clock and about 1.85ms per entry
— into `queue`, `wake`, `work`, and `complete` to settle whether it is waiting or work, and fix it
in this same task if gate G1 holds.

### Scope

In scope: a new `glide_gate_timing` header and source, handoff timestamps recorded in
`GlideOpenGlBackend::InvokeOnHostThread` and `PumpHostCommands`, a rendezvous breakdown in the
loader's exit summary, a deterministic accumulation group in `repiu_aot_probe`, and — if G1 holds —
replacing the host poll loop's `Sleep(1)` with a 1ms-bounded command wait plus a 60-second A/B. Out
of scope: reducing or batching GL work and any rendering-semantic change, which belong to a
separate task if G2 holds, and any change to OpenGL context ownership.

### Implementation notes

No new environment variable: the existing `REPIU_EXECUTION_TIME_PROFILE` opt-in is reused. No added
atomics, because waiting is the quantity under measurement and the existing mutex and condition
variable already supply happens-before. A single in-flight command means scalar handoff timestamps
suffice. Backwards TSC samples clamp to zero and are counted rather than failed on. Probe pass
conditions use deterministic facts only; timings are reported, never gated.

### Verification

Full Debug and Release builds, `repiu_aot_probe` exiting 0 in both configurations with the new
`glide_gate_timing_*` group passing, a 60-second Release `pumpit1` run to judge the gates, and — if
G1 holds — a 60-second Release A/B after the fix showing improved progress and frame count while
malformed, fatal, and Glide-gap counts stay at zero.
