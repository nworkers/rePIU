# Task 493 작업 지시: JAMMA IRQ0 프레임 수명 추적

## 목표

Task 492 실제 실행에서 `replay-reads=0`이 된 IF gate를 제거하고, 중첩 가능한 IRQ0
프레임 stack으로 due-time JAMMA replay의 실제 소비 범위를 판정합니다.

## 작업 항목

- [x] 입력 timeline을 단일 활성 replay에서 고정 크기 frame stack으로 변경합니다.
- [x] INT 8 주입 전 ESP와 주입 frame ESP를 timeline에 전달합니다.
- [x] JAMMA port read가 IF와 무관하게 활성 IRQ0 frame을 사용하도록 변경합니다.
- [x] frame retire/overflow/active-depth 진단을 추가합니다.
- [x] IF-enabled 및 nested frame probe를 추가합니다.
- [x] Win32 Debug 빌드와 전체 AOT probe를 수행합니다.
- [x] analysis, architecture, TODO와 작업 로그를 갱신합니다.

## 완료 조건

자동 probe와 빌드가 통과하고, 다음 실제 플레이 로그에서 `replay-reads > 0` 및
`frame-overflow=0`을 확인할 수 있어야 합니다.

---

# Task 493 Work Order: Tracking JAMMA IRQ0 Frame Lifetime

## Goal

Remove the IF gate that produced `replay-reads=0` in the Task 492 live run and use a nestable IRQ0
frame stack to define the actual consumption scope of due-time JAMMA replay.

## Work Items

- [x] Replace the single active replay with a fixed-capacity frame stack.
- [x] Pass both pre-injection ESP and injected-frame ESP into the timeline.
- [x] Make JAMMA port reads consume an active IRQ0 frame regardless of IF.
- [x] Add frame-retire, overflow, and active-depth diagnostics.
- [x] Add IF-enabled and nested-frame probes.
- [x] Run Win32 Debug builds and the complete AOT probe.
- [x] Update analysis, architecture, TODO, and the work log.

## Completion Criteria

The automated probes and builds pass, and the next live-play log can demonstrate
`replay-reads > 0` with `frame-overflow=0`.
