# Task 749: MP3 재생 시계를 자유 진행으로 — 계단마다 다시 그리던 선이 흔들림이었다

## 한국어

### 배경

사용자: "멈칫거리는 건 MP3 재생 위치 변화를 직접 관찰해야 될 것 같아." Task 748은 프레임 시간과 20 ms
census로는 이상을 못 찾았다. 그래서 게스트가 세는 위치 그 자체를 이벤트 단위로 찍는 트레이스를 넣었다:
호스트 시계가 frame-sync 비트를 토글한 시각과, 게스트가 포트 읽기로 그 새 값을 처음 본 시각.

wayland(진짜 vsync) 플레이의 결과가 원인을 둘로 갈랐다.

| 관측 | 곡 구간 |
|---|---|
| 토글 간격(26.1 ms여야 함) | **16–35 ms**로 흔들림, 16–18과 31–33이 짝을 이룸 |
| 토글이 시계보다 늦은 정도 | 0–2 ms(워커 주기는 문제 아님) |
| 토글마다 lag(장치 count − 시계) | 0 ↔ 17.4 ms 왕복 |
| 게스트 관측 지연 | 0–16 ms 균등(프레임당 한 번 몰아서 읽음, 별개 문제) |

시계는 Task 740의 설계대로 장치가 768프레임(17.4 ms)을 가져갈 때마다 **직전 계단의 count에서 선을 다시
그린다**. 장치의 pull 시각이 조금 이르면 이전 선이 상한에 닿기 전이라 새 선의 시작점(직전 계단 count)이
현재 추정보다 커서 그만큼 **점프**하고, 조금 늦으면 이전 선이 상한에 걸려 **멈춘다**. WSLg PulseAudio의
pull 지터 몇 ms가 12 ms 점프·정지가 되고, 토글 간격이 18–33 ms로 흩어진다. 게스트의 노트 시계는 이
토글을 세므로 노트가 그만큼 뛰거나 멈춘다.

### 설계

`PlaybackPosition`을 자유 진행 시계로 바꾼다.

1. 추정은 **자기 이전 값**에서 PCM 속도로 진행한다(계단마다 다시 그리지 않는다).
2. 목표 = 장치가 가져간 count − 장치 버퍼의 절반(이전 설계의 평균 lag과 같아 노트 오프셋이 그대로).
   오차(목표 − 추정)를 4초 시정수로 속도에 반영하되 ±2%로 묶는다: pull 지터(±5 ms)는 0.1% 남짓의 속도
   흔들림으로 흡수되고, 장치 클록의 실제 속도 차(0.1% 이내)는 계단 없이 따라간다.
3. 가져간 count를 넘지 않고, 뒤로 가지 않는다(이전과 같은 두 불변식). 첫 값은 count가 0이 아닐 때
   목표에서 시작한다.
4. 트레이스 `REPIU_PIU10_MP3_POSITION_TRACE=1`: 토글마다 `[repiu-mp3-pos] toggle seq= t_us= pos_ms=
   frame_ms= lag_ms= queued_ms=`, 게스트가 새 값을 본 읽기마다 `[repiu-mp3-pos] seen seq= t_us= delay_us=
   reads=`. 남겨 둔다.

### 검증 전략

같은 wayland 플레이에서 토글 간격이 26 ± 2 ms로 모이는지, x11 자유 실행·x11 페이싱에서도 같은지, MP3
census(multi, pcm-empty, starvation)가 나빠지지 않는지, core probe(Linux·Win32)를 본다. 노트 오프셋 체감은
사용자 확인.

## English

### Background

The user: "The hitch needs the MP3 playback position itself observed." Task 748 found nothing in the
frame times or the 20 ms census, so a trace of the position the guest counts was added, event by event:
when the host clock toggles the frame-sync bit, and when the guest's port read first sees the new value.

A wayland (real vsync) play split the cause in two.

| Observation | Inside the song |
|---|---|
| Toggle interval (should be 26.1 ms) | **16–35 ms**, 16–18 paired with 31–33 |
| Toggle lateness against the clock | 0–2 ms (the worker's cadence is not it) |
| Lag at each toggle (device count − clock) | swinging 0 ↔ 17.4 ms |
| Guest observation delay | uniform 0–16 ms (one burst of reads per frame; a separate matter) |

Task 740's clock **restarts its line at the previous step's count** every time the device takes 768
frames (17.4 ms). A pull that comes slightly early arrives before the previous line reached its cap, so
the new line's origin is above the current estimate and the clock **jumps** by the difference; one that
comes late leaves the previous line pinned at its cap, so the clock **stalls**. A few ms of pull jitter
in WSLg's PulseAudio become 12 ms jumps and stalls, toggles scatter to 18–33 ms apart, and the guest's
arrow clock, which counts those toggles, jumps and stalls with them.

### Design

`PlaybackPosition` becomes a free-running clock.

1. The estimate advances at the PCM rate **from its own previous value**; no restart per step.
2. Target = the device's pulled count less half a device buffer (the previous design's average lag, so
   the arrow offset is unchanged). The error (target − estimate) feeds the rate with a four second time
   constant, bounded to ±2%: pull jitter (±5 ms) becomes a rate wobble of about 0.1%, and a real device
   rate difference (under 0.1%) is followed without a step.
3. Never past the pulled count, never backwards (the same two invariants). The first value starts at the
   target once the count is non-zero.
4. The trace `REPIU_PIU10_MP3_POSITION_TRACE=1` stays: `[repiu-mp3-pos] toggle seq= t_us= pos_ms=
   frame_ms= lag_ms= queued_ms=` per toggle and `[repiu-mp3-pos] seen seq= t_us= delay_us= reads=` per
   guest read that first sees the new value.

### Verification strategy

The same wayland play with toggle intervals gathered at 26 ± 2 ms, the same under x11 free-running and
x11 pacing, an MP3 census no worse (multi, pcm-empty, starvation), the core probes (Linux, Win32); the
arrow offset by the user's ear.
