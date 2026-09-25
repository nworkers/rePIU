# Task 734 작업 로그 — Linux x64 return thunk의 guest EFLAGS 복원

설계: [20260926-734](../design/20260926-734-linux-x64-return-thunk-flags.md)
작업 지시: [20260926-734](../work-orders/20260926-734-linux-x64-return-thunk-flags.md)

## 요약

WSL Linux x64에서 `pumpit2a`가 곡 선택 화면에서 멈추던 원인을 찾아 고쳤습니다.
`RepiuLinuxX64ReturnThunk`가 guest의 `ret`과 간접 call/jump를 처리한 뒤 **guest EFLAGS를
복원하지 않아**, 이 thunk를 지나는 모든 `ret`이 CF=0으로 돌아왔습니다. Watcom C 런타임의
`sin` helper는 결과를 CF로 반환하므로 호출자가 `fsin`을 영원히 재시도했습니다. thunk가
`jmp` 직전에 frame의 EFLAGS를 복원하도록 고친 뒤, 같은 자동 입력으로 게임이 곡 선택을 지나
곡을 고르고 step 데이터를 읽어 플레이에 들어갑니다.

| | 수정 전 (자동 재현 3회, 사용자 실행 4회) | 수정 후 (자동 재현) |
|---|---|---|
| MSCDEX 요청 | 17 (모두) | 1,850 (재생 중 Q-channel 위치 질의) |
| DOS open | 16, 마지막 `MD_SEL2K.RES` | 23, `STEP/TM_HD_1.STF`, `TELLME.RES` |
| `progress` | 118에서 정지 | 9,176까지 증가 |
| 프레임 표시 | 정지 시점부터 끊김 | 실행 끝까지 (56 fps) |
| CD 재생 명령 | 트랙 3·4 미리듣기 뒤 끝 | 미리듣기 2곡 뒤 선택곡 LBA 152330 (6,926 frame) 재생 |

## 1단계 — 사용자 로그로 범위 좁히기

사용자 실행 4회(`build/stall.err.log`, `stall2.err.log`, 덮어쓴 `stall3.err.log` 두 번)는 모두
MSCDEX 요청 17개, 마지막 open `MD_SEL2K.RES`, `progress=118`에서 멈췄습니다. 정지 뒤의
게스트 카운터는 주입된 타이머 tick 하나당 정확히 dispatch 3, AOT boundary 1, AOT reentry 10,
JAMMA 스캔 2였습니다(run2·run3 모두 초당 711/237/2370).

이 단계에서 반증한 가설 두 개:

* **"입력을 읽고 있으니 메인 루프가 돈다"** — 아닙니다. `jamma_scan_count`(게스트 `IN` 명령
  수)가 런 전체에서 주입 tick의 정확히 2배이고, 포트 I/O 총량도 tick당 6.06개로 일정합니다.
  이 게임은 모든 포트 I/O를 240 Hz 타이머 ISR 안에서 합니다.
* **"미리듣기가 무음인 건 pregap 매핑 오류"** — 아닙니다. `repiu_chd_cd_probe --scan`으로
  보면 트랙 3은 `data_start=2804 pregap=148 start=2952`이고 첫 가청 프레임이 보고한 시작의
  +22 frame, 트랙 4는 +4 frame입니다. 게임이 요청한 LBA는 실제 오디오 시작이 맞습니다.

Glide gate와 포트 I/O가 direct dispatch라 기존 카운터로는 메인 루프가 계속 그리는지 가를 수
없었고, `_GRBUFFERSWAP` 총계로 푸는 산술도 수렴하지 않았습니다(런마다 attract 화면 대기
시간이 달라 정지 전 프레임 수가 일정하지 않음). 그래서 시간축 계측을 묶은
`scripts/task734_songselect_stall_capture.sh`를 추가했습니다.

## 2단계 — 자동 재현

사용자 실행을 기다리는 대신 WSLg에서 직접 재현했습니다. `SDL_VIDEO_DRIVER=x11`로 XWayland
창을 만들고, `libXtst`의 `XTestFakeKeyEvent`로 키를 누르는 작은 도구(scratch, 저장소 밖)를
만들었습니다. 사용자 로그의 시간표(SERVICE 5회 → P2 center 2회 → P2 up-right 2회)를 따르면
곡 선택에 도달하고 정지가 재현됩니다. 게스트가 합성 키를 읽는다는 것은 `[repiu-input]`
전이로 확인했습니다.

첫 자동 재현(`task734-auto2`)의 결과:

* `[repiu-frame-rate]` 줄이 정지 직후 끊겼습니다(마지막 `fps=15.3 frames=2549`). **메인
  루프가 멈췄다**는 뜻입니다.
