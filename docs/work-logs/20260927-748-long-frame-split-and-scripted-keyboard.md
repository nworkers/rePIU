# Task 748 작업 로그 — 페이싱 아래 "잠깐 멈칫" 조사: 긴 프레임 분해 진단과 스크립트 키보드

설계: [20260927-748](../design/20260927-748-long-frame-split-and-scripted-keyboard.md)
작업 지시: [20260927-748](../work-orders/20260927-748-long-frame-split-and-scripted-keyboard.md)

## 요약

사용자가 보고한 "게임 중 노트와 BGA가 잠깐씩 멈칫"(페이싱 켠 Linux 호스트)은 WSLg의 곡 구간에서는
재현되지 않았습니다. 스크립트 키보드로 곡까지 결정적으로 들어가 페이싱 플레이를 세 번 돌린 결과, 곡
구간(36–50 s)의 긴 프레임은 llvmpipe의 present(8–15 ms)가 쌓인 페이서 마감 빚뿐이고 게스트 구간은 6–18 ms,
MP3 census(20 ms)는 toggle 공백 0·시계 lag 최대 17 ms·starvation은 곡 시작에만 있었습니다. 곡 중 파일
I/O는 0건입니다(MP3 1.47 MB와 BGA 39.DAT의 조각 228 KB를 모두 곡 시작 전에 읽음). 처음에 곡 중 긴 프레임으로
보였던 50.3 s(85 ms)·54.4 s(92 ms)는 입력 없는 플레이의 **실패 연출**이었습니다: `HEYMAN.DAT`(248 KB)·
`GAMEOVER.DAT`(160 KB)·`TITLE.DAT`(760 KB)를 통째로 읽고 Huffman 디코더(`0x010EE71C` 점프 테이블)로 푸는 데
게스트 80–340 ms(약 0.35–0.45 ms/KB)가 듭니다. 사용자의 호스트에서 가를 도구(긴 프레임 분해 로그, 페이서
지터 카운터, 자산 트레이스 `all`)를 넣었고, WSLg에서 wayland 드라이버가 **진짜 vsync**를 받는 경로를
찾았습니다(`XDG_RUNTIME_DIR=/mnt/wslg/runtime-dir`; swap interval 1 applied/effective 1, 페이싱 없음, 60 fps).

## 과정

1. 처음 붙인 긴 프레임 로그(페이서 재동기 시점에만)는 곡 구간에서 50.3 s·54.4 s·56.7 s에 90·99·320 ms를
   두 실행 모두 같은 시각에 보였습니다. 같은 시각의 MP3 census는 정상(feed·toggle·lag 이상 없음).
2. 프레임을 guest/present로 갈랐더니 셋 다 **게스트** 80–90 ms, present 1–2 ms. 100 ms 라이브 프로파일
   (`REPIU_LIVE_PROFILE_INTERVAL_MS=100` — `REPIU_EXECUTION_TIME_PROFILE=1`이 함께 있어야 찍힘)은 그
   창에서 unaccounted(네이티브 게스트) +60 ms, DOS +10 ms, INT 21h +11(AH42 2·AH3F 3), FF /4 경계 +10
   (`0x010EE71C`, Task 219의 Huffman 디코더), 첫 번째에서만 translate +3.
3. 종료 시 DOS I/O 링(64건)이 `HEYMAN.DAT`·`GAMEOVER.DAT`·`TITLE.DAT`의 "4 KB + 나머지 전부 + 꼬리" 읽기를
   보였고, 상한을 푼 자산 트레이스(`all`)가 곡 중 읽기 0건과 곡 시작 전 39.DAT 조각 읽기(4 KB씩, ftell 폴링
   17k회)를 확정했습니다. 즉 세 긴 프레임은 게이지가 비어 실패하는 연출의 BGA 로드이지 사용자의 증상이
   아닙니다(사용자는 입력하며 플레이). `all` 트레이스는 /mnt/e에서 곡 로드를 0.9 s → 7.1 s로 늘립니다.
