# 20260728-335 작업 지시: gate 진입 pump rendezvous 제거 / Work order

설계: [20260728-335-glide-gate-pump-rendezvous.md](../design/20260728-335-glide-gate-pump-rendezvous.md)

## 한국어

### 목표

gate 진입마다 발생하는 중복 `PumpEvents` rendezvous를 없애 Glide gate 비용(17.14%)을
줄인다. 예측은 전체 wall-clock 약 8.5% 절감이다.

### 범위

**포함**

* `HandleLinexeGlideBoundary`의 `PumpEvents()` 호출 제거와
  `REPIU_GLIDE_GATE_PUMP=1` 복원 스위치.
* Release 60초 A/B와 gate G1~G4 판정.

**제외**

* rendezvous 자체의 지연 축소(스레드 전환 비용).
* VEH residual 11.19% 귀속(별도 Task).
* handler 축 중첩(`return` 484.73%) 수정(별도 Task).

### 구현 지침

* SDL 이벤트 처리 스레드는 바꾸지 않는다. host loop가 계속 유일한 pump 호출자다.
* 스위치는 프로세스당 1회 해석한다. gate는 hot path다.

### 검증 절차

1. Debug/Release 전체 빌드 통과.
2. `repiu_aot_probe` 두 구성 exit 0.
3. Release 60초 A/B(OFF/ON) — gate 진입당 rendezvous, Glide gate 비중, 프레임,
   progress 비교. malformed 0, fatal 0, Glide 공백 0 유지.
4. 창이 계속 응답하는지, 종료 요청이 동작하는지 확인한다.

---

## English

### Goal

Remove the duplicate `PumpEvents` rendezvous taken on every Glide gate entry, cutting the gate's
17.14% share; the pre-registered prediction is roughly 8.5% of guest wall clock.

### Scope

In scope: removing the `PumpEvents` call from `HandleLinexeGlideBoundary` with a
`REPIU_GLIDE_GATE_PUMP=1` restore switch, and a 60-second Release A/B judged against gates G1-G4.
Out of scope: reducing the rendezvous latency itself, attributing the 11.19% VEH residual, and
fixing the handler axis overlap, which are separate tasks.

### Implementation notes

The thread that processes SDL events does not change; the host loop remains the only caller. The
switch is resolved once per process because the gate is a hot path.

### Verification

Full Debug and Release builds, `repiu_aot_probe` exiting 0 in both configurations, a 60-second
Release A/B comparing rendezvous per gate entry, the Glide gate share, frames, and progress with
malformed, fatal, and Glide-gap counts staying at zero, and a check that the window still responds
and the exit request still works.