* 회수 지점 `0x20253CB1`, `REPIU_AOT_CACHE_MAP_TRACE=0x20253CAD` →
  `guest=0x010F20BA previous_guest=0x010F20B8`. Watcom `sin`의 `fsin` 재시도 루프입니다.
  사용자 실행 네 번의 회수 지점(`0x20253C46`, `0x20253CAD`)도 같은 블록입니다.
* 같은 실행에서 20 ms 간격 guest position census는 표본을 하나도 얻지 못했습니다(capture
  실패 4,180). 사용자 실행의 50 ms에서는 성공했으므로 간격 문제로 보이며, 이번 결론에는
  쓰지 않았습니다.

## 3단계 — 원인 특정

`REPIU_AOT_GUEST_MAP_TRACE`로 루프와 helper의 모든 명령(guest `0x010F20AE`~`0x010F20EC`)을
번역 바이트까지 떴습니다(`task734-auto3`).

* 역방향 `jae`(0x010F20BF)는 타이머 safe point로 번역되어 `pushfq; cmp [0x1E000000],0; jne
  → popfq; int3; … popfq; jae`입니다. flags를 보존합니다.
* `clc`는 그대로, `pop`은 `mov` + `lea`로 flags를 건드리지 않습니다.
* `ret`은 `mov r14d,[r15]; lea r15d,[r15+4]; movabs r12, 0x402D951C; …; jmp r12`이고,
  `0x402D951C`는 `RepiuLinuxX64ReturnThunk`입니다.

thunk를 디스어셈블하면 진입 시 `pushfq; pop rax; mov [r11+0x24],eax`로 flags를 저장하지만,
복귀 경로는 GPR만 되돌리고 `test r10,r10; je …; jmp r10`으로 끝납니다. `test`가 CF를 0으로
만들므로 helper가 CF=1("끝")을 반환해도 호출자의 `jae`가 항상 성립합니다. 같은 파일의
`RepiuLinuxX64LegacyResumeThunk`만 `[r11+36]`에서 flags를 복원하고 있었습니다.

32-bit bridge(`stack_bridge.inc.S`, Win32 naked thunk)는 `pushf`/`pusha` … `popa`/`popf`,
Glide gate thunk는 `pushfq`/`popfq`로 복원하므로 결함은 x64 return thunk 하나입니다.
reduction 상수 `DS:0x96398`도 확인했습니다. object 4(file `0x10E600`)의 해당 위치
`0x1A4998`에 80-bit 2π가 그대로 있습니다.

## 4단계 — 수정

`src/platform/linux/aot_dbt_return_thunk_x64.S`: `test r10, r10` / `jz` 뒤, `jmp r10` 직전에
`mov r11d, dword ptr [r11 + 36]; push r11; popfq`를 넣었습니다. frame 값을 쓰는 이유와 `r11`을
쓰는 근거는 설계에 있습니다.

## 5단계 — 검증

| 검증 | 결과 |
|---|---|
| core probe `guest_return_preserves_flags`, 수정 전 thunk | `observed=0x0 expected=0x101 MISMATCH`, core probe 실패 1 |
| 같은 probe, 수정 후 | `0x101`, `linux_x64_guest_register_all=true` |
| Linux x64 core probe 전체 | 31/31, 실패 0 |
| 자동 재현, 수정 후 (`task734-fix1`) | 위 요약 표. 곡 선택 → 미리듣기 LBA 252458·267690 → 선택곡 152330 재생, `STEP/TM_HD_1.STF` 로드 |
| 수정된 binary 확인 | `objdump`에서 `test r10,r10` 뒤 `mov r11d,[r11+0x24]; push r11; popf; jmp r10` |

Win32 빌드는 하지 않았습니다. 바뀐 파일은 Linux x64 전용 assembly와 Linux x64 전용 probe뿐입니다.

## CD 미리듣기 무음 — 이 환경에서는 재현되지 않음

사용자는 "미리듣기 음악이 처음부터 안 들렸다"고 했습니다. libpulse로 WSLg PulseServer의
sink-input 목록과 기본 sink monitor 레벨(100 ms RMS)을 기록하는 도구를 만들어 수정 전후 각
1회 측정했습니다.

* repiu의 오디오는 SDL3가 섞어 **PulseAudio stream 하나**로 나갑니다. 실행 내내 corked=0,
  mute=0, volume 100%였습니다.
* CD 재생 구간(수정 후: 18.97→21.74 s, 23.78→28.75 s)에 출력은 RMS 약 10,000, peak 32,768로
  연속적입니다. 그 앞의 YMZ how-to-play 음악은 RMS 3,000~4,000입니다. 수정 전 binary도 재생
  구간에서 RMS 약 11,000~12,000이었고, 정지 뒤로는 완전히 무음이었습니다(게임이 `sin`에 갇혀
  배경음도 없음).

