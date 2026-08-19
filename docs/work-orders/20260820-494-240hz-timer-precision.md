# 20260820-494 240Hz 타이머 인터럽트 정밀도 개선 작업 지시서 / Work order

## 한국어

### 목적

원본 PIT divisor `4972`가 요구하는 약 240Hz IRQ0를 host poll 양자화의 영향을
최소화하면서 원본 guest ISR로 전달합니다.

### 작업

- [x] 기존 PIT clock 비율 계산을 유지하고 next-deadline API를 구현합니다.
- [x] Win32 poll loop를 deadline-aware wait/spin 정책으로 연결합니다.
- [x] PIT 및 schedule probe 회귀 검사를 추가합니다.
- [x] 설계·작업 로그와 관련 분석 문서를 갱신합니다.
- [x] Win32 빌드와 AOT probe를 실행합니다.

### 영향 범위

공용 PIT HLE 계산, Win32 telemetry poll 대기, AOT probe에 한정합니다. 원본 실행
파일, guest INT 8 handler, 기존 IRQ pending/backlog semantics는 변경하지 않습니다.

### 최소 검증

`pit_timer_probe`, `timer_tick_delivery_probe`, 전체 AOT probe, Win32 x86 Debug
빌드 및 가능한 범위의 짧은 runtime smoke를 수행합니다.

## English

### Objective

Deliver the approximately 240Hz IRQ0 requested by original PIT divisor `4972` to the
original guest ISR with minimal host poll quantization.

### Work items

- [x] Preserve the existing cumulative clock-ratio calculation and implement the
  next-deadline API in `PitIrqSchedule`.
- [x] Connect the Win32 poll loop to deadline-aware wait/spin policy.
- [x] Add PIT and schedule regression probes.
- [x] Update design, work log, and relevant analysis documentation.
- [ ] Run the Win32 build and AOT probes.

### Scope

Limit changes to shared PIT HLE calculation, Win32 telemetry poll waiting, and AOT
probes. Do not change the original executable, guest INT 8 handler, or existing IRQ
pending/backlog semantics.

### Minimum verification

Run `pit_timer_probe`, `timer_tick_delivery_probe`, the full AOT probe, the Win32 x86
Debug build, and a short runtime smoke where available.
