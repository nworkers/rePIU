# Task 749 작업 로그 — MP3 재생 시계를 자유 진행으로

설계: [20260928-749](../design/20260928-749-free-running-mp3-playback-clock.md)
작업 지시: [20260928-749](../work-orders/20260928-749-free-running-mp3-playback-clock.md)

## 요약

게스트가 세는 MP3 위치를 이벤트 단위로 찍자(토글 시각·게스트 관측 시각) 두 가지가 드러났습니다.
(1) 호스트 시계의 토글 간격이 26.1 ms가 아니라 **16–35 ms**로 흔들렸습니다. Task 740의 시계가 장치
계단(768프레임)마다 선을 직전 계단에서 다시 그려서, 장치 pull 시각의 몇 ms 지터가 12 ms 점프·정지가 되고
있었습니다. 시계를 자유 진행(자기 이전 값에서 PCM 속도로 진행, 목표까지 4초 시정수 슬루 ±2%)으로 바꾸자
토글 간격이 **24–27 ms(98%)** 로 모였습니다. (2) 60 Hz 대기(페이싱·진짜 vsync)에서는 게스트가 토글을
0–16 ms 균등 지연으로, 프레임당 한 번 몰아서(5회 읽기) 관측해 위치가 **16/33 ms 걸음**으로 진행합니다.
자유 실행(x11, vsync off)에서는 지연 0–7 ms, 걸음 20–30 ms입니다. 이것이 사용자가 vsync ON에서만 느끼는
멈칫의 남은 기전이며, 게이트 대기 중 타이머 ISR을 돌려야 풀립니다(별도 과제).

## 과정

1. 트레이스 `REPIU_PIU10_MP3_POSITION_TRACE=1`. 토글(워커)과 게스트의 첫 관측(status 포트 읽기)에
   마이크로초 스탬프. wayland 플레이(곡 중 입력)에서 곡 구간 1,018 토글.
2. 토글 간격 히스토그램 16:2 17:2 18:43 … 25:193 26:54 27:202 … 33:63. 토글이 시계보다 늦은 정도는
   0–2 ms(워커는 2 ms 주기로 돈다)라 워커가 아니라 시계 자체가 흔들린 것. 토글마다 lag(장치 count −
   시계)가 0 ↔ 17.4 ms를 오갔습니다: 계단 관측 시 "직전 계단 count"에서 다시 시작하는 선은, 이전 선이
   상한에 못 미쳤으면 점프, 이미 걸렸으면 정지.
3. 자유 진행 시계로 교체. 목표 = 가져간 count − 장치 버퍼 절반(이전 평균 lag과 동일). 오차/(4 s × 속도)를
   속도에 더하되 ±2%로 제한. 상한·역행 금지 불변식 유지.
4. 같은 wayland 플레이: 토글 간격 24:297 25:195 26:705 27:126(그 밖 22건). x11 자유 실행: 24–27에 756/776,
   관측 지연 0–7 ms, 관측 간격 20–30 ms. x11 페이싱: 24–27에 994/1,016, 관측 지연 0–15 ms 균등, 관측 간격
   16:344 / 33:468. 곡 구간 이상(토글 공백 > 40 ms, 관측 공백 > 40 ms) 0건.

## 검증

| 검증 | 결과 |
|---|---|
| wayland 플레이 62초(곡 중 입력) 수정 전 → 후 | 토글 간격 16–35 ms 산포 → 24–27 ms 98%; multi 1 → 0, pcm-empty 0, starvation 8 → 6 |
| x11 vsync off 플레이 62초 | 토글 24–27 ms 97%, 관측 지연 ≤ 7 ms, multi 0, 폴트 0 |
| x11 페이싱 플레이 62초 | 토글 24–27 ms 98%, multi 1(곡 시작), pcm-empty 0, 폴트 0 |
| Linux x64 Release core probe | 아래 English 절과 같음 |
| Win32 x86 Debug 빌드 + core probe | 아래 English 절과 같음 |

로그: `build/task748-waypos-a.err.log`(수정 전), `build/task748-waypos-b.err.log`, `build/task749-x11*.err.log`.

## 남은 것