즉 이 머신의 WSLg 출력 단계까지는 CD-DA가 수정 전후 모두 들어갑니다. 사용자 쪽 무음의 원인은
확인하지 못했습니다. monitor 이후(Windows 쪽 RDP 오디오 경로)는 측정하지 않았고, YMZ와 CD가
같은 stream으로 섞여 나가므로 그 구간에서 CD만 사라질 이유는 아직 찾지 못했습니다.

## 남은 것

1. CD 미리듣기 무음(위). 수정 후 binary로 사용자 청취 확인이 필요합니다.
2. `repiu_instruction_census` 도구는 Linux x64에서 GL 심볼 링크 오류로 빌드되지 않습니다.
   `build/linux_x64_debug`에 이 binary가 만들어진 적이 없으며, 이번 변경과는 무관합니다.
3. 합성 입력 도구(XTest)와 PulseAudio 측정 도구는 scratch로만 만들었습니다. 대화형 상태를
   자동 재현하는 데 효과적이었으므로 저장소 도구로 들일지는 별도 결정입니다.
4. 이 결함은 `ret`과 간접 call/jump 뒤에 flags를 읽는 모든 guest 코드에 영향을 줬습니다.
   Linux x64에서 이전에 관찰된 다른 이상 동작을 이 수정 뒤에 다시 볼 가치가 있습니다.

---

# English

# Task 734 work log — restoring guest EFLAGS in the Linux x64 return thunk

Design: [20260926-734](../design/20260926-734-linux-x64-return-thunk-flags.md)
Work order: [20260926-734](../work-orders/20260926-734-linux-x64-return-thunk-flags.md)

## Summary

The freeze of `pumpit2a` at the song-select screen on WSL Linux x64 was attributed and fixed.
`RepiuLinuxX64ReturnThunk` **did not restore guest EFLAGS** after handling a guest `ret` or an indirect
call/jump, so every `ret` through the thunk came back with CF=0. The Watcom C runtime's `sin` helper
returns its answer in CF, and its caller retried `fsin` forever. With the thunk restoring the frame's
EFLAGS right before its `jmp`, the same automated input takes the game past song select into choosing a
song, loading its step data and playing it.

| | Before (3 automated, 4 user runs) | After (automated) |
|---|---|---|
| MSCDEX requests | 17 (every run) | 1,850 (Q-channel position queries during play) |
| DOS opens | 16, last `MD_SEL2K.RES` | 23, `STEP/TM_HD_1.STF`, `TELLME.RES` |
| `progress` | stuck at 118 | rises to 9,176 |
| Presented frames | stop at the freeze | continue to the end (56 fps) |
| CD play commands | end after the track 3/4 previews | two previews, then the chosen song at LBA 152330 (6,926 frames) |

## Stage 1 — narrowing it from the user's logs

All four user runs stopped with 17 MSCDEX requests, last open `MD_SEL2K.RES`, and `progress=118`.
After the freeze, the guest counters were exactly 3 dispatches, 1 AOT boundary, 10 AOT reentries and 2
JAMMA scans per injected timer tick (711/237/2370 per second in both run 2 and run 3).

Two hypotheses were refuted here:

* **"It reads input, so the main loop runs"** — no. `jamma_scan_count` (guest `IN` instructions) is
  exactly twice the injected ticks across the whole run, and total port I/O is a constant 6.06 per tick.
  This game does all of its port I/O inside the 240 Hz timer ISR.
* **"The silent preview is a pregap mapping error"** — no. `repiu_chd_cd_probe --scan` shows track 3
  as `data_start=2804 pregap=148 start=2952`, with the first audible frame 22 frames after the reported
  start (track 4: 4 frames). The LBAs the game requested are the real audio starts.

Because the Glide gate and port I/O use direct dispatch, the existing counters could not tell whether
the main loop kept drawing, and arithmetic on the `_GRBUFFERSWAP` totals did not converge (the time spent
on the attract screen differs per run, so the pre-freeze frame count is not constant). So
`scripts/task734_songselect_stall_capture.sh` was added to bundle the time-resolved instruments.

## Stage 2 — automated reproduction

Instead of waiting for another user run, the freeze was reproduced here on WSLg. `SDL_VIDEO_DRIVER=x11`
gives an XWayland window, and a small tool calling `libXtst`'s `XTestFakeKeyEvent` (scratch, outside the
repository) presses keys. Following the user's timeline (SERVICE ×5, P2 center ×2, P2 up-right ×2)
reaches song select and reproduces the freeze; `[repiu-input]` transitions confirm the guest read the
synthetic keys.

The first automated run (`task734-auto2`):

* `[repiu-frame-rate]` lines stopped right after the freeze (last `fps=15.3 frames=2549`): **the main
  loop stopped**.
