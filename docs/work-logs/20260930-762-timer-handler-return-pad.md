# Task 762 작업 로그: 타이머 핸들러의 복귀를 보는 return pad

설계: `docs/design/20260930-762-timer-handler-return-pad.md`
지시: `docs/work-orders/20260930-762-timer-handler-return-pad.md`

## 한국어

### 요약

Win32에서 pumpit8이 첫 곡을 로딩한 뒤 멈추던 문제를 고쳤습니다. direct 모델(Win32, Linux i386)에서
주입한 타이머 frame이 return pad로 복귀하고, 엔진이 그 복귀를 받아 중단된 코드를 되돌려 보냅니다.
Win32 Release에서 pumpit8은 곡을 재생하고 tick의 99.6%가 전달됩니다(이전 42%).

### 원인을 찾은 순서

1. 사용자 로그(pumpipx3)와 같은 입력 스크립트로 pumpit8을 실행해 멈춤을 재현했습니다. 멈춘 실행은
   `0x0101DB18`의 대기 루프(tick 카운터가 1200이 되길 기다림)를 제자리에서 돌고 있었고 카운터는
   185였습니다.
2. `REPIU_GLIDE_LFB_HIGH_PRECISION=0`으로도, Debug 빌드로도, v0.0.195·v0.0.194(직접 빌드)·v0.0.193
   (GitHub 릴리즈)으로도 같은 자리에서 멈췄습니다. 사용자가 마지막 정상 버전이 v0.0.190이라고
   알려 주어 범위가 v0.0.191 하나로 줄었습니다.
3. v0.0.191의 변경 가운데 Task 736이 주입 frame에서 TF를 지운 것이 원인 구간이었습니다. 현재 코드에
   v0.0.190의 frame 방식을 되살리고 PIC 규칙을 끄면 곡이 재생됐지만(4,650프레임, tick 99.6%), 같은
   조합의 다음 실행은 핸들러 중첩으로 스택이 320 KiB 내려갔습니다. v0.0.190의 방식은 운에 달려
   있었습니다.
4. 주입 로그로 확인한 것: 추적 상태가 핸들러 안으로 이어지면 핸들러 끝부분(`0x0402B712`,
   `0x0402B713`)의 step마다 tick이 다시 주입돼 ESP가 한 쌍에 0x38씩 내려갑니다. TF를 지운 현재 코드는
   마지막 주입(`sti` 지점, `0x040F6D5D`) 뒤로 제자리 코드가 감독 없이 돌아 루프에서 멈춥니다.

### 구현

* `TimerReturnPad`(`include/repiu/engine/timer_return_pad.h`, `src/engine/io/timer_return_pad.cpp`):
  record 스택과 스위치 `REPIU_TIMER_RETURN_PAD`(기본 on).
* `execution_trampoline.cpp`: `TimerReturnPadAddress`(커밋하지 않은 예약 페이지), `HandleTimerReturnPad`,
  `InjectPendingInterrupts`의 frame 작성, `DispatchGuestFault` 맨 앞의 복귀 처리.
* `VehExitSite::kTimerReturnPad`, 요약의 `timer return pad …` 줄.
* probe `timer_return_pad`(core probe와 `repiu_aot_probe --timer-return-pad`).

설계와 달라진 점 하나: 복귀 주소가 제자리 코드일 때 추적과 TF만 세운 첫 구현은 코드 캐시로 돌아가지
못하고 한 step씩 도는 상태에 갇혔습니다(tick 99.5%, 100초에 1프레임). `aot_reentry_pending`을 함께
세워 다음 step에서 `HandleAotReentry`가 캐시 entry를 찾게 했습니다. 설계 문서에 반영했습니다.

### 검증

Win32 x86 Release, 입력 스크립트(`pumpit8_play.txt`, 그 밖은 `survey_generic.txt`), vsync on.