4. 곡 구간의 나머지 긴 프레임(17–25 ms 초과)은 present 8–15 ms(llvmpipe)가 여러 프레임 쌓인 페이서 빚으로,
   guest 6–18 ms. WSLg 한정입니다.
5. wayland 드라이버 "not available"의 원인은 `$XDG_RUNTIME_DIR`(/run/user/1000)에 `wayland-0`이 없어서였고,
   SDL은 Wayland로 빌드돼 있었습니다(`SDL_VIDEO_DRIVER_WAYLAND 1`, dynamic). `/mnt/wslg/runtime-dir`를
   주면 창이 열리고 `swap interval override requested/value/applied/effective: true/1/true/1`, 페이싱
   비활성, 60.2–60.6 fps.

6. 사용자가 wayland(진짜 vsync)로도 멈칫이 남는다고 확인해 페이서 위상 가설은 기각. 긴 프레임 문턱을
   1.5주기(vblank 하나 놓침)로 낮추고(`REPIU_GLIDE_LONG_FRAME_LOG=1`; 큰 값은 µs 문턱), 곡 중 P2 패드를
   0.5 s마다 누르는 `pumpitea_play_steps.txt`로 사용자와 같은 wayland 설정에서 재현: 게스트가 입력 98회를
   봤고, 곡 구간(36–60 s) 25 ms 초과 프레임 **0**, fps 59.3–60.7, MP3 census 이상 없음(starvation 0,
   toggle 공백 없음). 엔진의 프레임 타이밍에는 멈칫이 없습니다.
7. 같은 wayland 실행이 6회 중 2회는 4 s부터 tick 주입이 **초당 18,000**(PIT 고속 단계에 고착, `last_eip`
   0x0102AB53, AOT boundary 초당 19k)으로 프레임 0장, 13 s에 중첩 ISR 프레임(0x102ab7e/0x24/0x200202
   반복)으로 게스트 스택이 넘쳐 segfault. 스위치 조합(census, 긴 프레임 로그, 입력 스크립트, 예산 62 s)
   으로는 갈리지 않았고 x11에서는 한 번도 없었습니다. Task 736의 단계와 같은 곳이며 별도 과제입니다.

## 검증

| 검증 | 결과 |
|---|---|
| wayland 플레이 62초(곡 중 입력, 진짜 vsync) | 입력 98회, 곡 구간 25 ms 초과 프레임 0, fps 59.3–60.7, MP3 starvation 0, 폴트 0 |
| wayland 시작 재현 6회 | 4회 정상(60 fps), 2회 tick 폭주(초당 18k) 뒤 13 s segfault |
| 페이싱 플레이 60초 × 3 (스크립트 키보드, x11) | 곡 도달 3/3, 폴트 0, 3,146–3,219 frame; 곡 구간 게스트 긴 프레임 0(실패 연출 제외) |
| 100 ms 라이브 프로파일 + 긴 프레임 | 실패 연출 로드: unaccounted +60 ms, DOS +10 ms, INT 21h +11 |
| 자산 트레이스 `all` 플레이 | 곡 중 DAT/AUD 읽기 0건; 곡 시작 전 39.DAT 228 KB·39.AUD 1.47 MB |
| 페이싱 attract 20초 (새 로그) | resyncs 23, late 0, oversleep>1ms 0, max 441 µs, 폴트 0 |
| 비페이싱(`REPIU_GLIDE_SWAP_INTERVAL=0`) attract 12초 | 로그 3줄(시작 로드), pacing active false, 폴트 0 |
| wayland 드라이버(WSLg, runtime-dir 지정) | 창 열림, swap interval 1 effective, 페이싱 없음, 60 fps |
| Linux x64 Release core probe | `core_probe_all=true` |
| Win32 x86 Debug 빌드 | 아래 English 절과 같음 |

로그: `build/task748-play3..5.err.log`, `build/task748-verify-*.err.log`, `build/task748-wayland2.err.log`.

## 남은 것