* Recovery point `0x20253CB1`; `REPIU_AOT_CACHE_MAP_TRACE=0x20253CAD` gives `guest=0x010F20BA
  previous_guest=0x010F20B8`, the `fsin` retry loop of Watcom `sin`. The four user runs' recovery points
  (`0x20253C46`, `0x20253CAD`) are in the same block.
* In the same run a 20 ms guest position census got no samples (4,180 capture failures). The user runs
  succeeded at 50 ms, so this looks like the interval; it was not used for the conclusion.

## Stage 3 — the cause

`REPIU_AOT_GUEST_MAP_TRACE` dumped every instruction of the loop and helper (guest `0x010F20AE` to
`0x010F20EC`) down to its translated bytes (`task734-auto3`):

* The backward `jae` (0x010F20BF) is translated as a timer safe point, `pushfq; cmp [0x1E000000],0;
  jne → popfq; int3; … popfq; jae`, and preserves flags.
* `clc` is copied, and `pop` becomes `mov` + `lea`, which leave flags alone.
* `ret` is `mov r14d,[r15]; lea r15d,[r15+4]; movabs r12, 0x402D951C; …; jmp r12`, and
  `0x402D951C` is `RepiuLinuxX64ReturnThunk`.

Disassembled, the thunk saves flags on entry (`pushfq; pop rax; mov [r11+0x24],eax`), but its return
path restores only the GPRs and ends in `test r10,r10; je …; jmp r10`. `test` clears CF, so the caller's
`jae` is always taken even when the helper returned CF=1 ("done"). Only
`RepiuLinuxX64LegacyResumeThunk`, in the same file, restored flags from `[r11+36]`.

The 32-bit bridges (`stack_bridge.inc.S`, the Win32 naked thunks) use `pushf`/`pusha` … `popa`/`popf`
and the Glide gate thunk uses `pushfq`/`popfq`, so the x64 return thunk is the only defective one. The
reduction constant `DS:0x96398` was checked as well: object 4 (file `0x10E600`) holds the exact 80-bit
2π at `0x1A4998`.

## Stage 4 — the fix

`src/platform/linux/aot_dbt_return_thunk_x64.S`: after `test r10, r10` / `jz` and right before
`jmp r10`, `mov r11d, dword ptr [r11 + 36]; push r11; popfq`. The design gives the reasons for using the
frame value and `r11`.

## Stage 5 — verification

| Check | Result |
|---|---|
| core probe `guest_return_preserves_flags`, old thunk | `observed=0x0 expected=0x101 MISMATCH`, one core probe failure |
| Same probe, fixed | `0x101`, `linux_x64_guest_register_all=true` |
| Whole Linux x64 core probe | 31/31, zero failures |
| Automated reproduction, fixed (`task734-fix1`) | Summary table above: song select → previews at LBA 252458 and 267690 → chosen song at 152330, `STEP/TM_HD_1.STF` loaded |
| Fixed binary | `objdump` shows `mov r11d,[r11+0x24]; push r11; popf; jmp r10` after `test r10,r10` |

No Win32 build was run: the changed files are Linux x64-only assembly and a Linux x64-only probe.

## Silent CD preview — not reproduced here

The user reported the preview music was never audible. A libpulse tool recorded the WSLg PulseServer's
sink-inputs and the default sink monitor's level (RMS per 100 ms), once before and once after the fix.

* repiu's audio leaves as **one PulseAudio stream** mixed by SDL3; it stayed corked=0, mute=0, volume
  100% throughout.
* During CD play (after the fix: 18.97→21.74 s and 23.78→28.75 s) the output is continuous at RMS about
  10,000 with peaks of 32,768; the YMZ how-to-play music before it sits at RMS 3,000-4,000. The pre-fix
  binary also reached RMS 11,000-12,000 while playing, and was fully silent after the freeze (the game
  stuck in `sin`, no background music either).

So on this machine CD-DA reaches the WSLg output stage both before and after the fix. The cause of the
user's silence was not found: nothing after the monitor (the Windows-side RDP audio path) was measured,
and since YMZ and CD leave in one mixed stream, no reason for CD alone to vanish there has been found.

## What remains

1. The silent CD preview (above). A listening check with the fixed binary is needed.
2. The `repiu_instruction_census` tool does not link on Linux x64 (unresolved GL symbols). It has never
   been produced in `build/linux_x64_debug` and is unrelated to this change.
3. The XTest input tool and the PulseAudio measurement tool exist only as scratch. They were effective
   at reproducing interactive states; bringing them into the repository is a separate decision.
4. The defect affected all guest code that reads flags after a `ret` or an indirect call/jump. Other
   anomalies previously seen on Linux x64 are worth re-checking after this fix.