| 실행 | 프레임 | tick 전달 | 비고 |
|---|---|---|---|
| pumpit8 100초, 수정 전 | 2,086 | 9,573 / 22,771 (42%) | 곡 로딩 뒤 멈춤, `AUDIO\720.AUD` 열기 55초 |
| pumpit8 100초, 수정 후 | 5,004 | 22,655 / 22,757 (99.6%) | 곡 재생, 60 fps, `AUDIO\720.AUD` 열기 44초 |
| pumpit8 100초, 수정 후 2회차 | 5,062 | 22,691 / 22,790 (99.6%) | 곡 재생 |
| pumpit8 70초, `REPIU_TIMER_RETURN_PAD=0` | 1,654 | 8,155 / 15,325 (53%) | 멈춤 재현 |
| pumpitea 60초 | 2,500 | 13,439 / 13,512 (99.5%) | 51.9 kHz → 240 Hz 도달 |
| pumpit2a 60초 | 3,125 | 13,869 / 13,909 (99.7%) | |
| pumpitpc 60초 | 3,085 | 13,578 / 13,640 (99.5%) | 51.9 kHz → 240 Hz 도달 |

모든 실행에서 return pad의 pushed와 returned가 같고 overflow·unmatched·abandoned는 0, 최대 깊이는
2였습니다. pumpit8의 VEH dispatch 횟수는 60초 시점에 421만에서 111만으로 줄었습니다(제자리 코드가
캐시로 돌아가 트랩이 줄었습니다).

| 검증 | 결과 |
|---|---|
| Win32 x86 Debug 빌드, core probe | `core_probe_all=true`, `timer_return_pad=true` |
| Win32 x86 Debug `repiu_aot_probe` `--timer-return-pad`, `--pic-timer-in-service`, `--jamma-input-timeline`, `--shutdown-recovery-policy` | 모두 exit 0 |
| Linux x64 Release·Debug, Linux i386 빌드, core probe | 세 구성 모두 `core_probe_all=true` |
| Linux i386 pumpit8 70초(WSL) | 폴트 없음, pad 15,863 / 15,863, tick 15,863 / 15,934 |
| Linux i386 pumpit8 70초, pad 끔 | 폴트 없음, tick 15,669 / 15,985 |
| Linux x64 pumpit8 50초(WSL) | 1,975프레임, pad 0(이 모델은 쓰지 않음) |

### 확인하지 않은 것, 남은 것

* 화면과 소리는 눈과 귀로 확인하지 않았습니다. 수치(프레임, tick, MP3 decode 수)로만 봤습니다.
* 전 롬셋 조사는 하지 않았습니다. Win32에서 실행한 것은 위 네 프로필입니다.
* Linux i386은 WSL에서 수정 전후 모두 4~13 fps입니다. 이 작업과 무관한 기존 상태입니다.
* **Win32 Release의 core probe는 `stack_bridge_contract=false`로 실패합니다.** v0.0.195를 Release로
  빌드한 probe도 같게 실패하므로 이 작업 이전부터 있던 것입니다(Debug는 통과). 고치지 않았습니다.
* **pumpipx3가 함께 살아났습니다.** 사용자가 잘 된다고 알려 주어 같은 Release 바이너리로 30초씩
  돌렸습니다: pad 켬 3회 모두 완주(1,377·1,392·1,381프레임), `REPIU_TIMER_RETURN_PAD=0` 2회 모두 이전과
  같은 `Fatal error: unable to find entry point in DLL.`(96번째 Glide 호출 뒤). 이 작업이 겨냥한 것은
  아니고, 왜 tick 주입 경로가 그 실패를 좌우하는지는 설명하지 못했습니다(사용자 확인 마지막 정상
  v0.0.171, 그 사이 릴리즈 없음). 원인은 별도 조사 대상입니다.
* 실행 로그는 `build/task762/`, 실행 스크립트는 `build/r762.sh`·`build/v762.sh`·`build/l762.sh`
  (저장소 밖)에 있습니다. bisect에 쓴 worktree `build/wt/v0.0.194`, `build/wt/v0.0.195`와 내려받은
  릴리즈 `build/rel/`이 남아 있습니다.

---

## English

### Summary

pumpit8 on Win32 no longer stops after loading its first song. On the direct model (Win32, Linux
i386) an injected timer frame returns to the return pad, and the engine takes that return and sends
the interrupted code on. On Win32 Release pumpit8 plays the song and 99.6% of the ticks are
delivered (42% before).

