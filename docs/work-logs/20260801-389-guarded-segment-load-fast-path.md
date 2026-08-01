# 20260801-389 Guarded Segment Load Fast Path 작업 로그 / Work Log

## 한국어

### 구현

- register-source `MOV Sreg, r16`을 `kGuardedSegmentLoad`로 분류했습니다.
- ES/DS/FS/GS와 ESP를 제외한 GPR source만 허용합니다. SS, ESP source, memory source는 기존 HLE boundary입니다.
- generated slot은 EFLAGS/EAX를 보존하고 source selector를 실제 CPU selector 및 HLE shadow와 차례로 비교합니다. 모두 같으면 selector 상태를 바꾸지 않고 fallthrough하며, 불일치는 상태 복구와 counter 기록 후 INT3/VEH로 fail closed 합니다.
- static placement, dynamic append, selector 재해석에 shadow 및 성공/복구 counter patch를 연결했습니다.
- `REPIU_AOT_GUARDED_SEGMENT_LOAD=1|on|true` opt-in을 추가했으며 기본값은 OFF입니다.

### 검증

- Win32 x86 Release `repiu_aot_probe`와 loader build: 성공. 기존 C4819/LNK4217 경고만 남았습니다.
- 두 PIU 실행 파일 구성의 전체 probe: exit 0.
- 신규 probe `guarded_segment_load_ready/layout/coverage/missing_fallback_rejected/patch/disabled_falls_back/rejected_forms=true`, `selector_guard_all=true`, `coherence_all=true`.
- 동일 3초 smoke:
  - off: 504 frame, 총 예외 48,320, breakpoint 27,328, effective `8E` 13,878.
  - on: 510 frame, 총 예외 36,397, breakpoint 15,302, effective `8E` 1,834, guarded success/fallback `12,045/1,597`.
- frame 정규화 결과 총 예외는 `95.87 -> 71.37`(-25.56%), breakpoint는 `54.22 -> 30.00`(-44.66%), `8E` 경계는 `27.54 -> 3.60`(-86.94%)입니다.
- 양쪽 모두 의도한 timeout 종료이며 final exception이 없고, Glide direct miss/terminal도 0입니다.

### 다음 검증

실제 Music Select 수동 캡처에서 장시간 성공/복구 비율, frame 정규화 예외, 화면·입력 회귀를 확인하기 전까지 기본값은 OFF로 유지합니다. `chdir`는 사용자 지시에 따라 후순위입니다.

## English

### Implementation

- Classified register-source `MOV Sreg, r16` as `kGuardedSegmentLoad`.
- Allowed ES/DS/FS/GS with GPR sources other than ESP. SS, ESP source, and memory source retain the existing HLE boundary.
- The generated slot preserves EFLAGS/EAX and compares the source selector with the physical CPU selector and HLE shadow. Equality leaves selector state unchanged and falls through; mismatch records a counter after restoring state and fails closed through INT3/VEH.
- Wired shadow and success/fallback counter patches through static placement, dynamic append, and selector re-resolution.
- Added the opt-in `REPIU_AOT_GUARDED_SEGMENT_LOAD=1|on|true`; default remains OFF.

### Verification

- Win32 x86 Release `repiu_aot_probe` and loader builds passed with only pre-existing C4819/LNK4217 warnings.
- Full probes for both PIU executable variants exited 0.
- New probes `guarded_segment_load_ready/layout/coverage/missing_fallback_rejected/patch/disabled_falls_back/rejected_forms=true`; `selector_guard_all=true`, `coherence_all=true`.
- Matched three-second smokes:
  - off: 504 frames, 48,320 total exceptions, 27,328 breakpoints, 13,878 effective `8E` boundaries.
  - on: 510 frames, 36,397 total exceptions, 15,302 breakpoints, 1,834 effective `8E` boundaries, guarded success/fallback `12,045/1,597`.
- Per frame, total exceptions fell `95.87 -> 71.37` (-25.56%), breakpoints `54.22 -> 30.00` (-44.66%), and `8E` boundaries `27.54 -> 3.60` (-86.94%).
- Both runs ended at the intended timeout without a final exception; Glide direct miss/terminal also remained zero.

### Next verification

Default remains OFF until a manual Music Select capture confirms long-running success/fallback ratio, frame-normalized exceptions, and visual/input behavior. `chdir` remains lower priority per user direction.