* 사용자의 멈칫은 엔진 밖입니다. vsync ON(페이싱이든 wayland든)에서만 느껴지고 vsync OFF(58–98 fps
  자유 실행)에서는 매끄러웠으므로, 60 fps로 잠긴 프레임이 WSLg의 Weston→RDP 표시 경로에서 한 장씩
  겹치거나 빠지는 것이 남는 설명입니다(자유 실행은 항상 새 프레임이 있어 덜 보임). WSLg에서는
  `swap_interval = 0`이 가장 매끄럽고, 컴포지터가 프레임을 통째로 올리므로 tearing도 없습니다. 이 결론을
  확정하려면 같은 머신의 Windows 빌드(진짜 vblank)와 비교하면 됩니다.
* wayland 시작 시 6회 중 2회의 tick 폭주(초당 18k, PIT 고속 단계 고착) → 13 s segfault. 비결정적이고
  x11에서는 없음. 별도 과제.
* 실패 연출 BGA 로드 80–340 ms는 원본도 동기 로드라 같은 성격이나, 디코드 0.35–0.45 ms/KB가 AOT로서
  느린지는 미측정(Win32 native 비교 미실시).

---

# English

# Task 748 work log — the "brief hitch" under pacing: a split long-frame log and a scripted keyboard

Design: [20260927-748](../design/20260927-748-long-frame-split-and-scripted-keyboard.md)
Work order: [20260927-748](../work-orders/20260927-748-long-frame-split-and-scripted-keyboard.md)

## Summary

The reported "arrows and BGA hitch briefly during play" (a paced Linux host) did not reproduce inside
the song on WSLg. With the scripted keyboard reaching the song deterministically, three paced plays
showed that the long frames inside the song (36–50 s) are only pacer debt from llvmpipe's 8–15 ms
presents; the guest's share stays at 6–18 ms, the 20 ms MP3 census has no toggle gaps, a clock lag of
at most 17 ms and starvation only at the song's start. There is no file I/O during the song (the 1.47 MB
MP3 and the 228 KB of 39.DAT chunks are read before it starts). The frames that first looked like in-song
stalls, 50.3 s (85 ms) and 54.4 s (92 ms), were the **fail sequence** of an unattended play:
`HEYMAN.DAT` (248 KB), `GAMEOVER.DAT` (160 KB) and `TITLE.DAT` (760 KB) read whole and decoded by the
Huffman decoder (`0x010EE71C` jump table) at 80–340 ms of guest time (about 0.35–0.45 ms/KB). The tools to
tell the cases apart on the user's host are in (split long-frame log, pacer jitter counters, uncapped asset
trace), and the wayland driver was found to take **real vsync** on WSLg (`XDG_RUNTIME_DIR=/mnt/wslg/
runtime-dir`; swap interval 1 applied/effective 1, no pacing, 60 fps).

## Steps

1. The first long-frame log (at pacer resyncs only) showed 90, 99 and 320 ms at 50.3, 54.4 and 56.7 s
   in two runs alike, with a normal MP3 census at those instants.
