# Task 493 작업 로그: JAMMA IRQ0 프레임 수명 추적

## 결과

Task 492 실제 로그의 `replay-reads=0` 원인을 IF gate로 확인하고, due-time replay 범위를
중첩 가능한 IRQ0 frame stack으로 교체했습니다.

- 각 frame은 due timestamp와 interrupt-frame ESP를 보존합니다.
- 새 INT 8 주입 전과 JAMMA read 시 완료된 frame을 ESP로 정리합니다.
- IF가 켜진 handler에서도 가장 안쪽 활성 frame의 state를 사용합니다.
- nested IRETD 뒤에는 바깥 frame state가 다시 활성화됩니다.
- 종료 진단에 `frames-retired`, `frame-overflow`, `active-depth`를 추가했습니다.

## 검증

- Win32 x86 Debug `repiu`, `repiu_aot_probe`: 성공
- `repiu_aot_probe --jamma-input-timeline`: 성공
  - `reads=4`, `retired=3`, `frame_overflow=0`
- pumpit1 전체 AOT probe: 성공, exit code 0
- pumpito 10초 smoke 실행:
  - `replays=264`, `replay-reads=2374`
  - `frames-retired=258`, `frame-overflow=0`, `active-depth=6`
  - timeout은 의도된 10초 wall 제한이며 exception/stall timeout은 없었습니다.
- 최초 전체 build script는 120초 도구 제한에 걸렸으나, 같은 생성 디렉터리의 증분 target
  build가 성공하여 두 실행 파일의 최종 link를 확인했습니다.

후속 120초 사용자 실행은 `replays=27814`, `replay-reads=275350`,
`frames-retired=27812`, `frame-overflow=0`, `active-depth=2`를 기록했습니다. 종료 시 두 active
frame은 주입과 완료 차이와 일치하며, 실제 rapid press/release 검증도 완료됐습니다.

---

# Task 493 Work Log: Tracking JAMMA IRQ0 Frame Lifetime

## Result

The IF gate was confirmed as the cause of Task 492's live `replay-reads=0`, and due-time replay
scope now uses a nestable IRQ0 frame stack.

- Each frame preserves its due timestamp and interrupt-frame ESP.
- Completed frames are retired by ESP before a new INT 8 injection and at JAMMA reads.
- An IF-enabled handler uses the innermost active frame's state.
- Returning from a nested IRETD exposes the outer frame state again.
- Final diagnostics now include `frames-retired`, `frame-overflow`, and `active-depth`.

## Verification

- Win32 x86 Debug `repiu` and `repiu_aot_probe`: passed.
- `repiu_aot_probe --jamma-input-timeline`: passed with `reads=4`, `retired=3`, and
  `frame_overflow=0`.
- Complete pumpit1 AOT probe: passed with exit code zero.
- A 10-second pumpito smoke run reported `replays=264`, `replay-reads=2374`,
  `frames-retired=258`, `frame-overflow=0`, and `active-depth=6`. The wall timeout was intentional;
  exception and stall timeout were false.
- The first full build script exceeded the 120-second tool limit, but incremental targets in the
  same generated directory completed and linked both executables successfully.

A subsequent 120-second user run reported `replays=27814`, `replay-reads=275350`,
`frames-retired=27812`, `frame-overflow=0`, and `active-depth=2`. The two active frames at shutdown
match the injection/retirement difference, and real rapid press/release validation is complete.