### How the cause was found

1. The stop was reproduced with pumpit8 and an input script. The stopped run was in the wait loop
   at `0x0101DB18`, in place, waiting for the tick counter to reach 1200 while it stood at 185.
2. It stopped at the same place with `REPIU_GLIDE_LFB_HIGH_PRECISION=0`, in a Debug build, and in
   v0.0.195, v0.0.194 (built) and v0.0.193 (the GitHub release). The user named v0.0.190 as the
   last working version, which left v0.0.191 alone.
3. Of v0.0.191's changes, Task 736's removal of TF from the injected frame was where it began.
   v0.0.190's frame rule restored on the current code, with the PIC rules off, played the song
   (4,650 frames, 99.6% of the ticks), and the next run of the same combination nested handlers
   until the stack had dropped 320 KiB. v0.0.190 worked by luck.
4. The injection log showed it: when the trace state reaches into the handler, a tick goes in at
   every step of the handler's tail (`0x0402B712`, `0x0402B713`) and ESP drops 0x38 a pair. With TF
   removed, the code after the last injection (at `sti`, `0x040F6D5D`) runs in place unsupervised
   and stops in the loop.

### Implementation

`TimerReturnPad` (the record stack and the switch `REPIU_TIMER_RETURN_PAD`, on by default);
in `execution_trampoline.cpp`, `TimerReturnPadAddress` (a reserved, uncommitted page),
`HandleTimerReturnPad`, the frame written by `InjectPendingInterrupts`, and the return taken at the
top of `DispatchGuestFault`; `VehExitSite::kTimerReturnPad` and the summary's `timer return pad …`
line; the probe `timer_return_pad`.

One departure from the design: a first implementation that set only the trace and TF for a return
into code in place never got back into the code cache and ran step by step (99.5% of the ticks, one
frame in 100 seconds). `aot_reentry_pending` is now set as well, so that `HandleAotReentry` finds
the cache entry at the next step. The design document says so.

### Verification

Win32 x86 Release with input scripts, vsync on: the table above. In every run the pad's pushed and
returned counts are equal, overflow, unmatched and abandoned are zero and the greatest depth is 2.
pumpit8's VEH dispatch count at 60 seconds fell from 4.21 million to 1.11 million.

Win32 x86 Debug: `core_probe_all=true` with `timer_return_pad=true`, and `repiu_aot_probe`
`--timer-return-pad`, `--pic-timer-in-service`, `--jamma-input-timeline` and
`--shutdown-recovery-policy` at exit 0. Linux x64 Release and Debug and Linux i386:
`core_probe_all=true`. Linux i386 pumpit8 for 70 seconds under WSL: no fault, 15,863 of 15,863
returns taken, 15,863 of 15,934 ticks; with the pad off, no fault and 15,669 of 15,985. Linux x64
pumpit8 for 50 seconds: 1,975 frames and no use of the pad.

### Not checked, and left

* The picture and the sound were not observed; only the numbers were.
* No survey of every ROM set. The four profiles above are what ran on Win32.
* Linux i386 under WSL runs at 4 to 13 fps before and after; unrelated to this task.
* **The core probe of a Win32 Release build fails with `stack_bridge_contract=false`.** A Release
  build of v0.0.195's probe fails the same way, so it predates this task (Debug passes). Not fixed.
* **pumpipx3 came back with it.** The user reported it running, and the same Release binary was
  run for 30 seconds at a time: three runs with the pad on all completed (1,377, 1,392 and 1,381
  frames), two with `REPIU_TIMER_RETURN_PAD=0` both died as before with `Fatal error: unable to
  find entry point in DLL.` after the 96th Glide call. This task did not aim at it, and why the
  tick injection path decides that failure is not explained (last working version v0.0.171, no
  release in between). The cause is a separate investigation.
* Run logs are in `build/task762/`; the scripts `build/r762.sh`, `build/v762.sh` and
  `build/l762.sh`, the worktrees `build/wt/v0.0.194` and `build/wt/v0.0.195` and the downloaded
  releases in `build/rel/` are outside the repository and remain.