2. Splitting the frame: all three were **guest** 80–90 ms, present 1–2 ms. The 100 ms live profile
   (`REPIU_LIVE_PROFILE_INTERVAL_MS=100`, which needs `REPIU_EXECUTION_TIME_PROFILE=1`) showed in that
   window unaccounted (native guest) +60 ms, DOS +10 ms, INT 21h +11 (AH42 ×2, AH3F ×3), FF /4
   boundaries +10 at `0x010EE71C` (Task 219's Huffman decoder), translate +3 on the first only.
3. The DOS I/O ring (64 entries) at exit showed the "4 KB + the rest + tail" reads of `HEYMAN.DAT`,
   `GAMEOVER.DAT` and `TITLE.DAT`, and the uncapped asset trace (`all`) confirmed zero reads during the
   song and the 39.DAT chunk reads before it (4 KB at a time, 17k ftell polls). So the three long frames
   are the BGA loads of the gauge-empty fail sequence, not the user's symptom (the user plays with input).
   The `all` trace stretches the song load on /mnt/e from 0.9 s to 7.1 s.
4. The remaining in-song long frames (17–25 ms over) are pacer debt from 8–15 ms presents (llvmpipe)
   accumulated over several frames, guest 6–18 ms. WSLg only.
5. The wayland driver's "not available" was `$XDG_RUNTIME_DIR` (/run/user/1000) lacking `wayland-0`; SDL
   is built with Wayland (`SDL_VIDEO_DRIVER_WAYLAND 1`, dynamic). With `/mnt/wslg/runtime-dir` the window
   opens, `swap interval override requested/value/applied/effective: true/1/true/1`, pacing inactive,
   60.2–60.6 fps.

6. The user confirmed the hitch stays under wayland (real vsync), which drops the pacer-phase
   hypothesis. With the long-frame threshold lowered to 1.5 periods (one missed vblank;
   `REPIU_GLIDE_LONG_FRAME_LOG=1`, a larger value is a µs threshold) and `pumpitea_play_steps.txt`
   pressing P2 pads every 0.5 s through the song, the user's wayland setup reproduced: the guest saw 98
   inputs, **zero** frames over 25 ms inside the song (36–60 s), 59.3–60.7 fps, a clean MP3 census
   (starvation 0, no toggle gaps). The engine's frame timing has no hitch.
7. The same wayland run stalled in 2 of 6 starts: from 4 s the tick injection ran at **18,000 a
   second** (stuck in the PIT fast stage, `last_eip` 0x0102AB53, 19k AOT boundaries a second), no frame
   presented, and at 13 s nested ISR frames (0x102ab7e/0x24/0x200202 repeating) overflowed the guest
   stack into a segfault. No switch combination (census, long-frame log, input script, 62 s budget)
   selects it, and x11 never showed it. It is Task 736's stage; a separate task.

## Verification

| Check | Result |
|---|---|
| wayland 62 s play (in-song input, real vsync) | 98 inputs, no frame over 25 ms inside the song, 59.3–60.7 fps, MP3 starvation 0, no faults |
| wayland start × 6 | 4 normal (60 fps), 2 tick storms (18k/s) then a segfault at 13 s |
| Paced 60 s play × 3 (scripted keyboard, x11) | song reached 3/3, no faults, 3,146–3,219 frames; no guest long frame inside the song (fail sequence aside) |
| 100 ms live profile + long frames | fail-sequence loads: unaccounted +60 ms, DOS +10 ms, INT 21h +11 |
| Asset trace `all` play | zero DAT/AUD reads during the song; 39.DAT 228 KB and 39.AUD 1.47 MB before it |
| Paced 20 s attract (new log) | resyncs 23, late 0, oversleep>1ms 0, max 441 µs, no faults |
| Unpaced (`REPIU_GLIDE_SWAP_INTERVAL=0`) 12 s attract | 3 lines (the start-up loads), pacing active false, no faults |
| wayland driver (WSLg, runtime dir given) | window opens, swap interval 1 effective, no pacing, 60 fps |
| Linux x64 Release core probe | `core_probe_all=true` |
| Win32 x86 Debug build + core probe | build succeeded (0 errors), `core_probe_all=true` |

Logs: `build/task748-play3..5.err.log`, `build/task748-verify-*.err.log`, `build/task748-wayland2.err.log`.

## What remains

* The user's hitch is outside the engine. It is felt only with vsync on (paced or wayland) and not with
  vsync off (58–98 fps free-running), so what remains is a frame locked to 60 fps being duplicated or
  dropped in WSLg's Weston→RDP display path (free-running always has a fresh frame, which hides it).
  On WSLg `swap_interval = 0` is the smoothest, and the compositor presents whole frames, so there is no
  tearing either. The Windows build on the same machine (a real vblank) is the comparison that settles it.
* The wayland start's tick storm in 2 of 6 runs (18k a second, stuck in the PIT fast stage) and the
  segfault at 13 s: nondeterministic, never on x11. A separate task.
* The fail sequence's 80–340 ms BGA loads are synchronous on the original too, but whether 0.35–0.45
  ms/KB of decode is slow for the AOT cache is unmeasured (no Win32 native comparison yet).