* **게이트 대기 중 타이머 ISR**: 60 Hz 대기에서 게스트는 프레임당 한 번만 status를 읽으므로 토글 관측이
  0–16 ms 늦고 위치가 16/33 ms 걸음이 됩니다. 실제 기계는 vblank 대기 중에도 ISR이 돌아 4 ms 안에 봅니다.
  제안: swap 게이트가 기다려야 할 때 자지 않고 EIP를 게이트 자리에 둔 채 밀린 tick을 주입해 게스트로
  돌아가고(ISR의 `iret`가 게이트 자리로 돌아와 다시 HLE), present가 끝났을 때 정상 반환 — 기존 주입
  기계만으로 가능. 별도 과제.
* 노트 오프셋(목표 = count − 버퍼 절반)은 이전 평균과 같게 뒀지만 체감은 사용자 확인.

---

# English

# Task 749 work log — a free-running MP3 playback clock

Design: [20260928-749](../design/20260928-749-free-running-mp3-playback-clock.md)
Work order: [20260928-749](../work-orders/20260928-749-free-running-mp3-playback-clock.md)

## Summary

Tracing the MP3 position the guest counts, event by event (toggle time, guest observation time),
showed two things. (1) The host clock's toggle intervals were **16–35 ms**, not 26.1 ms: Task 740's
clock restarted its line at the previous device step every 768 frames, so a few ms of pull jitter became
12 ms jumps and stalls. Made free-running (advancing from its own previous value at the PCM rate, slewing
to the target with a four second time constant, ±2%), the intervals gather at **24–27 ms (98%)**. (2)
Under any 60 Hz wait (pacing or real vsync) the guest observes a toggle with a uniform 0–16 ms delay,
in one burst of five reads per frame, so its position advances in **16/33 ms steps**; free-running (x11,
vsync off) observes within 0–7 ms with 20–30 ms steps. That is the remaining mechanism of the hitch the
user feels only with vsync on, and it needs the timer ISR to run during the gate wait (a separate task).

## Steps

1. The trace `REPIU_PIU10_MP3_POSITION_TRACE=1`: microsecond stamps at the toggle (worker) and at the
   guest's first observation (the status port read). A wayland play with in-song input: 1,018 toggles
   inside the song.
2. Toggle interval histogram 16:2 17:2 18:43 … 25:193 26:54 27:202 … 33:63. Toggle lateness against the
   clock 0–2 ms (the worker runs every 2 ms), so the clock itself wobbled: the lag at each toggle (device
   count − clock) swung 0 ↔ 17.4 ms. A line restarted at "the previous step's count" jumps when the
   previous line had not reached its cap and stalls when it had.
3. The free-running clock. Target = pulled count − half a device buffer (the previous average lag).
   error/(4 s × rate) is added to the rate, bounded to ±2%. The cap and no-reverse invariants stay.
4. The same wayland play: 24:297 25:195 26:705 27:126 (22 others). x11 free-running: 756/776 within
   24–27, observation delay 0–7 ms, observation intervals 20–30 ms. x11 paced: 994/1,016 within 24–27,
   observation delay uniform 0–15 ms, observation intervals 16:344 / 33:468. No in-song anomaly (toggle
   gap > 40 ms, observation gap > 40 ms).

## Verification

| Check | Result |
|---|---|
| wayland 62 s play (in-song input), before → after | toggle intervals scattered 16–35 ms → 24–27 ms 98%; multi 1 → 0, pcm-empty 0, starvation 8 → 6 |
| x11 vsync-off 62 s play | toggles 24–27 ms 97%, observation delay ≤ 7 ms, multi 0, no faults |
| x11 paced 62 s play | toggles 24–27 ms 98%, multi 1 (song start), pcm-empty 0, no faults |
| Linux x64 Release core probe | `core_probe_all=true` |
| Win32 x86 Debug build + core probe | build succeeded, `core_probe_all=true` |

Logs: `build/task748-waypos-a.err.log` (before), `build/task748-waypos-b.err.log`, `build/task749-x11*.err.log`.

## What remains

* **The timer ISR during a gate wait**: under a 60 Hz wait the guest reads the status once per frame,
  so a toggle is seen 0–16 ms late and the position steps 16/33 ms. The real machine's ISR runs through
  the vblank wait and sees it within 4 ms. Proposal: when the swap gate must wait, do not sleep; leave
  EIP at the gate site, inject the owed tick and return to the guest (the ISR's `iret` lands on the gate
  site and re-enters the HLE), and return normally once the present is done — the existing injection
  machinery suffices. A separate task.
* The arrow offset (target = count − half a buffer) matches the previous average; the user's ear decides.
